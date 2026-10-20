/* OpenLua - JSON module (OneLua-compatible).
 *
 * Pure parser/serializer plus bounded file helpers over the portable FS
 * core.  No PSP headers, so the whole module is host-testable.
 */

#include "engine/data/json.h"
#include "engine/fs/olfs.h"
#include "engine/core/olreg.h"

#include <lauxlib.h>
#include <lualib.h>

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSON_MAX_FILE   (256 * 1024)
#define JSON_MAX_DEPTH  128
#define JSON_MAX_NUMBER 64

/* ------------------------------------------------------------------ */
/* Growable byte buffer                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    char  *ptr;
    size_t len;
    size_t cap;
} JsonBuf;

static int buf_reserve(JsonBuf *b, size_t extra)
{
    size_t need;

    if (extra > (size_t)-1 - b->len - 1)
        return -1;
    need = b->len + extra + 1;
    if (need <= b->cap)
        return 0;

    size_t cap = b->cap ? b->cap : 64;
    while (cap < need) {
        if (cap > (size_t)-1 / 2) {
            cap = need;
            break;
        }
        cap *= 2;
    }

    char *p = realloc(b->ptr, cap);
    if (!p)
        return -1;
    b->ptr = p;
    b->cap = cap;
    return 0;
}

static int buf_putc(JsonBuf *b, char c)
{
    if (buf_reserve(b, 1) != 0)
        return -1;
    b->ptr[b->len++] = c;
    b->ptr[b->len] = 0;
    return 0;
}

static int buf_put(JsonBuf *b, const char *s, size_t n)
{
    if (n == 0)
        return 0;
    if (buf_reserve(b, n) != 0)
        return -1;
    memcpy(b->ptr + b->len, s, n);
    b->len += n;
    b->ptr[b->len] = 0;
    return 0;
}

static void buf_free(JsonBuf *b)
{
    free(b->ptr);
    b->ptr = NULL;
    b->len = b->cap = 0;
}

/* ------------------------------------------------------------------ */
/* Parser                                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    const unsigned char *start;
    const unsigned char *p;
    const unsigned char *end;
    const char          *err;
} JsonParser;

static void parse_error(JsonParser *P, const char *msg)
{
    if (!P->err)
        P->err = msg;
}

static void skip_ws(JsonParser *P)
{
    while (P->p < P->end) {
        unsigned char c = *P->p;
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
            break;
        P->p++;
    }
}

static int hex_value(unsigned char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int parse_hex4(JsonParser *P, unsigned *out)
{
    unsigned value = 0;
    int i;

    if ((size_t)(P->end - P->p) < 4) {
        parse_error(P, "incomplete \\u escape");
        return -1;
    }
    for (i = 0; i < 4; i++) {
        int d = hex_value(P->p[i]);
        if (d < 0) {
            parse_error(P, "invalid \\u escape");
            return -1;
        }
        value = (value << 4) | (unsigned)d;
    }
    P->p += 4;
    *out = value;
    return 0;
}

static int buf_utf8(JsonBuf *b, unsigned cp)
{
    unsigned char out[4];
    size_t n;

    if (cp <= 0x7F) {
        out[0] = (unsigned char)cp;
        n = 1;
    } else if (cp <= 0x7FF) {
        out[0] = (unsigned char)(0xC0 | (cp >> 6));
        out[1] = (unsigned char)(0x80 | (cp & 0x3F));
        n = 2;
    } else if (cp <= 0xFFFF) {
        out[0] = (unsigned char)(0xE0 | (cp >> 12));
        out[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (unsigned char)(0x80 | (cp & 0x3F));
        n = 3;
    } else if (cp <= 0x10FFFF) {
        out[0] = (unsigned char)(0xF0 | (cp >> 18));
        out[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (unsigned char)(0x80 | (cp & 0x3F));
        n = 4;
    } else {
        return -1;
    }
    return buf_put(b, (const char *)out, n);
}

static int parse_string(JsonParser *P, JsonBuf *out)
{
    if (P->p >= P->end || *P->p != '"') {
        parse_error(P, "expected string");
        return -1;
    }
    P->p++;

    while (P->p < P->end) {
        unsigned char c = *P->p++;

        if (c == '"')
            return 0;
        if (c < 0x20) {
            parse_error(P, "control character in string");
            return -1;
        }
        if (c != '\\') {
            if (buf_putc(out, (char)c) != 0) {
                parse_error(P, "out of memory");
                return -1;
            }
            continue;
        }

        if (P->p >= P->end) {
            parse_error(P, "incomplete escape");
            return -1;
        }
        c = *P->p++;
        switch (c) {
        case '"':  c = '"';  break;
        case '\\': c = '\\'; break;
        case '/':  c = '/';  break;
        case 'b':  c = '\b'; break;
        case 'f':  c = '\f'; break;
        case 'n':  c = '\n'; break;
        case 'r':  c = '\r'; break;
        case 't':  c = '\t'; break;
        case 'u': {
            unsigned cp, lo;
            if (parse_hex4(P, &cp) != 0)
                return -1;
            if (cp >= 0xD800 && cp <= 0xDBFF) {
                if ((size_t)(P->end - P->p) < 2 ||
                    P->p[0] != '\\' || P->p[1] != 'u') {
                    parse_error(P, "missing low surrogate");
                    return -1;
                }
                P->p += 2;
                if (parse_hex4(P, &lo) != 0)
                    return -1;
                if (lo < 0xDC00 || lo > 0xDFFF) {
                    parse_error(P, "invalid low surrogate");
                    return -1;
                }
                cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
            } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                parse_error(P, "unexpected low surrogate");
                return -1;
            }
            if (buf_utf8(out, cp) != 0) {
                parse_error(P, "invalid unicode escape");
                return -1;
            }
            continue;
        }
        default:
            parse_error(P, "invalid escape");
            return -1;
        }
        if (buf_putc(out, (char)c) != 0) {
            parse_error(P, "out of memory");
            return -1;
        }
    }

    parse_error(P, "unterminated string");
    return -1;
}

static int parse_number(JsonParser *P, lua_State *L)
{
    const unsigned char *start = P->p;
    int is_integer = 1;

    if (*P->p == '-')
        P->p++;

    if (P->p >= P->end) {
        parse_error(P, "incomplete number");
        return -1;
    }
    if (*P->p == '0') {
        P->p++;
        if (P->p < P->end && *P->p >= '0' && *P->p <= '9') {
            parse_error(P, "leading zero in number");
            return -1;
        }
    } else if (*P->p >= '1' && *P->p <= '9') {
        while (P->p < P->end && *P->p >= '0' && *P->p <= '9')
            P->p++;
    } else {
        parse_error(P, "invalid number");
        return -1;
    }

    if (P->p < P->end && *P->p == '.') {
        is_integer = 0;
        P->p++;
        if (P->p >= P->end || *P->p < '0' || *P->p > '9') {
            parse_error(P, "missing digits after decimal point");
            return -1;
        }
        while (P->p < P->end && *P->p >= '0' && *P->p <= '9')
            P->p++;
    }

    if (P->p < P->end && (*P->p == 'e' || *P->p == 'E')) {
        is_integer = 0;
        P->p++;
        if (P->p < P->end && (*P->p == '+' || *P->p == '-'))
            P->p++;
        if (P->p >= P->end || *P->p < '0' || *P->p > '9') {
            parse_error(P, "missing exponent digits");
            return -1;
        }
        while (P->p < P->end && *P->p >= '0' && *P->p <= '9')
            P->p++;
    }

    size_t n = (size_t)(P->p - start);
    if (n >= JSON_MAX_NUMBER) {
        parse_error(P, "number is too long");
        return -1;
    }

    char text[JSON_MAX_NUMBER];
    memcpy(text, start, n);
    text[n] = 0;

    char *endptr = NULL;
    double value = strtod(text, &endptr);
    if (!endptr || *endptr != 0) {
        parse_error(P, "invalid number");
        return -1;
    }
    if (value != value || value > DBL_MAX || value < -DBL_MAX) {
        parse_error(P, "number out of range");
        return -1;
    }

    if (is_integer && value >= (double)INT_MIN && value <= (double)INT_MAX) {
        lua_pushinteger(L, (lua_Integer)(int)value);
    } else if (is_integer && sizeof(lua_Integer) >= 8 &&
               value >= -9007199254740992.0 &&
               value <= 9007199254740992.0) {
        lua_pushinteger(L, (lua_Integer)(long long)value);
    } else {
        lua_pushnumber(L, (lua_Number)value);
    }
    return 0;
}

static int parse_value(JsonParser *P, lua_State *L, int depth);

static int parse_array(JsonParser *P, lua_State *L, int depth)
{
    int index = 1;

    P->p++;
    lua_newtable(L);

    skip_ws(P);
    if (P->p < P->end && *P->p == ']') {
        P->p++;
        return 0;
    }

    for (;;) {
        if (parse_value(P, L, depth + 1) != 0)
            return -1;
        lua_rawseti(L, -2, index++);

        skip_ws(P);
        if (P->p >= P->end) {
            parse_error(P, "unterminated array");
            return -1;
        }
        if (*P->p == ']') {
            P->p++;
            return 0;
        }
        if (*P->p != ',') {
            parse_error(P, "expected ',' or ']'");
            return -1;
        }
        P->p++;
        skip_ws(P);
    }
}

static int parse_object(JsonParser *P, lua_State *L, int depth)
{
    P->p++;
    lua_newtable(L);

    skip_ws(P);
    if (P->p < P->end && *P->p == '}') {
        P->p++;
        return 0;
    }

    for (;;) {
        JsonBuf key = {0};
        if (parse_string(P, &key) != 0) {
            buf_free(&key);
            return -1;
        }
        lua_pushlstring(L, key.ptr ? key.ptr : "", key.len);
        buf_free(&key);

        skip_ws(P);
        if (P->p >= P->end || *P->p != ':') {
            parse_error(P, "expected ':'");
            return -1;
        }
        P->p++;

        if (parse_value(P, L, depth + 1) != 0)
            return -1;
        lua_settable(L, -3);

        skip_ws(P);
        if (P->p >= P->end) {
            parse_error(P, "unterminated object");
            return -1;
        }
        if (*P->p == '}') {
            P->p++;
            return 0;
        }
        if (*P->p != ',') {
            parse_error(P, "expected ',' or '}'");
            return -1;
        }
        P->p++;
        skip_ws(P);
    }
}

static int parse_literal(JsonParser *P, lua_State *L,
                         const char *literal, int lua_type)
{
    size_t n = strlen(literal);
    if ((size_t)(P->end - P->p) < n || memcmp(P->p, literal, n) != 0) {
        parse_error(P, "invalid literal");
        return -1;
    }
    P->p += n;
    if (lua_type == LUA_TBOOLEAN)
        lua_pushboolean(L, literal[0] == 't');
    else
        lua_pushnil(L);
    return 0;
}

static int parse_value(JsonParser *P, lua_State *L, int depth)
{
    if (depth > JSON_MAX_DEPTH) {
        parse_error(P, "nesting too deep");
        return -1;
    }

    skip_ws(P);
    if (P->p >= P->end) {
        parse_error(P, "unexpected end of input");
        return -1;
    }

    switch (*P->p) {
    case '{': return parse_object(P, L, depth);
    case '[': return parse_array(P, L, depth);
    case '"': {
        JsonBuf s = {0};
        int rc = parse_string(P, &s);
        if (rc == 0)
            lua_pushlstring(L, s.ptr ? s.ptr : "", s.len);
        buf_free(&s);
        return rc;
    }
    case 't': return parse_literal(P, L, "true",  LUA_TBOOLEAN);
    case 'f': return parse_literal(P, L, "false", LUA_TBOOLEAN);
    case 'n': return parse_literal(P, L, "null",  LUA_TNIL);
    default:
        if (*P->p == '-' || (*P->p >= '0' && *P->p <= '9'))
            return parse_number(P, L);
        parse_error(P, "unexpected character");
        return -1;
    }
}

static int json_parse(lua_State *L, const char *text, size_t len,
                      const char **err, size_t *offset)
{
    JsonParser P;
    P.start = (const unsigned char *)text;
    P.p = P.start;
    P.end = P.start + len;
    P.err = NULL;

    if (parse_value(&P, L, 0) != 0)
        goto fail;

    skip_ws(&P);
    if (P.p != P.end) {
        parse_error(&P, "trailing characters");
        goto fail;
    }
    return 0;

fail:
    *err = P.err ? P.err : "parse error";
    *offset = (size_t)(P.p - P.start);
    return -1;
}

/* ------------------------------------------------------------------ */
/* Serializer                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    lua_State *L;
    JsonBuf   *buf;
    const char *err;
} JsonEncoder;

static int enc_value(JsonEncoder *E, int index, int depth);

static int enc_string(JsonEncoder *E, const char *s, size_t len)
{
    static const char hex[] = "0123456789abcdef";
    size_t i;

    if (buf_putc(E->buf, '"') != 0)
        goto oom;

    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
        case '"':  if (buf_put(E->buf, "\\\"", 2) != 0) goto oom; break;
        case '\\': if (buf_put(E->buf, "\\\\", 2) != 0) goto oom; break;
        case '\b': if (buf_put(E->buf, "\\b", 2) != 0) goto oom; break;
        case '\f': if (buf_put(E->buf, "\\f", 2) != 0) goto oom; break;
        case '\n': if (buf_put(E->buf, "\\n", 2) != 0) goto oom; break;
        case '\r': if (buf_put(E->buf, "\\r", 2) != 0) goto oom; break;
        case '\t': if (buf_put(E->buf, "\\t", 2) != 0) goto oom; break;
        default:
            if (c < 0x20) {
                char esc[6];
                esc[0] = '\\';
                esc[1] = 'u';
                esc[2] = '0';
                esc[3] = '0';
                esc[4] = hex[c >> 4];
                esc[5] = hex[c & 0x0F];
                if (buf_put(E->buf, esc, sizeof(esc)) != 0)
                    goto oom;
            } else if (buf_putc(E->buf, (char)c) != 0) {
                goto oom;
            }
            break;
        }
    }

    if (buf_putc(E->buf, '"') != 0)
        goto oom;
    return 0;

oom:
    E->err = "out of memory";
    return -1;
}

static int enc_table(JsonEncoder *E, int index, int depth)
{
    lua_State *L = E->L;
    int has_int = 0, has_str = 0;
    long long max = 0;
    size_t count = 0;

    lua_pushnil(L);
    while (lua_next(L, index)) {
        int kt = lua_type(L, -2);
        if (kt == LUA_TSTRING) {
            has_str = 1;
        } else if (kt == LUA_TNUMBER) {
            lua_Number n = lua_tonumber(L, -2);
            long long k = (long long)n;
            if (n != (lua_Number)k || k < 1) {
                E->err = "array keys must be positive integers";
                lua_pop(L, 2);
                return -1;
            }
            has_int = 1;
            count++;
            if (k > max)
                max = k;
        } else {
            E->err = "table keys must be strings or positive integers";
            lua_pop(L, 2);
            return -1;
        }
        lua_pop(L, 1);
    }

    if (has_int && has_str) {
        E->err = "mixed array/object keys are not supported";
        return -1;
    }
    if (has_int && max != (long long)count) {
        E->err = "sparse arrays are not supported";
        return -1;
    }
    if (has_int && max > INT_MAX) {
        E->err = "array index is too large";
        return -1;
    }

    if (has_int) {
        long long i;
        if (buf_putc(E->buf, '[') != 0)
            goto oom;
        for (i = 1; i <= max; i++) {
            if (i > 1 && buf_putc(E->buf, ',') != 0)
                goto oom;
            lua_rawgeti(L, index, (int)i);
            if (enc_value(E, -1, depth + 1) != 0) {
                lua_pop(L, 1);
                return -1;
            }
            lua_pop(L, 1);
        }
        if (buf_putc(E->buf, ']') != 0)
            goto oom;
        return 0;
    }

    {
        int first = 1;
        if (buf_putc(E->buf, '{') != 0)
            goto oom;
        lua_pushnil(L);
        while (lua_next(L, index)) {
            size_t klen;
            const char *key = lua_tolstring(L, -2, &klen);
            if (!first && buf_putc(E->buf, ',') != 0) {
                lua_pop(L, 2);
                goto oom;
            }
            first = 0;
            if (enc_string(E, key, klen) != 0) {
                lua_pop(L, 2);
                return -1;
            }
            if (buf_putc(E->buf, ':') != 0) {
                lua_pop(L, 2);
                goto oom;
            }
            if (enc_value(E, -1, depth + 1) != 0) {
                lua_pop(L, 2);
                return -1;
            }
            lua_pop(L, 1);
        }
        if (buf_putc(E->buf, '}') != 0)
            goto oom;
        return 0;
    }

oom:
    E->err = "out of memory";
    return -1;
}

static int enc_number(JsonEncoder *E, lua_Number number)
{
    double value = (double)number;
    char text[64];

    if (value != value || value > DBL_MAX || value < -DBL_MAX) {
        E->err = "cannot encode NaN or infinity";
        return -1;
    }

    if (value == floor(value) &&
        value >= -9007199254740992.0 && value <= 9007199254740992.0) {
        snprintf(text, sizeof(text), "%lld", (long long)value);
    } else {
        snprintf(text, sizeof(text), "%.17g", value);
    }

    if (buf_put(E->buf, text, strlen(text)) != 0) {
        E->err = "out of memory";
        return -1;
    }
    return 0;
}

static int enc_value(JsonEncoder *E, int index, int depth)
{
    lua_State *L = E->L;
    int type;

    if (depth > JSON_MAX_DEPTH) {
        E->err = "nesting too deep";
        return -1;
    }

    type = lua_type(L, index);
    switch (type) {
    case LUA_TNIL:
        if (buf_put(E->buf, "null", 4) != 0) goto oom;
        return 0;
    case LUA_TBOOLEAN:
        if (lua_toboolean(L, index)) {
            if (buf_put(E->buf, "true", 4) != 0) goto oom;
        } else {
            if (buf_put(E->buf, "false", 5) != 0) goto oom;
        }
        return 0;
    case LUA_TNUMBER:
        return enc_number(E, lua_tonumber(L, index));
    case LUA_TSTRING: {
        size_t len;
        const char *s = lua_tolstring(L, index, &len);
        return enc_string(E, s, len);
    }
    case LUA_TTABLE:
        if (index < 0)
            index = lua_gettop(L) + index + 1;
        return enc_table(E, index, depth);
    default:
        E->err = "cannot encode this Lua type";
        return -1;
    }

oom:
    E->err = "out of memory";
    return -1;
}

/* ------------------------------------------------------------------ */
/* File helpers                                                       */
/* ------------------------------------------------------------------ */

