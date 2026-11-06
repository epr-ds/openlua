/* OpenLua - OneLua-compatible TCP socket module. */

#include "engine/sys/socket.h"
#include "engine/net/net.h"
#include "engine/core/olreg.h"

#include <lauxlib.h>
#include <stdint.h>
#include <stdlib.h>

#define OL_SOCKET_MT "OL.Socket"
#define OL_SOCKET_MAX 64
#define OL_SOCKET_RECV_MAX (1024 * 1024)

typedef struct {
    int fd;
    int refs;
    int closed;
} OlSocket;

static OlSocket *g_sockets[OL_SOCKET_MAX];

static void track_add(OlSocket *s)
{
    int i;
    for (i = 0; i < OL_SOCKET_MAX; i++) {
        if (!g_sockets[i]) {
            g_sockets[i] = s;
            return;
        }
    }
}

static void track_del(OlSocket *s)
{
    int i;
    for (i = 0; i < OL_SOCKET_MAX; i++) {
        if (g_sockets[i] == s)
            g_sockets[i] = NULL;
    }
}

static void socket_release(OlSocket *s)
{
    if (!s || s->closed)
        return;
    s->closed = 1;
    track_del(s);
    ol_net_tcp_close(s->fd);
    s->fd = -1;
    if (s->refs > 0) {
        int n = s->refs;
        s->refs = 0;
        while (n-- > 0)
            ol_net_release();
    }
}

static OlSocket *socket_push(lua_State *L, int fd, int refs)
{
    OlSocket *s = (OlSocket *)lua_newuserdata(L, sizeof(OlSocket));

    s->fd = fd;
    s->refs = refs;
    s->closed = 0;
    track_add(s);

    luaL_getmetatable(L, OL_SOCKET_MT);
    lua_setmetatable(L, -2);
    return s;
}

static OlSocket *socket_check(lua_State *L, int index)
{
    OlSocket *s = (OlSocket *)luaL_checkudata(L, index, OL_SOCKET_MT);
    luaL_argcheck(L, !s->closed, index, "socket is closed");
    return s;
}

static int l_gc(lua_State *L)
{
    OlSocket *s = (OlSocket *)luaL_checkudata(L, 1, OL_SOCKET_MT);
    socket_release(s);
    return 0;
}

static int l_tostring(lua_State *L)
{
    OlSocket *s = (OlSocket *)luaL_checkudata(L, 1, OL_SOCKET_MT);
    lua_pushfstring(L, "OL.Socket(%d%s)", s->fd, s->closed ? ", closed" : "");
    return 1;
}

static int l_server(lua_State *L)
{
    int port = (int)luaL_optinteger(L, 1, 80);
    int fd;

    if (port < 0 || port > 65535)
        return luaL_error(L, "port out of range");
    if (!ol_net_acquire()) {
        lua_pushnil(L);
        lua_pushliteral(L, "network unavailable");
        return 2;
    }
    fd = ol_net_tcp_listen(port, 4);
    if (fd < 0) {
        ol_net_release();
        lua_pushnil(L);
        lua_pushliteral(L, "could not listen");
        return 2;
    }
    socket_push(L, fd, 1);
    return 1;
}

static int l_connect(lua_State *L)
{
    const char *host = luaL_checkstring(L, 1);
    int port = (int)luaL_optinteger(L, 2, 80);
    int fd;

    if (port <= 0 || port > 65535)
        return luaL_error(L, "port out of range");
    if (!ol_net_acquire()) {
        lua_pushnil(L);
        lua_pushliteral(L, "network unavailable");
        return 2;
    }
    fd = ol_net_tcp_connect(host, port, 10000);
    if (fd < 0) {
        ol_net_release();
        lua_pushnil(L);
        lua_pushliteral(L, "connection failed");
        return 2;
    }
    socket_push(L, fd, 1);
    return 1;
}

static int l_accept(lua_State *L)
{
    OlSocket *server = socket_check(L, 1);
    int fd;

    if (!ol_net_acquire()) {
        lua_pushnil(L);
        lua_pushliteral(L, "network unavailable");
        return 2;
    }
    fd = ol_net_tcp_accept(server->fd, -1);
    if (fd < 0) {
        ol_net_release();
        lua_pushnil(L);
        lua_pushliteral(L, "accept failed");
        return 2;
    }
    socket_push(L, fd, 1);
    return 1;
}

static int l_send(lua_State *L)
{
    OlSocket *s = socket_check(L, 1);
    size_t len = 0;
    const char *data = luaL_checklstring(L, 2, &len);
    lua_Integer want = (lua_Integer)len;

    if (lua_gettop(L) >= 3)
        want = luaL_checkinteger(L, 3);
    if (want < 0)
        want = 0;
    if ((size_t)want > len)
        want = (lua_Integer)len;

    lua_pushinteger(L, ol_net_tcp_send_all(s->fd, data, (size_t)want));
    return 1;
}

static int l_recv(lua_State *L)
{
    OlSocket *s = socket_check(L, 1);
    lua_Integer want = luaL_optinteger(L, 2, 1024);
    char *buf;
    int n;

    if (want <= 0)
        want = 1;
    if (want > OL_SOCKET_RECV_MAX)
        want = OL_SOCKET_RECV_MAX;
    buf = (char *)malloc((size_t)want);
    if (!buf)
        return luaL_error(L, "out of memory");

    n = ol_net_tcp_recv(s->fd, buf, (size_t)want);
    if (n < 0) {
        free(buf);
        lua_pushnil(L);
        lua_pushinteger(L, 0);
        return 2;
    }
    lua_pushlstring(L, buf, (size_t)n);
    lua_pushinteger(L, n);
    free(buf);
    return 2;
}

static int l_close(lua_State *L)
{
    OlSocket *s = socket_check(L, 1);
    socket_release(s);
    return 0;
}

static int l_term(lua_State *L)
{
    int i;
    (void)L;
    for (i = 0; i < OL_SOCKET_MAX; i++) {
        if (g_sockets[i])
            socket_release(g_sockets[i]);
    }
    return 0;
}

static const luaL_Reg socket_funcs[] = {
    { "server",  l_server  },
    { "connect", l_connect },
    { "accept",  l_accept  },
    { "send",    l_send    },
    { "recv",    l_recv    },
    { "close",   l_close   },
    { "term",    l_term    },
    { NULL, NULL }
};

static const luaL_Reg socket_meta[] = {
    { "__gc",       l_gc       },
    { "__tostring", l_tostring },
    { NULL, NULL }
};

void socket_api_register(lua_State *L)
{
    luaL_newmetatable(L, OL_SOCKET_MT);
    ol_setfuncs(L, socket_meta);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    lua_newtable(L);
    ol_setfuncs(L, socket_funcs);
    lua_setglobal(L, "socket");
}
