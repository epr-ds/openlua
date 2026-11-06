/* OpenLua - shared networking helpers. */

#include "engine/net/net.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__PSP__)

#include <pspkernel.h>
#include <pspnet.h>
#include <pspnet_apctl.h>
#include <pspnet_inet.h>
#include <pspnet_resolver.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>

#define OL_ACQUIRE_STACK (128 * 1024)

#else /* host: POSIX sockets */

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#endif

static int g_refs;
static int g_inited;
static int g_apctl_ok;
static int g_resolver_ok;

#if !defined(__PSP__)
static unsigned long host_millis(void);
#endif

unsigned long ol_net_millis(void)
{
#if defined(__PSP__)
    return (unsigned long)(sceKernelGetSystemTimeWide() / 1000);
#else
    return host_millis();
#endif
}

void ol_net_sleep_ms(unsigned int ms)
{
#if defined(__PSP__)
    sceKernelDelayThread(ms * 1000);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

#if !defined(__PSP__)
static unsigned long host_millis(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long)ts.tv_sec * 1000UL + (unsigned long)(ts.tv_nsec / 1000000L);
}
#endif

int ol_net_acquire(void)
{
    if (g_refs > 0) {
        g_refs++;
        return g_inited;
    }

    g_refs = 1;
    g_inited = 0;
    g_apctl_ok = 0;
    g_resolver_ok = 0;

#if defined(__PSP__)
    if (sceNetInit(OL_ACQUIRE_STACK, 0x20, 0x1000, 0x20, 0x1000) < 0) {
        g_refs = 0;
        return 0;
    }
    if (sceNetInetInit() < 0) {
        sceNetTerm();
        g_refs = 0;
        return 0;
    }
    if (sceNetApctlInit(0x8000, 0x20) == 0)
        g_apctl_ok = 1;
    if (sceNetResolverInit() == 0)
        g_resolver_ok = 1;
#endif

    g_inited = 1;
    return 1;
}

void ol_net_release(void)
{
    if (g_refs <= 0)
        return;
    if (--g_refs > 0)
        return;

#if defined(__PSP__)
    if (g_resolver_ok)
        sceNetResolverTerm();
    if (g_apctl_ok)
        sceNetApctlTerm();
    if (g_inited) {
        sceNetInetTerm();
        sceNetTerm();
    }
#endif

    g_inited = 0;
    g_apctl_ok = 0;
    g_resolver_ok = 0;
}

int ol_net_acquired(void)
{
    return g_inited;
}

int ol_net_status(void)
{
#if defined(__PSP__)
    int state = 0;
    if (!g_inited || !g_apctl_ok)
        return 0;
    if (sceNetApctlGetState(&state) < 0)
        return -13;
    return state;
#else
    (void)g_apctl_ok;
    return 0;
#endif
}

int ol_net_connected(void)
{
    return ol_net_status() == 4;
}

int ol_net_ip(char *buf, size_t cap)
{
    if (!buf || cap == 0)
        return 0;
    buf[0] = '\0';

#if defined(__PSP__)
    SceNetApctlInfo info;
    if (!g_inited || !g_apctl_ok)
        return 0;
    if (ol_net_status() != 4)
        return 0;
    memset(&info, 0, sizeof(info));
    if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_IP, &info) < 0)
        return 0;
    snprintf(buf, cap, "%s", info.ip);
    return buf[0] != '\0';
#else
    if (!g_inited)
        return 0;
    snprintf(buf, cap, "127.0.0.1");
    return 1;
#endif
}

int ol_net_profile_name(char *buf, size_t cap)
{
    if (!buf || cap == 0)
        return 0;
    buf[0] = '\0';

#if defined(__PSP__)
    SceNetApctlInfo info;
    if (!g_inited || !g_apctl_ok)
        return 0;
    memset(&info, 0, sizeof(info));
    if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_PROFILE_NAME, &info) < 0)
        return 0;
    snprintf(buf, cap, "%s", info.name);
    return buf[0] != '\0';
#else
    if (!g_inited)
        return 0;
    return 0;
#endif
}

int ol_net_strength(void)
{
#if defined(__PSP__)
    SceNetApctlInfo info;
    if (!g_inited || !g_apctl_ok || ol_net_status() != 4)
        return 0;
    memset(&info, 0, sizeof(info));
    if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_STRENGTH, &info) < 0)
        return 0;
    return (int)info.strength;
#else
    return 0;
#endif
}

