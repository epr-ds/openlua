/* OpenLua - OneLua-compatible HTTP client. */

#include "engine/sys/http.h"
#include "engine/net/net.h"
#include "engine/core/olreg.h"

#include <ctype.h>
#include <lauxlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OL_HTTP_HEADER_MAX  (16u * 1024u)
#define OL_HTTP_REDIRECTS   4
#define OL_HTTP_TIMEOUT_MS  15000

typedef struct {
    unsigned char *data;
    size_t len;
    size_t cap;
} OlHttpBuf;

static char g_cookie_name[8][32];
static char g_cookie_value[8][128];
static int g_cookie_count;

static int buf_reserve(OlHttpBuf *b, size_t extra)
{
    size_t need = b->len + extra;
    size_t cap;
    unsigned char *p;

    if (need < b->len || need > OL_HTTP_BODY_MAX)
        return 0;
    if (need <= b->cap)
        return 1;
    cap = b->cap ? b->cap : 4096;
    while (cap < need) {
        if (cap > OL_HTTP_BODY_MAX / 2) {
            cap = OL_HTTP_BODY_MAX;
            break;
        }
        cap *= 2;
    }
    p = (unsigned char *)realloc(b->data, cap);
    if (!p)
        return 0;
    b->data = p;
    b->cap = cap;
    return 1;
}

static int buf_add(OlHttpBuf *b, const void *data, size_t len)
{
    if (!buf_reserve(b, len))
        return 0;
    if (len)
        memcpy(b->data + b->len, data, len);
    b->len += len;
    return 1;
}

static void buf_free(OlHttpBuf *b)
{
    free(b->data);
    b->data = NULL;
    b->len = b->cap = 0;
}

static int ascii_equal(const char *a, const char *b)
{
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a++;
        unsigned char cb = (unsigned char)*b++;
        if (tolower(ca) != tolower(cb))
            return 0;
    }
    return *a == '\0' && *b == '\0';
}

static const char *find_header(const OlHttpResponse *r, const char *name)
{
    int i;
    for (i = 0; i < r->header_count; i++) {
        if (ascii_equal(r->headers[i].name, name))
            return r->headers[i].value;
    }
    return NULL;
}

int http_parse_url(const char *url, OlHttpUrl *out)
{
    const char *p;
    const char *host_start;
    const char *host_end;
    size_t host_len;
    int port = 80;

    if (!url || !out)
        return 0;
    memset(out, 0, sizeof(*out));

    if (strncmp(url, "http://", 7) != 0)
        return 0;
    p = url + 7;
    host_start = p;
    while (*p && *p != ':' && *p != '/' && *p != '?' && *p != '#')
        p++;
    host_end = p;
    host_len = (size_t)(host_end - host_start);
    if (host_len == 0 || host_len >= sizeof(out->host))
        return 0;

    {
        const char *q;
        for (q = host_start; q < host_end; q++) {
            unsigned char c = (unsigned char)*q;
            if (c <= ' ' || c == '@' || c == '[' || c == ']')
                return 0;
        }
    }
    memcpy(out->host, host_start, host_len);
    out->host[host_len] = '\0';

    if (*p == ':') {
        int digits = 0;
        port = 0;
        p++;
        while (*p >= '0' && *p <= '9') {
            port = port * 10 + (*p - '0');
            if (++digits > 5 || port > 65535)
                return 0;
            p++;
        }
        if (digits == 0 || port == 0)
            return 0;
    }

    if (*p == '#') {
        snprintf(out->path, sizeof(out->path), "/");
    } else if (*p == '\0') {
        snprintf(out->path, sizeof(out->path), "/");
    } else if (*p == '?') {
        if (snprintf(out->path, sizeof(out->path), "/%s", p) >=
            (int)sizeof(out->path))
            return 0;
    } else if (*p == '/') {
        if (snprintf(out->path, sizeof(out->path), "%s", p) >=
            (int)sizeof(out->path))
            return 0;
    } else {
        return 0;
    }

    {
        char *hash = strchr(out->path, '#');
        if (hash)
            *hash = '\0';
    }

    out->port = port;
    return 1;
}

