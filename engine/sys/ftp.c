/* OpenLua - OneLua-compatible FTP file server. */

#include "engine/sys/ftp.h"
#include "engine/net/net.h"
#include "engine/fs/olfs.h"
#include "engine/core/olreg.h"

#include <ctype.h>
#include <lauxlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__PSP__)
#include <pspkernel.h>
#else
#include <pthread.h>
#endif

#define FTP_LINE_MAX   1024
#define FTP_PARTS_MAX  16
#define FTP_PART_LEN   128
#define FTP_LIST_MAX   64
#define FTP_DATA_WAIT  5000

static volatile int g_stop;
static volatile int g_running;
static int g_listen_fd = -1;
static char g_root[512];

#if defined(__PSP__)
static SceUID g_thread = -1;
#else
static pthread_t g_thread;
static int g_thread_valid;
#endif

typedef struct {
    int fd;
    char buf[FTP_LINE_MAX * 2];
    size_t len;
} FtpInput;

static int ftp_resolve(const char *cwd, const char *arg, char *out, size_t cap)
{
    char parts[FTP_PARTS_MAX][FTP_PART_LEN];
    int count = 0;
    const char *p;
    int absolute = arg && arg[0] == '/';
    size_t used = 0;
    int i;

    if (!out || cap == 0)
        return 0;
    out[0] = '\0';

    /* Seed from the current directory unless the argument is absolute. */
    if (!absolute) {
        p = cwd ? cwd : "/";
        while (*p) {
            const char *start;
            size_t n;
            while (*p == '/')
                p++;
            if (!*p)
                break;
            start = p;
            while (*p && *p != '/')
                p++;
            n = (size_t)(p - start);
            if (n == 0 || (n == 1 && start[0] == '.'))
                continue;
            if (n == 2 && start[0] == '.' && start[1] == '.') {
                if (count > 0)
                    count--;
                continue;
            }
            if (n >= FTP_PART_LEN || count >= FTP_PARTS_MAX)
                return 0;
            memcpy(parts[count], start, n);
            parts[count][n] = '\0';
            count++;
        }
    }

    p = arg ? arg : "";
    while (*p) {
        const char *start;
        size_t n;
        while (*p == '/')
            p++;
        if (!*p)
            break;
        start = p;
        while (*p && *p != '/')
            p++;
        n = (size_t)(p - start);
        if (n == 0 || (n == 1 && start[0] == '.'))
            continue;
        if (n == 2 && start[0] == '.' && start[1] == '.') {
            if (count == 0)
                return 0;   /* refuse to escape the root */
            count--;
            continue;
        }
        if (n >= FTP_PART_LEN || count >= FTP_PARTS_MAX)
            return 0;
        memcpy(parts[count], start, n);
        parts[count][n] = '\0';
        count++;
    }

    if (cap < 2)
        return 0;
    out[used++] = '/';
    for (i = 0; i < count; i++) {
        size_t n = strlen(parts[i]);
        if (used + n + (i + 1 < count ? 1 : 0) >= cap)
            return 0;
        memcpy(out + used, parts[i], n);
        used += n;
        if (i + 1 < count)
            out[used++] = '/';
    }
    out[used] = '\0';
    return 1;
}

static int ftp_fs_path(const char *rel, char *out, size_t cap)
{
    int n;
    if (!rel || rel[0] != '/')
        return 0;
    if (rel[1] == '\0')
        n = snprintf(out, cap, "%s", g_root);
    else
        n = snprintf(out, cap, "%s%s", g_root, rel);
    return n > 0 && (size_t)n < cap;
}

static int send_text(int fd, const char *text)
{
    return ol_net_tcp_send_all(fd, text, strlen(text)) >= 0;
}

static int ftp_read_line(FtpInput *in, char *out, size_t cap)
{
    for (;;) {
        size_t i;
        for (i = 0; i < in->len; i++) {
            if (in->buf[i] == '\n') {
                size_t n = i;
                size_t copy;
                if (n && in->buf[n - 1] == '\r')
                    n--;
                copy = n < cap - 1 ? n : cap - 1;
                memcpy(out, in->buf, copy);
                out[copy] = '\0';
                memmove(in->buf, in->buf + i + 1, in->len - i - 1);
                in->len -= i + 1;
                return 1;
            }
        }
        if (in->len >= sizeof(in->buf))
            return 0;

        {
            int r = ol_net_tcp_wait(in->fd, 1000);
            int n;
            if (r < 0)
                return 0;
            if (r == 0) {
                if (g_stop)
                    return 0;
                continue;
            }
            n = ol_net_tcp_recv(in->fd, in->buf + in->len,
                                sizeof(in->buf) - in->len);
            if (n <= 0)
                return 0;
            in->len += (size_t)n;
        }
    }
}