static char *read_file(const char *path, long *size_out)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0 || size > JSON_MAX_FILE ||
        fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    char *buf = malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (size > 0 && fread(buf, 1, (size_t)size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    buf[size] = 0;
    if (size_out)
        *size_out = size;
    return buf;
}

static int replace_file(const char *tmp, const char *path)
{
    char backup[512];

    if (olfs_move(tmp, path) == 0)
        return 0;
    if (!olfs_exists(path)) {
        olfs_delete(tmp);
        return -1;
    }
    if (strlen(path) + sizeof(".~olbak") > sizeof(backup)) {
        olfs_delete(tmp);
        return -1;
    }
    snprintf(backup, sizeof(backup), "%s.~olbak", path);
    if (olfs_exists(backup))
        olfs_delete(backup);
    if (olfs_move(path, backup) != 0) {
        olfs_delete(tmp);
        return -1;
    }
    if (olfs_move(tmp, path) != 0) {
        olfs_move(backup, path);
        olfs_delete(tmp);
        return -1;
    }
    olfs_delete(backup);
    return 0;
}

static int write_file_atomic(const char *path, const char *data, size_t len)
{
    char tmp[512];

    if (strlen(path) + sizeof(".~oltmp") > sizeof(tmp))
        return -1;
    snprintf(tmp, sizeof(tmp), "%s.~oltmp", path);

    FILE *f = fopen(tmp, "wb");
    if (!f)
        return -1;

    int failed = 0;
    if (len > 0 && fwrite(data, 1, len, f) != len)
        failed = 1;
    if (fclose(f) != 0)
        failed = 1;
    if (failed) {
        olfs_delete(tmp);
        return -1;
    }
    return replace_file(tmp, path);
}

/* ------------------------------------------------------------------ */
/* Lua bindings                                                       */
/* ------------------------------------------------------------------ */

static int l_decode(lua_State *L)
{
    size_t len;
    const char *text = luaL_checklstring(L, 1, &len);
    const char *err = NULL;
    size_t offset = 0;

    if (json_parse(L, text, len, &err, &offset) != 0)
        return luaL_error(L, "json.decode: %s at byte %lu",
                          err, (unsigned long)offset);
    return 1;
}

static int l_encode(lua_State *L)
{
    JsonBuf buf = {0};
    JsonEncoder E;

    luaL_checkany(L, 1);
    E.L = L;
    E.buf = &buf;
    E.err = NULL;

    if (enc_value(&E, 1, 0) != 0) {
        buf_free(&buf);
        return luaL_error(L, "json.encode: %s", E.err ? E.err : "encode error");
    }

    lua_pushlstring(L, buf.ptr ? buf.ptr : "", buf.len);
    buf_free(&buf);
    return 1;
}

static int l_load(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    long size = 0;
    char *text = read_file(path, &size);
    if (!text)
        return luaL_error(L, "json.load: cannot read '%s'", path);

    const char *err = NULL;
    size_t offset = 0;
    int rc = json_parse(L, text, (size_t)size, &err, &offset);
    free(text);
    if (rc != 0)
        return luaL_error(L, "json.load: %s at byte %lu",
                          err, (unsigned long)offset);
    return 1;
}

static int l_save(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    JsonBuf buf = {0};
    JsonEncoder E;

    luaL_checkany(L, 2);
    E.L = L;
    E.buf = &buf;
    E.err = NULL;

    if (enc_value(&E, 2, 0) != 0) {
        buf_free(&buf);
        return luaL_error(L, "json.save: %s", E.err ? E.err : "encode error");
    }

    if (buf.len > JSON_MAX_FILE) {
        buf_free(&buf);
        return luaL_error(L, "json.save: encoded JSON exceeds %d bytes",
                          JSON_MAX_FILE);
    }

    int rc = write_file_atomic(path, buf.ptr ? buf.ptr : "", buf.len);
    buf_free(&buf);
    if (rc != 0)
        return luaL_error(L, "json.save: cannot write '%s'", path);

    lua_pushboolean(L, 1);
    return 1;
}

static const luaL_Reg json_funcs[] = {
    { "decode", l_decode },
    { "encode", l_encode },
    { "load",   l_load   },
    { "save",   l_save   },
    { NULL,     NULL     }
};

void json_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, json_funcs);
    lua_setglobal(L, "json");
}