static int join_redirect(const OlHttpUrl *base, const char *location,
                         char *out, size_t cap)
{
    int n;
    const char *slash;
    size_t dir;

    if (!location || !*location)
        return 0;
    if (strncmp(location, "http://", 7) == 0) {
        if (strlen(location) >= cap)
            return 0;
        snprintf(out, cap, "%s", location);
        return 1;
    }
    if (location[0] == '/') {
        if (base->port == 80)
            n = snprintf(out, cap, "http://%s%s", base->host, location);
        else
            n = snprintf(out, cap, "http://%s:%d%s", base->host, base->port,
                         location);
        return n > 0 && (size_t)n < cap;
    }

    slash = strrchr(base->path, '/');
    dir = slash ? (size_t)(slash - base->path) + 1 : 1;
    if (base->port == 80)
        n = snprintf(out, cap, "http://%s%.*s%s", base->host,
                     (int)dir, base->path, location);
    else
        n = snprintf(out, cap, "http://%s:%d%.*s%s", base->host, base->port,
                     (int)dir, base->path, location);
    return n > 0 && (size_t)n < cap;
}

static void cookie_store(const char *set_cookie)
{
    char name[32];
    char value[128];
    const char *p = set_cookie;
    size_t n = 0;
    int i;

    while (*p && *p != '=' && *p != ';' && n + 1 < sizeof(name))
        name[n++] = *p++;
    name[n] = '\0';
    if (n == 0 || *p != '=')
        return;
    p++;
    n = 0;
    while (*p && *p != ';' && n + 1 < sizeof(value))
        value[n++] = *p++;
    value[n] = '\0';

    for (i = 0; i < g_cookie_count; i++) {
        if (strcmp(g_cookie_name[i], name) == 0) {
            snprintf(g_cookie_value[i], sizeof(g_cookie_value[i]), "%s", value);
            return;
        }
    }
    if (g_cookie_count < 8) {
        snprintf(g_cookie_name[g_cookie_count], sizeof(g_cookie_name[0]), "%s", name);
        snprintf(g_cookie_value[g_cookie_count], sizeof(g_cookie_value[0]), "%s", value);
        g_cookie_count++;
    }
}

static void cookie_header(char *out, size_t cap)
{
    int i;
    size_t used = 0;
    out[0] = '\0';
    for (i = 0; i < g_cookie_count; i++) {
        int n = snprintf(out + used, cap - used, "%s%s=%s",
                         i ? "; " : "", g_cookie_name[i], g_cookie_value[i]);
        if (n < 0 || (size_t)n >= cap - used)
            return;
        used += (size_t)n;
    }
}

static int read_more(int fd, unsigned char *buf, size_t cap, size_t *used)
{
    int n;
    if (*used >= cap)
        return 0;
    if (ol_net_tcp_wait(fd, OL_HTTP_TIMEOUT_MS) <= 0)
        return -1;
    n = ol_net_tcp_recv(fd, buf + *used, cap - *used);
    if (n <= 0)
        return n == 0 ? 0 : -1;
    *used += (size_t)n;
    return 1;
}