int ol_net_parse_ipv4(const char *text, unsigned int *out)
{
    unsigned int parts[4];
    const char *p = text;
    int i;

    if (!text || !out)
        return 0;

    for (i = 0; i < 4; i++) {
        unsigned int value = 0;
        int digits = 0;

        while (*p >= '0' && *p <= '9') {
            value = value * 10u + (unsigned int)(*p - '0');
            if (value > 255u || ++digits > 3)
                return 0;
            p++;
        }
        if (digits == 0)
            return 0;
        parts[i] = value;
        if (i < 3) {
            if (*p != '.')
                return 0;
            p++;
        }
    }
    if (*p != '\0')
        return 0;

    *out = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
    return 1;
}

void ol_net_format_ipv4(unsigned int addr, char *buf, size_t cap)
{
    if (!buf || cap == 0)
        return;
    snprintf(buf, cap, "%u.%u.%u.%u",
             (addr >> 24) & 0xffu, (addr >> 16) & 0xffu,
             (addr >> 8) & 0xffu, addr & 0xffu);
}

int ol_net_resolve(const char *host, char *ip, size_t cap)
{
    unsigned int addr;

    if (!host || !ip || cap == 0)
        return 0;
    ip[0] = '\0';

    if (ol_net_parse_ipv4(host, &addr)) {
        ol_net_format_ipv4(addr, ip, cap);
        return 1;
    }

#if defined(__PSP__)
    {
        int rid;
        char work[1024];
        struct in_addr in;

        if (!g_inited || !g_resolver_ok)
            return 0;
        if (sceNetResolverCreate(&rid, work, sizeof(work)) < 0)
            return 0;
        if (sceNetResolverStartNtoA(rid, host, &in, 5, 3) < 0) {
            sceNetResolverDelete(rid);
            return 0;
        }
        sceNetResolverDelete(rid);
        ol_net_format_ipv4((unsigned int)ntohl(in.s_addr), ip, cap);
        return 1;
    }
#else
    {
        struct addrinfo hints;
        struct addrinfo *res = NULL;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(host, NULL, &hints, &res) != 0 || !res)
            return 0;
        if (inet_ntop(AF_INET, &((struct sockaddr_in *)res->ai_addr)->sin_addr,
                      ip, (socklen_t)cap) == NULL) {
            freeaddrinfo(res);
            return 0;
        }
        freeaddrinfo(res);
        return 1;
    }
#endif
}

static int ol_socket(int domain, int type, int protocol)
{
#if defined(__PSP__)
    return sceNetInetSocket(domain, type, protocol);
#else
    return socket(domain, type, protocol);
#endif
}

static int ol_bind(int fd, const struct sockaddr *addr, socklen_t len)
{
#if defined(__PSP__)
    return sceNetInetBind(fd, addr, len);
#else
    return bind(fd, addr, len);
#endif
}

static int ol_listen(int fd, int backlog)
{
#if defined(__PSP__)
    return sceNetInetListen(fd, backlog);
#else
    return listen(fd, backlog);
#endif
}

static int ol_accept(int fd, struct sockaddr *addr, socklen_t *len)
{
#if defined(__PSP__)
    return sceNetInetAccept(fd, addr, len);
#else
    return accept(fd, addr, len);
#endif
}

static int ol_setsockopt(int fd, int level, int name, const void *val, socklen_t len)
{
#if defined(__PSP__)
    return sceNetInetSetsockopt(fd, level, name, val, len);
#else
    return setsockopt(fd, level, name, val, len);
#endif
}

static int ol_select(int n, fd_set *readfds, fd_set *writefds, struct timeval *tv)
{
#if defined(__PSP__)
    SceNetInetTimeval stv;
    SceNetInetTimeval *p = NULL;
    if (tv) {
        stv.tv_sec = (uint32_t)tv->tv_sec;
        stv.tv_usec = (uint32_t)tv->tv_usec;
        p = &stv;
    }
    return sceNetInetSelect(n, readfds, writefds, NULL, p);
#else
    return select(n, readfds, writefds, NULL, tv);
#endif
}

