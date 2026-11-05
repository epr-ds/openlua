/* OpenLua - OneLua-compatible SIO (serial) module.
 *
 * See sio.h.  The user-mode build has no safe SIO syscall, so the hardware
 * hooks are intentionally absent and every call reports the documented
 * "nothing attached" result.
 */

#include "engine/sys/sio.h"
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

static int l_available(lua_State *L)
{
    (void)L;
    lua_pushinteger(L, 0);
    return 1;
}

static int l_read(lua_State *L)
{
    (void)L;
    lua_pushinteger(L, -1);
    return 1;
}

static int l_write(lua_State *L)
{
    int data = (int)luaL_checkinteger(L, 1);
    if (data < 0)
        data = 0;
    if (data > 255)
        data = 255;
    (void)data;
    return 0;
}

static int l_print(lua_State *L)
{
    (void)luaL_checkstring(L, 1);
    return 0;
}

static int l_putstring(lua_State *L)
{
    size_t len = 0;
    const char *data = luaL_checklstring(L, 1, &len);
    lua_Integer want = luaL_checkinteger(L, 2);
    if (want < 0)
        want = 0;
    if ((size_t)want > len)
        want = (lua_Integer)len;
    (void)data;
    (void)want;
    return 0;
}

static int l_term(lua_State *L)
{
    (void)L;
    lua_pushinteger(L, 1);
    return 1;
}

static const luaL_Reg sio_funcs[] = {
    { "init",      l_init      },
    { "baud",      l_baud      },
    { "available", l_available },
    { "read",      l_read      },
    { "write",     l_write     },
    { "print",     l_print     },
    { "putstring", l_putstring },
    { "term",      l_term      },
    { NULL, NULL }
};

void sio_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, sio_funcs);
    lua_setglobal(L, "sio");
}