static int read_body(int fd, const unsigned char *initial, size_t initial_len,
                     const OlHttpResponse *resp, OlHttpBuf *body)
{
    const char *te = find_header(resp, "Transfer-Encoding");
    const char *cl = find_header(resp, "Content-Length");
    int chunked = te && strstr(te, "chunked") != NULL;
    int have_len = 0;
    size_t want = 0;

    if (initial_len && !buf_add(body, initial, initial_len))
        return 0;

    if (!chunked && cl) {
        unsigned long long v = 0;
        const char *p = cl;
        while (*p == ' ' || *p == '\t')
            p++;
        while (*p >= '0' && *p <= '9') {
            v = v * 10ull + (unsigned long long)(*p - '0');
            if (v > OL_HTTP_BODY_MAX)
                return 0;
            p++;
        }
        if (*p == '\0' || *p == ' ' || *p == '\t') {
            have_len = 1;
            want = (size_t)v;
        }
    }

    if (have_len) {
        unsigned char tmp[4096];
        if (body->len > want)
            body->len = want;
        while (body->len < want) {
            size_t need = want - body->len;
            int n;
            if (need > sizeof(tmp))
                need = sizeof(tmp);
            if (ol_net_tcp_wait(fd, OL_HTTP_TIMEOUT_MS) <= 0)
                return 0;
            n = ol_net_tcp_recv(fd, tmp, need);
            if (n <= 0)
                return 0;
            if (!buf_add(body, tmp, (size_t)n))
                return 0;
        }
    } else {
        unsigned char tmp[4096];
        for (;;) {
            int n;
            if (ol_net_tcp_wait(fd, OL_HTTP_TIMEOUT_MS) <= 0)
                break;
            n = ol_net_tcp_recv(fd, tmp, sizeof(tmp));
            if (n <= 0)
                break;
            if (!buf_add(body, tmp, (size_t)n))
                return 0;
        }
    }

    if (!chunked)
        return 1;

    /* Decode chunked transfer encoding in place into a second buffer. */
    {
        OlHttpBuf decoded = {0};
        size_t pos = 0;
        while (pos < body->len) {
            size_t line_end = pos;
            unsigned long long chunk = 0;
            int digits = 0;
            while (line_end < body->len &&
                   body->data[line_end] != '\r' && body->data[line_end] != '\n')
                line_end++;
            if (line_end >= body->len)
                break;
            {
                size_t i = pos;
                int invalid = 0;
                while (i < line_end && isxdigit((unsigned char)body->data[i])) {
                    unsigned char c = body->data[i];
                    int v = (c <= '9') ? (c - '0') :
                            (tolower(c) - 'a' + 10);
                    if (chunk > (OL_HTTP_BODY_MAX >> 4)) {
                        invalid = 1;
                        break;
                    }
                    chunk = chunk * 16u + (unsigned int)v;
                    digits++;
                    i++;
                }
                while (i < line_end && (body->data[i] == ' ' ||
                                        body->data[i] == '\t'))
                    i++;
                if (invalid || (i < line_end && body->data[i] != ';'))
                    break;
            }
            if (digits == 0)
                break;
            pos = line_end;
            while (pos < body->len && (body->data[pos] == '\r' || body->data[pos] == '\n'))
                pos++;
            if (chunk == 0)
                break;
            if (chunk > body->len - pos)
                break;
            if (!buf_add(&decoded, body->data + pos, (size_t)chunk)) {
                buf_free(&decoded);
                return 0;
            }
            pos += (size_t)chunk;
            while (pos < body->len && (body->data[pos] == '\r' || body->data[pos] == '\n'))
                pos++;
        }
        buf_free(body);
        *body = decoded;
    }
    return 1;
}

static int parse_headers(const unsigned char *data, size_t len,
                         OlHttpResponse *resp)
{
    size_t pos = 0;
    int first = 1;

    while (pos < len) {
        size_t end = pos;
        while (end < len && data[end] != '\r' && data[end] != '\n')
            end++;
        if (end == pos)
            break;
        if (first) {
            const char *sp = (const char *)data + pos;
            size_t n = end - pos;
            const char *space = memchr(sp, ' ', n);
            if (n < 8 || memcmp(sp, "HTTP/", 5) != 0 || !space)
                return 0;
            resp->status = atoi(space + 1);
            if (resp->status < 100 || resp->status > 599)
                return 0;
            {
                const char *r = space + 1;
                while (*r && *r != ' ')
                    r++;
                while (*r == ' ')
                    r++;
                n = end - (size_t)(r - sp);
                if (n >= sizeof(resp->reason))
                    n = sizeof(resp->reason) - 1;
                memcpy(resp->reason, r, n);
                resp->reason[n] = '\0';
            }
            first = 0;
        } else if (resp->header_count < OL_HTTP_MAX_HEADERS) {
            const char *line = (const char *)data + pos;
            const char *colon = memchr(line, ':', end - pos);
            if (colon) {
                size_t nl = (size_t)(colon - line);
                const char *value = colon + 1;
                size_t vl = end - (size_t)(value - (const char *)data);
                OlHttpHeader *h = &resp->headers[resp->header_count];

                while (nl && (line[nl - 1] == ' ' || line[nl - 1] == '\t'))
                    nl--;
                while (vl && (*value == ' ' || *value == '\t')) {
                    value++;
                    vl--;
                }
                while (vl && (value[vl - 1] == ' ' || value[vl - 1] == '\t'))
                    vl--;
                if (nl >= sizeof(h->name))
                    nl = sizeof(h->name) - 1;
                if (vl >= sizeof(h->value))
                    vl = sizeof(h->value) - 1;
                memcpy(h->name, line, nl);
                h->name[nl] = '\0';
                memcpy(h->value, value, vl);
                h->value[vl] = '\0';
                resp->header_count++;
            }
        }
        pos = end;
        while (pos < len && (data[pos] == '\r' || data[pos] == '\n'))
            pos++;
    }
    return !first;
}

