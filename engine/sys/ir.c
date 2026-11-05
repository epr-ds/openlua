/* OpenLua - OneLua-compatible infrared (IR) module.
 *
 * See ir.h.  The user-mode build has no raw IR driver, so every call reports
 * the documented "nothing attached" result instead of pretending to send.
 */

#include "engine/sys/ir.h"
#include "engine/core/olreg.h"

#include <lauxlib.h>

static int l_init(lua_State *L)
{
    lua_pushinteger(L, 0);
    return 1;
}

static int l_baud(lua_State *L)
{
    (void)luaL_checkinteger(L, 1);
    return 0;
}

static int l_state(lua_State *L)
{
    (void)L;
    lua_pushboolean(L, 0);
    return 1;
}

static int l_get(lua_State *L)
{
    (void)L;
    lua_pushinteger(L, -1);
    return 1;
}

static int l_put(lua_State *L)
{
    int data = (int)luaL_checkinteger(L, 1);
    if (data < 0)
        data = 0;
    if (data > 255)
        data = 255;
    (void)data;
    lua_pushinteger(L, 0);
    return 1;
}

static int l_read(lua_State *L)
{
    (void)L;
    lua_pushnil(L);
    return 1;
}

static int l_write(lua_State *L)
{
    (void)luaL_checkstring(L, 1);
    lua_pushinteger(L, -1);
    return 1;
}

static int l_term(lua_State *L)
{
    (void)L;
    lua_pushinteger(L, 1);
    return 1;
}

static const luaL_Reg ir_funcs[] = {
    { "init",  l_init  },
    { "baud",  l_baud  },
    { "state", l_state },
    { "get",   l_get   },
    { "put",   l_put   },
    { "read",  l_read  },
    { "write", l_write },
    { "term",  l_term  },
    { NULL, NULL }
};

void ir_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, ir_funcs);
    lua_setglobal(L, "ir");
}
