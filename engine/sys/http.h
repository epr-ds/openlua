/* OpenLua - OneLua-compatible HTTP client.
 *
 * Synchronous HTTP/1.x only (no TLS).  The parser and request helpers are
 * exposed for host tests; the Lua layer adds http.getfile/postfile/request.
 */

#ifndef OL_HTTP_H
#define OL_HTTP_H

#include <stddef.h>
#include <lua.h>

#define OL_HTTP_MAX_HEADERS 32
#define OL_HTTP_BODY_MAX    (16u * 1024u * 1024u)

typedef struct {
    char name[64];
    char value[256];
} OlHttpHeader;

typedef struct {
    int status;
    char reason[64];
    OlHttpHeader headers[OL_HTTP_MAX_HEADERS];
    int header_count;
    unsigned char *body;
    size_t body_len;
} OlHttpResponse;

typedef struct {
    char host[256];
    char path[1024];
    int port;
} OlHttpUrl;

/* Parse http://host[:port]/path.  Returns 1 on success, 0 on unsupported or
 * malformed URLs.  https:// is rejected because PSPSDK has no TLS client. */
int http_parse_url(const char *url, OlHttpUrl *out);

/* Perform a request.  Returns 1 when a response was received (any status),
 * 0 on connect/parse failure.  The caller frees body with http_response_free. */
int http_request(const char *method, const char *url, const char *body,
                 size_t body_len, OlHttpResponse *out);
void http_response_free(OlHttpResponse *resp);

/* Convenience wrappers used by the Lua module and host tests. */
int http_getfile(const char *url, const char *path);
int http_postfile(const char *url, const char *path, const char *body,
                  size_t body_len);

/* Lua registration. */
void http_api_register(lua_State *L);

#endif