int http_request(const char *method, const char *url, const char *body,
                 size_t body_len, OlHttpResponse *out)
{
    char current[2048];
    int redirects = 0;

    if (!method || !url || !out)
        return 0;
    memset(out, 0, sizeof(*out));
    snprintf(current, sizeof(current), "%s", url);

    for (;;) {
        OlHttpUrl u;
        int fd = -1;
        char request[4096];
        char cookies[1024];
        int n;
        unsigned char *hdr = NULL;
        size_t used = 0;
        size_t header_end = 0;
        OlHttpBuf body_buf = {0};
        const char *location;
        int ret;

        memset(out, 0, sizeof(*out));
        if (!http_parse_url(current, &u))
            return 0;
        if (!ol_net_acquire())
            return 0;

        fd = ol_net_tcp_connect(u.host, u.port, OL_HTTP_TIMEOUT_MS);
        if (fd < 0) {
            ol_net_release();
            return 0;
        }

        cookie_header(cookies, sizeof(cookies));
        if (body && body_len)
            n = snprintf(request, sizeof(request),
                         "%s %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: OpenLua/1.0\r\n"
                         "Accept: */*\r\nConnection: close\r\nContent-Type: "
                         "application/x-www-form-urlencoded\r\nContent-Length: %u\r\n"
                         "%s%s\r\n",
                         method, u.path, u.host, (unsigned int)body_len,
                         cookies[0] ? "Cookie: " : "", cookies);
        else
            n = snprintf(request, sizeof(request),
                         "%s %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: OpenLua/1.0\r\n"
                         "Accept: */*\r\nConnection: close\r\n%s%s\r\n",
                         method, u.path, u.host,
                         cookies[0] ? "Cookie: " : "", cookies);
        if (n <= 0 || (size_t)n >= sizeof(request)) {
            ol_net_tcp_close(fd);
            ol_net_release();
            return 0;
        }
        if (ol_net_tcp_send_all(fd, request, (size_t)n) < 0 ||
            (body_len && ol_net_tcp_send_all(fd, body, body_len) < 0)) {
            ol_net_tcp_close(fd);
            ol_net_release();
            return 0;
        }

        hdr = (unsigned char *)malloc(OL_HTTP_HEADER_MAX);
        if (!hdr) {
            ol_net_tcp_close(fd);
            ol_net_release();
            return 0;
        }

        while (used < OL_HTTP_HEADER_MAX) {
            int r = read_more(fd, hdr, OL_HTTP_HEADER_MAX, &used);
            size_t i;
            if (r < 0)
                break;
            for (i = 0; i + 3 < used; i++) {
                if (hdr[i] == '\r' && hdr[i + 1] == '\n' &&
                    hdr[i + 2] == '\r' && hdr[i + 3] == '\n') {
                    header_end = i + 4;
                    break;
                }
            }
            if (header_end)
                break;
            if (r == 0)
                break;
        }

        if (!header_end || !parse_headers(hdr, header_end - 4, out)) {
            free(hdr);
            ol_net_tcp_close(fd);
            ol_net_release();
            return 0;
        }

        ret = read_body(fd, hdr + header_end, used - header_end, out, &body_buf);
        free(hdr);
        ol_net_tcp_close(fd);
        ol_net_release();
        if (!ret) {
            buf_free(&body_buf);
            return 0;
        }

        {
            int i;
            for (i = 0; i < out->header_count; i++) {
                if (ascii_equal(out->headers[i].name, "Set-Cookie"))
                    cookie_store(out->headers[i].value);
            }
        }

        out->body = body_buf.data;
        out->body_len = body_buf.len;

        location = find_header(out, "Location");
        if ((out->status == 301 || out->status == 302 ||
             out->status == 303 || out->status == 307 ||
             out->status == 308) && location && redirects < OL_HTTP_REDIRECTS) {
            char next[2048];
            if (!join_redirect(&u, location, next, sizeof(next))) {
                http_response_free(out);
                return 0;
            }
            http_response_free(out);
            snprintf(current, sizeof(current), "%s", next);
            redirects++;
            continue;
        }

        return 1;
    }
}

