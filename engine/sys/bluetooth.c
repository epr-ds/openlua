/* OpenLua - Bluetooth compatibility stub. */

#include "engine/sys/bluetooth.h"
#include "engine/core/olreg.h"

#include <lauxlib.h>

static int l_init(lua_State *L)
{
    (void)L;
    lua_pushboolean(L, 0);
    return 1;
}

static int l_term(lua_State *L)
{
    (void)L;
    lua_pushboolean(L, 0);
    return 1;
}

static int l_state(lua_State *L)
{
    (void)L;
    lua_pushboolean(L, 0);
    return 1;
}

static int l_available(lua_State *L)
{
    (void)L;
    lua_pushboolean(L, 0);
    return 1;
}

static const luaL_Reg bluetooth_funcs[] = {
    { "init",      l_init      },
    { "term",      l_term      },
    { "state",     l_state     },
    { "available", l_available },
    { NULL, NULL }
};

void bluetooth_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, bluetooth_funcs);
    lua_setglobal(L, "bluetooth");
}
