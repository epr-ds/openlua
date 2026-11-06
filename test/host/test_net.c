/* Host checks for the networking modules.
 *
 * URL/response parsing is pure; socket, HTTP and FTP are exercised against
 * loopback servers in this process.  No PSP or real network is involved.
 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "engine/net/net.h"
#include "engine/sys/http.h"
#include "engine/sys/ftp.h"

static int checks;
static int failures;

static void expect(const char *what, int ok)
{
    checks++;
    if (!ok) {
        failures++;
        printf("net: %s: FAILED\n", what);
    }
}

static void expect_str(const char *what, const char *got, const char *want)
{
    checks++;
    if (!got || strcmp(got, want) != 0) {
        failures++;
        printf("net: %s: got \"%s\", want \"%s\"\n", what,
               got ? got : "(null)", want);
    }
}

static void expect_int(const char *what, long got, long want)
{
    checks++;
    if (got != want) {
        failures++;
        printf("net: %s: got %ld, want %ld\n", what, got, want);
    }
}

static void test_urls(void)
{
    OlHttpUrl u;

    expect("url simple", http_parse_url("http://example.com/a?b=1", &u));
    expect_str("url host", u.host, "example.com");
    expect_str("url path", u.path, "/a?b=1");
    expect_int("url port", u.port, 80);

    expect("url port", http_parse_url("http://127.0.0.1:8080/", &u));
    expect_int("url custom port", u.port, 8080);
    expect_str("url ip", u.host, "127.0.0.1");

    expect("url no path", http_parse_url("http://host", &u));
    expect_str("url default path", u.path, "/");

    expect("url query only", http_parse_url("http://host?x=1", &u));
    expect_str("url query path", u.path, "/?x=1");

    expect("url fragment", http_parse_url("http://host/#frag", &u));
    expect_str("url fragment path", u.path, "/");

    expect("url rejects https", !http_parse_url("https://host/", &u));
    expect("url rejects empty host", !http_parse_url("http:///x", &u));
    expect("url rejects bad port", !http_parse_url("http://host:0/", &u));
    expect("url rejects huge port", !http_parse_url("http://host:70000/", &u));
    expect("url rejects userinfo", !http_parse_url("http://a@host/", &u));
}

static void test_ipv4(void)
{
    unsigned int addr;
    char text[64];

    expect("ipv4 valid", ol_net_parse_ipv4("192.168.1.20", &addr));
    ol_net_format_ipv4(addr, text, sizeof(text));
    expect_str("ipv4 round trip", text, "192.168.1.20");
    expect("ipv4 rejects high octet", !ol_net_parse_ipv4("256.1.1.1", &addr));
    expect("ipv4 rejects short", !ol_net_parse_ipv4("1.2.3", &addr));
    expect("ipv4 rejects text", !ol_net_parse_ipv4("a.b.c.d", &addr));
}

typedef struct {
    int fd;
    int count;
    const char *responses[4];
} TestServer;

static void *server_thread(void *arg)
{
    TestServer *s = (TestServer *)arg;
    int i;

    for (i = 0; i < s->count; i++) {
        int client = ol_net_tcp_accept(s->fd, 5000);
        char request[4096];
        if (client < 0)
            break;
        (void)ol_net_tcp_recv(client, request, sizeof(request));
        ol_net_tcp_send_all(client, s->responses[i], strlen(s->responses[i]));
        ol_net_tcp_close(client);
    }
    ol_net_tcp_close(s->fd);
    return NULL;
}

static int start_server(TestServer *s, int count, ...)
{
    va_list ap;
    pthread_t thread;
    int i;

    memset(s, 0, sizeof(*s));
    s->fd = ol_net_tcp_listen(0, 4);
    if (s->fd < 0)
        return 0;
    s->count = count;
    va_start(ap, count);
    for (i = 0; i < count; i++)
        s->responses[i] = va_arg(ap, const char *);
    va_end(ap);
    if (pthread_create(&thread, NULL, server_thread, s) != 0) {
        ol_net_tcp_close(s->fd);
        return 0;
    }
    pthread_detach(thread);
    return 1;
}

static void test_socket_loopback(void)
{
    int listen_fd = ol_net_tcp_listen(0, 1);
    int port;
    int client;
    char buf[16];

    expect("socket listen", listen_fd >= 0);
    if (listen_fd < 0)
        return;
    port = ol_net_tcp_local_port(listen_fd);
    expect("socket local port", port > 0);
    client = ol_net_tcp_connect("127.0.0.1", port, 2000);
    expect("socket connect", client >= 0);
    if (client >= 0) {
        int server = ol_net_tcp_accept(listen_fd, 2000);
        expect("socket accept", server >= 0);
        if (server >= 0) {
            expect("socket send", ol_net_tcp_send_all(client, "ping", 4) == 4);
            expect_int("socket recv", ol_net_tcp_recv(server, buf, sizeof(buf)), 4);
            buf[4] = '\0';
            expect_str("socket payload", buf, "ping");
            ol_net_tcp_close(server);
        }
        ol_net_tcp_close(client);
    }
    ol_net_tcp_close(listen_fd);
}

static void test_http(void)
{
    TestServer server;
    char url[128];
    char path[] = "/tmp/openlua-http-XXXXXX";
    int fd;
    char got[64];
    FILE *f;
    size_t n;
    OlHttpResponse resp;
    int port;

    if (!start_server(&server, 2,
                      "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nX-Test: yes\r\n"
                      "Connection: close\r\n\r\nhello",
                      "HTTP/1.1 200 OK\r\nContent-Length: 6\r\n"
                      "Connection: close\r\n\r\nposted")) {
        expect("http server", 0);
        return;
    }
    port = ol_net_tcp_local_port(server.fd);
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/hello", port);

    expect("http request", http_request("GET", url, NULL, 0, &resp) == 1);
    expect_int("http status", resp.status, 200);
    expect_int("http body len", (long)resp.body_len, 5);
    expect_str("http body", (const char *)resp.body, "hello");
    {
        const char *x = NULL;
        int i;
        for (i = 0; i < resp.header_count; i++) {
            if (strcmp(resp.headers[i].name, "X-Test") == 0)
                x = resp.headers[i].value;
        }
        expect_str("http header", x, "yes");
    }
    http_response_free(&resp);

    fd = mkstemp(path);
    expect("http temp file", fd >= 0);
    if (fd >= 0)
        close(fd);
    expect("http postfile", http_postfile(url, path, "x=1", 3) == 1);
    f = fopen(path, "rb");
    n = f ? fread(got, 1, sizeof(got) - 1, f) : 0;
    if (f)
        fclose(f);
    got[n] = '\0';
    expect_str("http post body", got, "posted");
    unlink(path);

    expect("http rejects https",
           !http_request("GET", "https://example.com/", NULL, 0, &resp));
    expect("http rejects bad url",
           !http_request("GET", "not-a-url", NULL, 0, &resp));
}

static void test_http_redirect(void)
{
    TestServer server;
    char url[128];
    OlHttpResponse resp;
    int port;

    if (!start_server(&server, 2,
                      "HTTP/1.1 302 Found\r\nLocation: /final\r\n"
                      "Content-Length: 0\r\nConnection: close\r\n\r\n",
                      "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n"
                      "Connection: close\r\n\r\nfinal")) {
        expect("redirect server", 0);
        return;
    }
    port = ol_net_tcp_local_port(server.fd);
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/start", port);
    expect("http redirect", http_request("GET", url, NULL, 0, &resp) == 1);
    expect_int("http redirect status", resp.status, 200);
    expect_str("http redirect body", (const char *)resp.body, "final");
    http_response_free(&resp);
}

static int recv_line(int fd, char *out, size_t cap)
{
    size_t used = 0;
    while (used + 1 < cap) {
        char c;
        int n = ol_net_tcp_recv(fd, &c, 1);
        if (n <= 0)
            return 0;
        if (c == '\n') {
            if (used && out[used - 1] == '\r')
                used--;
            out[used] = '\0';
            return 1;
        }
        out[used++] = c;
    }
    out[used] = '\0';
    return 0;
}

static int ftp_command(int fd, const char *cmd, char *reply, size_t cap)
{
    if (ol_net_tcp_send_all(fd, cmd, strlen(cmd)) < 0)
        return 0;
    if (ol_net_tcp_send_all(fd, "\r\n", 2) < 0)
        return 0;
    return recv_line(fd, reply, cap);
}

static void test_ftp(void)
{
    char dir[] = "/tmp/openlua-ftp-XXXXXX";
    char file[256];
    char reply[512];
    char data[512];
    char url[64];
    FILE *f;
    int listen_probe;
    int port;
    int control;
    int data_fd;
    unsigned int a, b, c, d, p1, p2;
    size_t used = 0;

    if (!mkdtemp(dir)) {
        expect("ftp temp dir", 0);
        return;
    }
    snprintf(file, sizeof(file), "%s/hello.txt", dir);
    f = fopen(file, "wb");
    expect("ftp fixture", f != NULL);
    if (f) {
        fputs("hello ftp", f);
        fclose(f);
    }

    listen_probe = ol_net_tcp_listen(0, 1);
    port = listen_probe >= 0 ? ol_net_tcp_local_port(listen_probe) : 0;
    if (listen_probe >= 0)
        ol_net_tcp_close(listen_probe);
    snprintf(url, sizeof(url), "%d", port);
    setenv("OPENLUA_FTP_PORT", url, 1);

    expect("ftp start", ftp_start(dir) == 1);
    expect("ftp state on", ftp_state() == 1);

    control = ol_net_tcp_connect("127.0.0.1", port, 2000);
    expect("ftp connect", control >= 0);
    if (control < 0) {
        ftp_stop();
        return;
    }

    expect("ftp greeting", recv_line(control, reply, sizeof(reply)));
    expect("ftp greeting code", strncmp(reply, "220", 3) == 0);
    expect("ftp user", ftp_command(control, "USER anonymous", reply, sizeof(reply)));
    expect("ftp user code", strncmp(reply, "230", 3) == 0);
    expect("ftp pwd", ftp_command(control, "PWD", reply, sizeof(reply)));
    expect_str("ftp pwd value", reply, "257 \"/\"");
    expect("ftp type", ftp_command(control, "TYPE I", reply, sizeof(reply)));
    expect("ftp type code", strncmp(reply, "200", 3) == 0);

    expect("ftp pasv", ftp_command(control, "PASV", reply, sizeof(reply)));
    expect("ftp pasv parse",
           sscanf(reply, "227 Entering Passive Mode (%u,%u,%u,%u,%u,%u)",
                  &a, &b, &c, &d, &p1, &p2) == 6);
    data_fd = ol_net_tcp_connect("127.0.0.1", (int)(p1 * 256u + p2), 2000);
    expect("ftp data connect", data_fd >= 0);
    expect("ftp list", ftp_command(control, "LIST", reply, sizeof(reply)));
    expect("ftp list code", strncmp(reply, "150", 3) == 0);
    if (data_fd >= 0) {
        int n;
        while ((n = ol_net_tcp_recv(data_fd, data + used,
                                    sizeof(data) - 1 - used)) > 0)
            used += (size_t)n;
        data[used] = '\0';
        ol_net_tcp_close(data_fd);
    }
    expect("ftp list contains file", strstr(data, "hello.txt") != NULL);
    expect("ftp list done", recv_line(control, reply, sizeof(reply)));
    expect("ftp list done code", strncmp(reply, "226", 3) == 0);

    expect("ftp pasv retr", ftp_command(control, "PASV", reply, sizeof(reply)));
    expect("ftp pasv retr parse",
           sscanf(reply, "227 Entering Passive Mode (%u,%u,%u,%u,%u,%u)",
                  &a, &b, &c, &d, &p1, &p2) == 6);
    data_fd = ol_net_tcp_connect("127.0.0.1", (int)(p1 * 256u + p2), 2000);
    expect("ftp data retr", data_fd >= 0);
    expect("ftp retr", ftp_command(control, "RETR hello.txt", reply, sizeof(reply)));
    expect("ftp retr code", strncmp(reply, "150", 3) == 0);
    used = 0;
    if (data_fd >= 0) {
        int n;
        while ((n = ol_net_tcp_recv(data_fd, data + used,
                                    sizeof(data) - 1 - used)) > 0)
            used += (size_t)n;
        data[used] = '\0';
        ol_net_tcp_close(data_fd);
    }
    expect_str("ftp retr body", data, "hello ftp");
    expect("ftp retr done", recv_line(control, reply, sizeof(reply)));
    expect("ftp retr done code", strncmp(reply, "226", 3) == 0);

    expect("ftp read-only", ftp_command(control, "STOR x", reply, sizeof(reply)));
    expect("ftp read-only code", strncmp(reply, "550", 3) == 0);
    expect("ftp escape", ftp_command(control, "CWD ../../..", reply, sizeof(reply)));
    expect("ftp escape code", strncmp(reply, "550", 3) == 0);

    expect("ftp quit", ftp_command(control, "QUIT", reply, sizeof(reply)));
    expect("ftp quit code", strncmp(reply, "221", 3) == 0);
    ol_net_tcp_close(control);
    ftp_stop();
    expect("ftp state off", ftp_state() == 0);

    unlink(file);
    rmdir(dir);
}

int main(void)
{
    expect("network acquire", ol_net_acquire() == 1);

    test_urls();
    test_ipv4();
    test_socket_loopback();
    test_http();
    test_http_redirect();
    test_ftp();

    ol_net_release();
    printf("net: %d checks, %d failure(s)\n", checks, failures);
    return failures ? 1 : 0;
}
