/* OpenLua - INI file module (OneLua-compatible).
 *
 * Pure text logic over the portable FS core: unit-testable on the host.
 */

#include "engine/fs/ini.h"
#include "engine/fs/olfs.h"
#include "engine/core/olreg.h"

#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INI_MAX_LINE   512        /* max chars per line */
#define INI_MAX_FILE   (256 * 1024)   /* read cap (sanity bound) */
#define NO_SECTION     ""

/* ------------------------------------------------------------------ */
/* helpers                                                              */
static char *trim(char *s)
{
    while (*s && (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n'))
        s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' ||
                       end[-1] == '\r' || end[-1] == '\n'))
        *--end = 0;
    return s;
}

/* Read the whole file (bounded) into a NUL-terminated malloc'd buffer. */
static char *read_inf(const char *path, long *size_out)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0 || size > INI_MAX_FILE)
    { fclose(f); return NULL; }

    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)size, f) != (size_t)size)
    { free(buf); fclose(f); return NULL; }
    fclose(f);
    buf[size] = 0;
    if (size_out) *size_out = size;
    return buf;
}

/*
 * Find "key=" inside section "section" in "text".
 * On success copies the value (after '=') into out (max out_len bytes),
 * and returns 0.  Returns -1 on not found.
 */
static int ini_find(const char *text, const char *section,
                    const char *key, char *out, size_t out_len)
{
    const char *cursor = text;
    int in_section = section[0] == 0;

    while (*cursor) {
        char line[INI_MAX_LINE];
        const char *nl = strchr(cursor, '\n');
        size_t raw_len = nl ? (size_t)(nl - cursor) : strlen(cursor);
        size_t len = raw_len;
        if (len >= sizeof(line))
            len = sizeof(line) - 1;
        memcpy(line, cursor, len);
        line[len] = 0;
        cursor = nl ? nl + 1 : cursor + raw_len;

        char *t = trim(line);
        if (!*t || *t == ';' || *t == '#')
            continue;

        if (*t == '[') {
            char *cb = strchr(t, ']');
            if (!cb) continue;
            *cb = 0;
            char *name = trim(t + 1);
            in_section = (strcmp(name, section) == 0);
            continue;
        }

        if (!in_section)
            continue;

        char *eq = strchr(t, '=');
        if (!eq)
            continue;
        *eq = 0;
        char *kname = trim(t);
        if (strcmp(kname, key) != 0)
            continue;
        /* Found it! Copy value safely. */
        const char *val = trim(eq + 1);
        size_t vlen = strlen(val);
        if (vlen >= out_len) vlen = out_len - 1;
        memcpy(out, val, vlen);
        out[vlen] = 0;
        return 0;
    }
    return -1;
}
static int ini_line_kind(char *line, char *name_or_key, char *value)
{
    char *t = trim(line);
    if (!*t)
        return 2;
    if (*t == '[') {
        char *cb = strchr(t, ']');
        if (!cb)
            return 2;
        *cb = 0;
        snprintf(name_or_key, 128, "%s", trim(t + 1));
        return 1;
    }
    char *eq = strchr(t, '=');
    if (!eq)
        return 2;
    *eq = 0;
    snprintf(name_or_key, 128, "%s", trim(t));
    if (value)
        snprintf(value, INI_MAX_LINE, "%s", trim(eq + 1));
    return 0;
}

typedef struct {
    const char *ptr;
    size_t len;
} IniSpan;

static int next_span(const char **cursor, IniSpan *line)
{
    const char *p = *cursor;
    if (!*p)
        return 0;
    const char *nl = strchr(p, '\n');
    line->ptr = p;
    line->len = nl ? (size_t)(nl - p) : strlen(p);
    *cursor = nl ? nl + 1 : p + line->len;
    return 1;
}

static void span_work(const IniSpan *line, char *work, size_t cap)
{
    size_t len = line->len;
    if (len >= cap)
        len = cap - 1;
    memcpy(work, line->ptr, len);
    work[len] = 0;
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
        olfs_move(backup, path);       /* restore the original         */
        olfs_delete(tmp);
        return -1;
    }
    olfs_delete(backup);
    return 0;
}