static void trim_arg(char *line, char **arg)
{
    char *p = line;
    while (*p && *p != ' ' && *p != '\t')
        p++;
    if (*p) {
        *p++ = '\0';
        while (*p == ' ' || *p == '\t')
            p++;
    }
    *arg = p;
}

static void uppercase(char *s)
{
    for (; *s; s++)
        *s = (char)toupper((unsigned char)*s);
}

static int open_data(int *listen_fd, unsigned short *port)
{
    int fd = ol_net_tcp_listen(0, 1);
    int p;
    if (fd < 0)
        return 0;
    p = ol_net_tcp_local_port(fd);
    if (p <= 0) {
        ol_net_tcp_close(fd);
        return 0;
    }
    *listen_fd = fd;
    *port = (unsigned short)p;
    return 1;
}

static int accept_data(int listen_fd)
{
    int fd = ol_net_tcp_accept(listen_fd, FTP_DATA_WAIT);
    if (fd < 0 && !g_stop) {
        /* Retry once: some clients connect a moment after PASV. */
        fd = ol_net_tcp_accept(listen_fd, FTP_DATA_WAIT);
    }
    return fd;
}

static void serve_client(int fd)
{
    FtpInput in;
    char line[FTP_LINE_MAX];
    char cwd[512] = "/";
    int data_listen = -1;
    unsigned short data_port = 0;
    char reply[512];

    memset(&in, 0, sizeof(in));
    in.fd = fd;

    send_text(fd, "220 OpenLua FTP ready\r\n");

    while (!g_stop && ftp_read_line(&in, line, sizeof(line))) {
        char *arg;
        char cmd[32];
        size_t clen;

        trim_arg(line, &arg);
        snprintf(cmd, sizeof(cmd), "%.*s", (int)(sizeof(cmd) - 1), line);
        uppercase(cmd);
        clen = strlen(cmd);

        if (strcmp(cmd, "USER") == 0 || strcmp(cmd, "PASS") == 0 ||
            strcmp(cmd, "ACCT") == 0) {
            send_text(fd, "230 Anonymous access allowed\r\n");
        } else if (strcmp(cmd, "SYST") == 0) {
            send_text(fd, "215 UNIX Type: L8\r\n");
        } else if (strcmp(cmd, "FEAT") == 0) {
            send_text(fd, "211-Features:\r\n UTF8\r\n PASV\r\n EPSV\r\n"
                          " SIZE\r\n REST STREAM\r\n211 End\r\n");
        } else if (strcmp(cmd, "OPTS") == 0) {
            send_text(fd, "200 UTF8 enabled\r\n");
        } else if (strcmp(cmd, "PWD") == 0 || strcmp(cmd, "XPWD") == 0) {
            snprintf(reply, sizeof(reply), "257 \"%s\"\r\n", cwd);
            send_text(fd, reply);
        } else if (strcmp(cmd, "CWD") == 0 || strcmp(cmd, "XCWD") == 0) {
            char rel[512];
            char fs[768];
            if (ftp_resolve(cwd, arg, rel, sizeof(rel)) &&
                ftp_fs_path(rel, fs, sizeof(fs)) && olfs_isdir(fs)) {
                snprintf(cwd, sizeof(cwd), "%s", rel);
                send_text(fd, "250 Directory changed\r\n");
            } else {
                send_text(fd, "550 No such directory\r\n");
            }
        } else if (strcmp(cmd, "CDUP") == 0 || strcmp(cmd, "XCUP") == 0) {
            char rel[512];
            if (ftp_resolve(cwd, "..", rel, sizeof(rel))) {
                snprintf(cwd, sizeof(cwd), "%s", rel);
                send_text(fd, "250 Directory changed\r\n");
            } else {
                send_text(fd, "250 Directory changed\r\n");
            }
        } else if (strcmp(cmd, "TYPE") == 0 || strcmp(cmd, "MODE") == 0 ||
                   strcmp(cmd, "STRU") == 0) {
            send_text(fd, "200 Type set\r\n");
        } else if (strcmp(cmd, "PASV") == 0) {
            char ip[64];
            unsigned int a, b, c, d;
            if (data_listen >= 0) {
                ol_net_tcp_close(data_listen);
                data_listen = -1;
            }
            if (!open_data(&data_listen, &data_port)) {
                send_text(fd, "425 Cannot open passive port\r\n");
                continue;
            }
            if (!ol_net_ip(ip, sizeof(ip)))
                snprintf(ip, sizeof(ip), "127.0.0.1");
            if (sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
                a = 127; b = 0; c = 0; d = 1;
            }
            snprintf(reply, sizeof(reply),
                     "227 Entering Passive Mode (%u,%u,%u,%u,%u,%u)\r\n",
                     a, b, c, d, (unsigned)(data_port >> 8),
                     (unsigned)(data_port & 0xff));
            send_text(fd, reply);
        } else if (strcmp(cmd, "EPSV") == 0) {
            if (data_listen >= 0) {
                ol_net_tcp_close(data_listen);
                data_listen = -1;
            }
            if (!open_data(&data_listen, &data_port)) {
                send_text(fd, "425 Cannot open passive port\r\n");
                continue;
            }
            snprintf(reply, sizeof(reply),
                     "229 Entering Extended Passive Mode (|||%u|)\r\n",
                     (unsigned)data_port);
            send_text(fd, reply);
        } else if (strcmp(cmd, "PORT") == 0) {
            send_text(fd, "502 Active mode is not supported; use PASV\r\n");
        } else if (strcmp(cmd, "LIST") == 0 || strcmp(cmd, "NLST") == 0) {
            static OLEntry entries[FTP_LIST_MAX];
            char rel[512];
            char fs[768];
            int count = 0, trunc = 0, i;
            int dfd;
            int nlst = strcmp(cmd, "NLST") == 0;

            if (data_listen < 0) {
                send_text(fd, "425 Use PASV first\r\n");
                continue;
            }
            if (!ftp_resolve(cwd, (arg && arg[0] == '-') ? "" : arg,
                             rel, sizeof(rel)) ||
                !ftp_fs_path(rel, fs, sizeof(fs)) || !olfs_isdir(fs)) {
                send_text(fd, "550 No such directory\r\n");
                ol_net_tcp_close(data_listen);
                data_listen = -1;
                continue;
            }
            dfd = accept_data(data_listen);
            ol_net_tcp_close(data_listen);
            data_listen = -1;
            if (dfd < 0) {
                if (!g_stop)
                    send_text(fd, "425 Cannot open data connection\r\n");
                continue;
            }
            send_text(fd, "150 Opening ASCII mode data connection.\r\n");
            if (olfs_list(fs, entries, FTP_LIST_MAX, &count, &trunc) == 0) {
                for (i = 0; i < count && !g_stop; i++) {
                    if (nlst) {
                        snprintf(reply, sizeof(reply), "%.*s\r\n",
                                 (int)(sizeof(entries[i].name) - 1),
                                 entries[i].name);
                    } else {
                        snprintf(reply, sizeof(reply),
                                 "%s 1 psp psp %ld Jan 01 00:00 %.*s\r\n",
                                 entries[i].is_dir ? "drwxr-xr-x" : "-rw-r--r--",
                                 entries[i].size,
                                 (int)(sizeof(entries[i].name) - 1),
                                 entries[i].name);
                    }
                    if (!send_text(dfd, reply))
                        break;
                }
            }
            ol_net_tcp_close(dfd);
            send_text(fd, "226 Transfer complete.\r\n");
        } else if (strcmp(cmd, "SIZE") == 0) {
            char rel[512];
            char fs[768];
            long size;
            if (!ftp_resolve(cwd, arg, rel, sizeof(rel)) ||
                !ftp_fs_path(rel, fs, sizeof(fs)) ||
                (size = olfs_size(fs)) < 0) {
                send_text(fd, "550 No such file\r\n");
            } else {
                snprintf(reply, sizeof(reply), "213 %ld\r\n", size);
                send_text(fd, reply);
            }
        } else if (strcmp(cmd, "RETR") == 0) {
            char rel[512];
            char fs[768];
            FILE *file;
            int dfd;
            char buf[4096];
            size_t n;

            if (data_listen < 0) {
                send_text(fd, "425 Use PASV first\r\n");
                continue;
            }
            if (!ftp_resolve(cwd, arg, rel, sizeof(rel)) ||
                !ftp_fs_path(rel, fs, sizeof(fs)) || olfs_isdir(fs) ||
                !(file = fopen(fs, "rb"))) {
                send_text(fd, "550 No such file\r\n");
                ol_net_tcp_close(data_listen);
                data_listen = -1;
                continue;
            }
            dfd = accept_data(data_listen);
            ol_net_tcp_close(data_listen);
            data_listen = -1;
            if (dfd < 0) {
                fclose(file);
                if (!g_stop)
                    send_text(fd, "425 Cannot open data connection\r\n");
                continue;
            }
            send_text(fd, "150 Opening BINARY mode data connection.\r\n");
            while (!g_stop && (n = fread(buf, 1, sizeof(buf), file)) > 0) {
                if (ol_net_tcp_send_all(dfd, buf, n) < 0)
                    break;
            }
            fclose(file);
            ol_net_tcp_close(dfd);
            send_text(fd, "226 Transfer complete.\r\n");
        } else if (strcmp(cmd, "QUIT") == 0) {
            send_text(fd, "221 Goodbye\r\n");
            break;
        } else if (strcmp(cmd, "NOOP") == 0 || strcmp(cmd, "ABOR") == 0) {
            send_text(fd, "200 OK\r\n");
        } else if (strcmp(cmd, "STOR") == 0 || strcmp(cmd, "APPE") == 0 ||
                   strcmp(cmd, "DELE") == 0 || strcmp(cmd, "MKD") == 0 ||
                   strcmp(cmd, "RMD") == 0 || strcmp(cmd, "RNFR") == 0 ||
                   strcmp(cmd, "RNTO") == 0 || strcmp(cmd, "SITE") == 0) {
            send_text(fd, "550 Read-only server\r\n");
        } else if (clen == 0) {
            send_text(fd, "500 Empty command\r\n");
        } else {
            send_text(fd, "502 Command not implemented\r\n");
        }
    }

    if (data_listen >= 0)
        ol_net_tcp_close(data_listen);
}