void http_response_free(OlHttpResponse *resp)
{
    if (!resp)
        return;
    free(resp->body);
    resp->body = NULL;
    resp->body_len = 0;
}

int http_getfile(const char *url, const char *path)
{
    OlHttpResponse resp;
    FILE *f;
    int ok;

    if (!http_request("GET", url, NULL, 0, &resp))
        return 0;
    ok = resp.status >= 200 && resp.status < 300;
    if (ok) {
        f = fopen(path, "wb");
        if (!f)
            ok = 0;
        else {
            if (resp.body_len && fwrite(resp.body, 1, resp.body_len, f) != resp.body_len)
                ok = 0;
            if (fclose(f) != 0)
                ok = 0;
        }
    }
    http_response_free(&resp);
    return ok;
}

int http_postfile(const char *url, const char *path, const char *body,
                  size_t body_len)
{
    OlHttpResponse resp;
    FILE *f;
    int ok;

    if (!http_request("POST", url, body, body_len, &resp))
        return 0;
    ok = resp.status >= 200 && resp.status < 300;
    if (ok) {
        f = fopen(path, "wb");
        if (!f)
            ok = 0;
        else {
            if (resp.body_len && fwrite(resp.body, 1, resp.body_len, f) != resp.body_len)
                ok = 0;
            if (fclose(f) != 0)
                ok = 0;
        }
    }
    http_response_free(&resp);
    return ok;
}

static int l_getfile(lua_State *L)
{
    const char *url = luaL_checkstring(L, 1);
    const char *path = luaL_checkstring(L, 2);

    /* OneLua's third argument (background download) is accepted but the
     * request still runs synchronously; see docs/API.md. */
    if (lua_gettop(L) >= 3)
        (void)luaL_checkinteger(L, 3);
    lua_pushboolean(L, http_getfile(url, path));
    return 1;
}

static int l_postfile(lua_State *L)
{
    const char *url = luaL_checkstring(L, 1);
    const char *path = luaL_checkstring(L, 2);
    size_t len = 0;
    const char *data = luaL_checklstring(L, 3, &len);
    lua_Integer want = (lua_Integer)len;

    if (lua_gettop(L) >= 4)
        want = luaL_checkinteger(L, 4);
    if (want < 0)
        want = 0;
    if ((size_t)want > len)
        want = (lua_Integer)len;

    lua_pushboolean(L, http_postfile(url, path, data, (size_t)want));
    return 1;
}

static int l_request(lua_State *L)
{
    const char *method = luaL_checkstring(L, 1);
    const char *url = luaL_checkstring(L, 2);
    size_t body_len = 0;
    const char *body = NULL;
    OlHttpResponse resp;
    int i;

    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3))
        body = luaL_checklstring(L, 3, &body_len);
    if (!http_request(method, url, body, body_len, &resp)) {
        lua_pushnil(L);
        lua_pushliteral(L, "request failed");
        return 2;
    }

    lua_newtable(L);
    lua_pushinteger(L, resp.status);
    lua_setfield(L, -2, "status");
    lua_pushstring(L, resp.reason);
    lua_setfield(L, -2, "reason");
    lua_pushstring(L, url);
    lua_setfield(L, -2, "url");
    lua_pushlstring(L, resp.body ? (const char *)resp.body : "",
                    resp.body_len);
    lua_setfield(L, -2, "body");
    lua_pushinteger(L, (lua_Integer)resp.body_len);
    lua_setfield(L, -2, "length");

    lua_newtable(L);
    for (i = 0; i < resp.header_count; i++) {
        lua_pushstring(L, resp.headers[i].value);
        lua_setfield(L, -2, resp.headers[i].name);
    }
    lua_setfield(L, -2, "headers");

    http_response_free(&resp);
    return 1;
}

static int l_term(lua_State *L)
{
    (void)L;
    memset(g_cookie_name, 0, sizeof(g_cookie_name));
    memset(g_cookie_value, 0, sizeof(g_cookie_value));
    g_cookie_count = 0;
    return 0;
}

static const luaL_Reg http_funcs[] = {
    { "getfile",  l_getfile  },
    { "postfile", l_postfile },
    { "request",  l_request  },
    { "term",     l_term     },
    { NULL, NULL }
};

void http_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, http_funcs);
    lua_setglobal(L, "http");
}