int ol_net_tcp_connect(const char *host, int port, int timeout_ms)
{
    char ip[64];
    unsigned int addr;
    struct sockaddr_in sin;
    int fd;
    int rc;

    if (!host || port <= 0 || port > 65535)
        return -1;
    if (!ol_net_resolve(host, ip, sizeof(ip)))
        return -1;
    if (!ol_net_parse_ipv4(ip, &addr))
        return -1;

    fd = ol_socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((unsigned short)port);
    sin.sin_addr.s_addr = htonl(addr);

#if defined(__PSP__)
    (void)timeout_ms;
    rc = sceNetInetConnect(fd, (struct sockaddr *)&sin, sizeof(sin));
    if (rc < 0) {
        sceNetInetClose(fd);
        return -1;
    }
#else
    {
        int flags = fcntl(fd, F_GETFL, 0);
        fd_set wfds;
        struct timeval tv;
        int err;

        if (flags >= 0)
            (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        rc = connect(fd, (struct sockaddr *)&sin, sizeof(sin));
        if (rc < 0 && errno != EINPROGRESS) {
            close(fd);
            return -1;
        }
        if (rc < 0) {
            FD_ZERO(&wfds);
            FD_SET(fd, &wfds);
            tv.tv_sec = (timeout_ms > 0 ? timeout_ms : 10000) / 1000;
            tv.tv_usec = ((timeout_ms > 0 ? timeout_ms : 10000) % 1000) * 1000;
            rc = select(fd + 1, NULL, &wfds, NULL, &tv);
            if (rc <= 0) {
                close(fd);
                return -1;
            }
            err = 0;
            {
                socklen_t len = sizeof(err);
                if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
                    close(fd);
                    return -1;
                }
            }
        }
        if (flags >= 0)
            (void)fcntl(fd, F_SETFL, flags);
    }
#endif

    return fd;
}

int ol_net_tcp_listen(int port, int backlog)
{
    struct sockaddr_in sin;
    int fd;
    int one = 1;

    if (port < 0 || port > 65535)
        return -1;

    fd = ol_socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    ol_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((unsigned short)port);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);

    if (ol_bind(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0 ||
        ol_listen(fd, backlog > 0 ? backlog : 4) < 0) {
        ol_net_tcp_close(fd);
        return -1;
    }
    return fd;
}

int ol_net_tcp_accept(int fd, int timeout_ms)
{
    fd_set rfds;
    struct timeval tv;
    struct timeval *ptv = NULL;
    int rc;

    if (fd < 0)
        return -1;

    if (timeout_ms >= 0) {
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        ptv = &tv;
    }

    rc = ol_select(fd + 1, &rfds, NULL, ptv);
    if (rc <= 0)
        return -1;
    return ol_accept(fd, NULL, NULL);
}

int ol_net_tcp_send(int fd, const void *data, size_t len)
{
    if (fd < 0 || !data)
        return -1;
#if defined(__PSP__)
    return (int)sceNetInetSend(fd, data, len, 0);
#else
    {
        ssize_t n = send(fd, data, len, 0);
        return n < 0 ? -1 : (int)n;
    }
#endif
}

int ol_net_tcp_send_all(int fd, const void *data, size_t len)
{
    const unsigned char *p = (const unsigned char *)data;
    size_t done = 0;

    if (fd < 0 || (!data && len))
        return -1;
    while (done < len) {
        int n = ol_net_tcp_send(fd, p + done, len - done);
        if (n <= 0)
            return -1;
        done += (size_t)n;
    }
    return (int)done;
}

int ol_net_tcp_recv(int fd, void *buf, size_t len)
{
    if (fd < 0 || !buf || len == 0)
        return -1;
#if defined(__PSP__)
    return (int)sceNetInetRecv(fd, buf, len, 0);
#else
    {
        ssize_t n = recv(fd, buf, len, 0);
        return n < 0 ? -1 : (int)n;
    }
#endif
}

int ol_net_tcp_peek(int fd, void *buf, size_t len)
{
    if (fd < 0 || !buf || len == 0)
        return -1;
#if defined(__PSP__)
    return (int)sceNetInetRecv(fd, buf, len, MSG_PEEK);
#else
    {
        ssize_t n = recv(fd, buf, len, MSG_PEEK);
        return n < 0 ? -1 : (int)n;
    }
#endif
}

int ol_net_tcp_wait(int fd, int timeout_ms)
{
    fd_set rfds;
    struct timeval tv;
    struct timeval *ptv = NULL;

    if (fd < 0)
        return -1;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        ptv = &tv;
    }
    return ol_select(fd + 1, &rfds, NULL, ptv);
}

void ol_net_tcp_close(int fd)
{
    if (fd < 0)
        return;
#if defined(__PSP__)
    sceNetInetClose(fd);
#else
    close(fd);
#endif
}

int ol_net_tcp_local_port(int fd)
{
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);

    if (fd < 0)
        return -1;
    memset(&sin, 0, sizeof(sin));
#if defined(__PSP__)
    if (sceNetInetGetsockname(fd, (struct sockaddr *)&sin, &len) < 0)
        return -1;
#else
    if (getsockname(fd, (struct sockaddr *)&sin, &len) < 0)
        return -1;
#endif
    return (int)ntohs(sin.sin_port);
}
