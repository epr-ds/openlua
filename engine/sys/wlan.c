/* OpenLua - OneLua-compatible wlan module. */

#include "engine/sys/wlan.h"
#include "engine/net/net.h"
#include "engine/core/olreg.h"

#include <lauxlib.h>
#include <string.h>

#if defined(__PSP__)
#include <pspnet_apctl.h>
#endif

static int g_ready;

static int wlan_ensure(void)
{
    if (g_ready)
        return 1;
    if (!ol_net_acquire())
        return 0;
    g_ready = 1;
    return 1;
}

static void wlan_drop(void)
{
    if (!g_ready)
        return;
    g_ready = 0;
    ol_net_release();
}

static int l_init(lua_State *L)
{
    lua_pushboolean(L, wlan_ensure());
    return 1;
}

static int l_term(lua_State *L)
{
    (void)L;
    wlan_drop();
    return 0;
}

static int l_connect(lua_State *L)
{
    int net = (int)luaL_optinteger(L, 1, 1);
    int was_ready = g_ready;

    if (!wlan_ensure()) {
        lua_pushinteger(L, 0);
        return 1;
    }

#if defined(__PSP__)
    if (ol_net_connected()) {
        lua_pushinteger(L, 1);
        return 1;
    }
    if (sceNetApctlConnect(net) >= 0) {
        unsigned long deadline = ol_net_millis() + 30000UL;
        while (ol_net_millis() < deadline) {
            if (ol_net_connected()) {
                lua_pushinteger(L, 1);
                return 1;
            }
            ol_net_sleep_ms(100);
        }
    }
    if (!was_ready)
        wlan_drop();
    lua_pushinteger(L, 0);
    return 1;
#else
    (void)was_ready;
    (void)net;
    if (!was_ready)
        wlan_drop();
    lua_pushinteger(L, 0);
    return 1;
#endif
}

static int l_disconnect(lua_State *L)
{
    (void)L;
#if defined(__PSP__)
    if (g_ready)
        sceNetApctlDisconnect();
#endif
    wlan_drop();
    return 0;
}

static int l_isconnected(lua_State *L)
{
    (void)L;
    if (!wlan_ensure()) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, ol_net_connected());
    return 1;
}

static int l_status(lua_State *L)
{
    (void)L;
    if (!wlan_ensure()) {
        lua_pushinteger(L, -13);
        return 1;
    }
    lua_pushinteger(L, ol_net_status());
    return 1;
}

static int l_getip(lua_State *L)
{
    char ip[64];

    if (!wlan_ensure() || !ol_net_ip(ip, sizeof(ip))) {
        lua_pushnil(L);
        return 1;
    }
#if !defined(__PSP__)
    /* The host backend has no WiFi profile; expose no address so scripts
     * follow the same not-connected path as a PSP without a connection. */
    lua_pushnil(L);
    return 1;
#else
    lua_pushstring(L, ip);
    return 1;
#endif
}

static int l_strength(lua_State *L)
{
    (void)L;
    if (!wlan_ensure()) {
        lua_pushinteger(L, 0);
        return 1;
    }
    lua_pushinteger(L, ol_net_strength());
    return 1;
}

static int l_over(lua_State *L)
{
    char name[128];

    if (!wlan_ensure() || !ol_net_profile_name(name, sizeof(name))) {
        lua_pushstring(L, "");
        return 1;
    }
    lua_pushstring(L, name);
    return 1;
}

static int l_getconfigs(lua_State *L)
{
    /* PSPSDK has no user-mode API that enumerates the saved network
     * profiles; the dialog owns that list.  Return an empty table rather than
     * pretending there are entries. */
    (void)L;
    lua_newtable(L);
    return 1;
}

static int l_scan(lua_State *L)
{
    /* The PSP WLAN driver only exposes a power/switch query to user mode;
     * infrastructure scanning is done inside the network dialog. */
    (void)L;
    lua_newtable(L);
    return 1;
}

static int l_autoconnect(lua_State *L)
{
    int mode = (int)luaL_checkinteger(L, 1);
    if (lua_gettop(L) >= 2)
        (void)luaL_checkinteger(L, 2);

    /* The thread-mode auto-connect loop is not implemented.  Validate the
     * arguments and return without changing the connection. */
    (void)mode;
    return 0;
}

static int l_autostatus(lua_State *L)
{
    (void)L;
    lua_pushboolean(L, 0);
    return 1;
}

static const luaL_Reg wlan_funcs[] = {
    { "init",         l_init         },
    { "term",         l_term         },
    { "connect",      l_connect      },
    { "disconnect",   l_disconnect   },
    { "isconnected",  l_isconnected  },
    { "status",       l_status       },
    { "getip",        l_getip        },
    { "strength",     l_strength     },
    { "over",         l_over         },
    { "getconfigs",   l_getconfigs   },
    { "scan",         l_scan         },
    { "autoconnect",  l_autoconnect  },
    { "autostatus",   l_autostatus   },
    { NULL, NULL }
};

void wlan_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, wlan_funcs);
    lua_setglobal(L, "wlan");
}
