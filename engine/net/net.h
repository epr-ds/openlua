/* OpenLua - shared networking helpers.
 *
 * A small portability layer over the PSP network stack and POSIX sockets.
 * Modules acquire a network reference for as long as they need the stack and
 * release it when they stop, so creating a socket does not tear down a WiFi
 * connection owned by another module.
 */

#ifndef OL_NET_H
#define OL_NET_H

#include <stddef.h>

/* Acquire/release the shared network stack.  The first acquire initialises
 * sceNet/sceNetInet (and apctl/resolver when available); the last release
 * tears them down.  Returns 1 on success, 0 when networking is unavailable. */
int ol_net_acquire(void);
void ol_net_release(void);
int ol_net_acquired(void);

/* Connection state (OneLua wlan.status values). */
int ol_net_connected(void);
int ol_net_status(void);
int ol_net_strength(void);
int ol_net_ip(char *buf, size_t cap);
int ol_net_profile_name(char *buf, size_t cap);

/* TCP helpers.  All return -1 on failure. */
int ol_net_tcp_connect(const char *host, int port, int timeout_ms);
int ol_net_tcp_listen(int port, int backlog);
int ol_net_tcp_accept(int fd, int timeout_ms);
int ol_net_tcp_send(int fd, const void *data, size_t len);
int ol_net_tcp_send_all(int fd, const void *data, size_t len);
int ol_net_tcp_recv(int fd, void *buf, size_t len);
int ol_net_tcp_peek(int fd, void *buf, size_t len);
int ol_net_tcp_wait(int fd, int timeout_ms);
void ol_net_tcp_close(int fd);
int ol_net_tcp_local_port(int fd);

/* DNS.  Accepts dotted IPv4 or a hostname; writes the numeric address. */
int ol_net_resolve(const char *host, char *ip, size_t cap);
int ol_net_parse_ipv4(const char *text, unsigned int *out);
void ol_net_format_ipv4(unsigned int addr, char *buf, size_t cap);

/* Milliseconds since an arbitrary point, used for timeouts. */
unsigned long ol_net_millis(void);
void ol_net_sleep_ms(unsigned int ms);

#endif