#if defined(__PSP__)
static int ftp_thread_main(SceSize args, void *arg)
#else
static void *ftp_thread_main(void *arg)
#endif
{
#if defined(__PSP__)
    (void)args;
#endif
    (void)arg;

    while (!g_stop) {
        int fd = ol_net_tcp_accept(g_listen_fd, 500);
        if (fd < 0) {
            if (g_stop)
                break;
            continue;
        }
        serve_client(fd);
        ol_net_tcp_close(fd);
    }

    if (g_listen_fd >= 0) {
        ol_net_tcp_close(g_listen_fd);
        g_listen_fd = -1;
    }
#if defined(__PSP__)
    return 0;
#else
    return NULL;
#endif
}

int ftp_start(const char *root)
{
    const char *env;
    int port = 21;

    if (g_running)
        return 1;

    snprintf(g_root, sizeof(g_root), "%s", (root && *root) ? root : ".");
    env = getenv("OPENLUA_FTP_PORT");
    if (env && *env) {
        int value = atoi(env);
        if (value > 0 && value <= 65535)
            port = value;
    }

    if (!ol_net_acquire())
        return 0;

    g_stop = 0;
    g_listen_fd = ol_net_tcp_listen(port, 2);
    if (g_listen_fd < 0) {
        ol_net_release();
        return 0;
    }

#if defined(__PSP__)
    g_thread = sceKernelCreateThread("openlua_ftp", ftp_thread_main, 0x20,
                                     0x10000, 0, NULL);
    if (g_thread < 0) {
        ol_net_tcp_close(g_listen_fd);
        g_listen_fd = -1;
        ol_net_release();
        return 0;
    }
    if (sceKernelStartThread(g_thread, 0, NULL) < 0) {
        sceKernelDeleteThread(g_thread);
        g_thread = -1;
        ol_net_tcp_close(g_listen_fd);
        g_listen_fd = -1;
        ol_net_release();
        return 0;
    }
#else
    if (pthread_create(&g_thread, NULL, ftp_thread_main, NULL) != 0) {
        ol_net_tcp_close(g_listen_fd);
        g_listen_fd = -1;
        ol_net_release();
        return 0;
    }
    g_thread_valid = 1;
#endif

    g_running = 1;
    return 1;
}

