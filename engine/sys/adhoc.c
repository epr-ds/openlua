/* OpenLua - OneLua-compatible adhoc module. */

#include "engine/sys/adhoc.h"
#include "engine/net/net.h"
#include "engine/core/olreg.h"

#include <ctype.h>
#include <lauxlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__PSP__)
#include <stdint.h>
#include <pspnet.h>
#include <pspnet_adhoc.h>
#include <pspnet_adhocctl.h>
#include <pspnet_adhocmatching.h>
#include <pspwlan.h>
#endif

#define ADHOC_GAMEID_MAX 8

static int g_ready;
static int g_net_ref;
static char g_name[9];

static void adhoc_name(char *out, size_t cap, const char *gameid)
{
    size_t n = 0;
    const char *p = (gameid && *gameid) ? gameid : "OPENLUA";

    while (*p && n + 1 < cap && n < ADHOC_GAMEID_MAX) {
        char c = *p++;
        if (c >= 'a' && c <= 'z')
            c = (char)toupper((unsigned char)c);
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '_' || c == '-')
            out[n++] = c;
    }
    if (n == 0) {
        snprintf(out, cap, "OPENLUA");
        return;
    }
    out[n] = '\0';
}

static void adhoc_cleanup(void)
{
#if defined(__PSP__)
    if (g_ready) {
        sceNetAdhocctlDisconnect();
        sceNetAdhocctlTerm();
        sceNetAdhocTerm();
    }
#endif
    g_ready = 0;
    if (g_net_ref) {
        g_net_ref = 0;
        ol_net_release();
    }
}

static int adhoc_init_impl(const char *gameid)
{
    if (g_ready)
        return 1;
    if (!ol_net_acquire())
        return 0;
    g_net_ref = 1;
    adhoc_name(g_name, sizeof(g_name), gameid);

#if defined(__PSP__)
    /* Adhoc is unusable while the WLAN switch is off.  Checking first also
     * avoids starting the adhoc control threads only to tear them down again
     * (PPSSPP's HLE can stall during that teardown with WLAN disabled). */
    if (sceWlanGetSwitchState() <= 0) {
        adhoc_cleanup();
        return 0;
    }

    {
        struct productStruct product;
        int event = 0;
        unsigned long deadline;

        if (sceNetAdhocInit() < 0) {
            adhoc_cleanup();
            return 0;
        }
        memset(&product, 0, sizeof(product));
        snprintf(product.product, sizeof(product.product), "%.*s",
                 (int)(sizeof(product.product) - 1), g_name);
        if (sceNetAdhocctlInit(0x2000, 0x30, &product) < 0) {
            sceNetAdhocTerm();
            adhoc_cleanup();
            return 0;
        }
        if (sceNetAdhocctlConnect(g_name) < 0) {
            sceNetAdhocctlTerm();
            sceNetAdhocTerm();
            adhoc_cleanup();
            return 0;
        }
        deadline = ol_net_millis() + 5000UL;
        while (ol_net_millis() < deadline) {
            event = 0;
            if (sceNetAdhocctlGetState(&event) == 0 && event == 1)
                break;
            ol_net_sleep_ms(50);
        }
        if (event != 1) {
            sceNetAdhocctlDisconnect();
            sceNetAdhocctlTerm();
            sceNetAdhocTerm();
            adhoc_cleanup();
            return 0;
        }
        g_ready = 1;
        return 1;
    }
#else
    (void)gameid;
    adhoc_cleanup();
    return 0;
#endif
}

static int l_init(lua_State *L)
{
    const char *gameid = NULL;
    if (lua_gettop(L) >= 1 && !lua_isnil(L, 1))
        gameid = luaL_checkstring(L, 1);
    lua_pushinteger(L, adhoc_init_impl(gameid));
    return 1;
}

static int l_term(lua_State *L)
{
    (void)L;
    adhoc_cleanup();
    return 0;
}