static int ini_set_value(const char *path, const char *section,
                         const char *key, const char *value)
{
    char tmp[512];
    const char *cursor;
    IniSpan line;
    int first_header = -1;
    int sec_start = -1, sec_end = -1;
    int replaced_at = -1, prefix_key = -1;
    int nlines = 0;

    if (strlen(path) + sizeof(".~oltmp") > sizeof(tmp))
        return -1;
    snprintf(tmp, sizeof(tmp), "%s.~oltmp", path);

    char *text = read_inf(path, NULL);
    if (!text) {
        if (olfs_exists(path))
            return -1;                 /* unreadable or over the cap   */
        text = malloc(1);
        if (!text)
            return -1;
        text[0] = 0;
    }

    /* Pass 1: find the requested section, an existing key and insertion point. */
    cursor = text;
    while (next_span(&cursor, &line)) {
        char work[INI_MAX_LINE];
        char name[128];

        span_work(&line, work, sizeof(work));
        int kind = ini_line_kind(work, name, NULL);
        if (kind == 1) {
            if (first_header < 0)
                first_header = nlines;
            if (sec_start >= 0 && sec_end < 0)
                sec_end = nlines;
            if (sec_start < 0 && strcmp(name, section) == 0)
                sec_start = nlines;
        } else if (kind == 0) {
            if (sec_start >= 0) {
                if ((sec_end < 0 || nlines < sec_end) &&
                    replaced_at < 0 && strcmp(name, key) == 0)
                    replaced_at = nlines;
            } else if (section[0] == 0 && first_header < 0 &&
                       prefix_key < 0 && strcmp(name, key) == 0) {
                prefix_key = nlines;
            }
        }
        nlines++;
    }
    if (sec_start >= 0 && sec_end < 0)
        sec_end = nlines;

    if (section[0] == 0 && sec_start < 0 && replaced_at < 0)
        replaced_at = prefix_key;

    int append_header = section[0] != 0 && sec_start < 0;
    int insert_at = -1;
    if (replaced_at < 0) {
        if (sec_start >= 0)
            insert_at = sec_end;
        else if (section[0] == 0)
            insert_at = first_header >= 0 ? first_header : nlines;
        else
            insert_at = nlines;
    }

    /* Pass 2: stream the old text into the temp file, replacing/inserting. */
    FILE *w = fopen(tmp, "wb");
    if (!w) {
        free(text);
        return -1;
    }

    int written = 0;
    cursor = text;
    int line_no = 0;
    while (next_span(&cursor, &line)) {
        if (insert_at >= 0 && line_no == insert_at) {
            if (append_header) {
                if (nlines > 0)
                    fputc('\n', w);
                fprintf(w, "[%s]\n", section);
            }
            fprintf(w, "%s = %s\n", key, value);
            written = 1;
        }
        if (line_no == replaced_at) {
            fprintf(w, "%s = %s\n", key, value);
            written = 1;
        } else {
            fwrite(line.ptr, 1, line.len, w);
            fputc('\n', w);
        }
        line_no++;
    }
    if (!written) {
        if (append_header) {
            if (nlines > 0)
                fputc('\n', w);
            fprintf(w, "[%s]\n", section);
        }
        fprintf(w, "%s = %s\n", key, value);
    }

    int write_failed = ferror(w) != 0;
    if (fclose(w) != 0)
        write_failed = 1;
    free(text);
    if (write_failed) {
        olfs_delete(tmp);
        return -1;
    }
    return replace_file(tmp, path);
}

/* ---- Lua bindings ------------------------------------------------------ */

static int l_read(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    const char *section, *key, *def;
    int nargs = lua_gettop(L);

    if (nargs >= 3) {     /* section mode with path+sec+key */
        section = luaL_checkstring(L, 2);
        key     = luaL_checkstring(L, 3);
        def     = luaL_optstring(L, 4, "");
    } else {                /* 2 args: no-section path+key */
        section = NO_SECTION;
        key     = luaL_checkstring(L, 2);
        def     = "";
    }

    long size = 0;
    char *text = read_inf(path, &size);
    if (!text) {
        lua_pushstring(L, def);
        return 1;
    }

    char val[INI_MAX_LINE];
    int rc = ini_find(text, section, key, val, sizeof(val));
    free(text);

    if (rc == 0)
        lua_pushstring(L, val);
    else
        lua_pushstring(L, def);
    return 1;
}

static int l_write(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    const char *section, *key, *value;
    int nargs = lua_gettop(L);

    if (nargs >= 4) {
        section = luaL_checkstring(L, 2);
        key     = luaL_checkstring(L, 3);
        value   = luaL_checkstring(L, 4);
    } else {
        section = NO_SECTION;
        key     = luaL_checkstring(L, 2);
        value   = luaL_checkstring(L, 3);
    }

    if (ini_set_value(path, section, key, value) != 0)
        luaL_error(L, "ini.write: failed for '%s' -> %s.%s", path, section, key);
    return 0;
}

static const luaL_Reg ini_funcs[] = {
    { "read",  l_read },
    { "write", l_write },
    { NULL,    NULL }
};

void ini_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, ini_funcs);
    lua_setglobal(L, "ini");
}