void ftp_stop(void)
{
    if (!g_running)
        return;

    g_stop = 1;
    if (g_listen_fd >= 0) {
        ol_net_tcp_close(g_listen_fd);
        g_listen_fd = -1;
    }

#if defined(__PSP__)
    if (g_thread >= 0) {
        sceKernelWaitThreadEnd(g_thread, NULL);
        sceKernelDeleteThread(g_thread);
        g_thread = -1;
    }
#else
    if (g_thread_valid) {
        pthread_join(g_thread, NULL);
        g_thread_valid = 0;
    }
#endif

    g_running = 0;
    ol_net_release();
}

int ftp_state(void)
{
    return g_running != 0;
}

static int l_init(lua_State *L)
{
    lua_pushboolean(L, ftp_start("."));
    return 1;
}

static int l_term(lua_State *L)
{
    (void)L;
    ftp_stop();
    lua_pushboolean(L, 1);
    return 1;
}

static int l_state(lua_State *L)
{
    (void)L;
    lua_pushboolean(L, ftp_state());
    return 1;
}

static const luaL_Reg ftp_funcs[] = {
    { "init",  l_init  },
    { "term",  l_term  },
    { "state", l_state },
    { NULL, NULL }
};

void ftp_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, ftp_funcs);
    lua_setglobal(L, "ftp");
}