static int l_getmac(lua_State *L)
{
#if defined(__PSP__)
    unsigned char mac[6];
    if (!g_ready || sceNetGetLocalEtherAddr(mac) < 0) {
        lua_pushstring(L, "");
        return 1;
    }
    lua_pushfstring(L, "%02X:%02X:%02X:%02X:%02X:%02X",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#else
    lua_pushstring(L, "");
#endif
    return 1;
}

#if defined(__PSP__)
typedef struct {
    SceNetAdhocctlPeerInfo *peer;
    void *raw;
    int count;
} AdhocPeers;

static int adhoc_peers(AdhocPeers *out)
{
    int size = 8192;
    void *buf;
    int length;
    SceNetAdhocctlPeerInfo *peer;

    memset(out, 0, sizeof(*out));
    if (!g_ready)
        return 0;

    buf = malloc((size_t)size);
    if (!buf)
        return 0;
    length = size;
    if (sceNetAdhocctlGetPeerList(&length, buf) < 0) {
        free(buf);
        return 0;
    }
    out->raw = buf;
    out->peer = (SceNetAdhocctlPeerInfo *)buf;
    for (peer = out->peer; peer; peer = peer->next)
        out->count++;
    return out->count;
}

static void adhoc_peers_free(AdhocPeers *p)
{
    free(p->raw);
    memset(p, 0, sizeof(*p));
}

static void push_peer(lua_State *L, SceNetAdhocctlPeerInfo *peer)
{
    lua_newtable(L);
    lua_pushstring(L, peer->nickname);
    lua_setfield(L, -2, "name");
    lua_pushfstring(L, "%02X:%02X:%02X:%02X:%02X:%02X",
                    peer->mac[0], peer->mac[1], peer->mac[2],
                    peer->mac[3], peer->mac[4], peer->mac[5]);
    lua_setfield(L, -2, "mac");
    lua_pushinteger(L, 1);   /* __JOINED */
    lua_setfield(L, -2, "state");
}
#endif

static int l_scan(lua_State *L)
{
    lua_newtable(L);
#if defined(__PSP__)
    {
        AdhocPeers peers;
        int i = 1;
        SceNetAdhocctlPeerInfo *peer;
        if (!adhoc_peers(&peers))
            return 1;
        for (peer = peers.peer; peer; peer = peer->next) {
            push_peer(L, peer);
            lua_rawseti(L, -2, i++);
        }
        adhoc_peers_free(&peers);
    }
#else
    (void)L;
#endif
    return 1;
}

static int l_gettotal(lua_State *L)
{
#if defined(__PSP__)
    AdhocPeers peers;
    int count = adhoc_peers(&peers);
    adhoc_peers_free(&peers);
    lua_pushinteger(L, count);
#else
    lua_pushinteger(L, 0);
#endif
    return 1;
}

static int l_sendrequest(lua_State *L)
{
    (void)luaL_checkinteger(L, 1);
    if (lua_gettop(L) >= 2)
        (void)luaL_checkinteger(L, 2);
    lua_pushboolean(L, 0);
    return 1;
}

static int l_getrequest(lua_State *L)
{
    (void)L;
    lua_newtable(L);
    return 1;
}

static int l_accept(lua_State *L)
{
    (void)luaL_checkinteger(L, 1);
    return 0;
}

static int l_reject(lua_State *L)
{
    (void)luaL_checkinteger(L, 1);
    return 0;
}

static int l_send(lua_State *L)
{
    size_t len = 0;
    (void)luaL_checkinteger(L, 1);
    (void)luaL_checklstring(L, 2, &len);
    (void)len;
    if (lua_gettop(L) >= 3)
        (void)luaL_checkinteger(L, 3);
    lua_pushinteger(L, 0);
    return 1;
}

static int l_recv(lua_State *L)
{
    (void)luaL_checkinteger(L, 1);
    if (lua_gettop(L) >= 2)
        (void)luaL_checkinteger(L, 2);
    lua_pushnil(L);
    return 1;
}

static const luaL_Reg adhoc_funcs[] = {
    { "init",        l_init        },
    { "term",        l_term        },
    { "getmac",      l_getmac      },
    { "scan",        l_scan        },
    { "gettotal",    l_gettotal    },
    { "sendrequest", l_sendrequest },
    { "getrequest",  l_getrequest  },
    { "accept",      l_accept      },
    { "reject",      l_reject      },
    { "send",        l_send        },
    { "recv",        l_recv        },
    { NULL, NULL }
};

void adhoc_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, adhoc_funcs);
    lua_setglobal(L, "adhoc");
}
