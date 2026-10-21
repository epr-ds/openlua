#include "olmath.h"
#include "engine/core/olreg.h"

#include <lauxlib.h>

/* OneLua's math helpers use 32-bit integer semantics.  Lua 5.1 stores
 * numbers as doubles, so accept any integral value in [-2^31, 2^32-1] and
 * return the signed 32-bit result. */
static unsigned int check_bits(lua_State *L, int idx)
{
    lua_Number n = luaL_checknumber(L, idx);
    long long v = (long long)n;

    if (n != (lua_Number)v ||
        v < -2147483648LL || v > 4294967295LL) {
        luaL_error(L, "math: bitwise operand must be a 32-bit integer");
        return 0;
    }
    return (unsigned int)v;
}

static int push_bits(lua_State *L, unsigned int bits)
{
    lua_pushinteger(L, (lua_Integer)(int)bits);
    return 1;
}

static int l_minmax(lua_State *L)
{
    lua_Number x   = luaL_checknumber(L, 1);
    lua_Number min = luaL_checknumber(L, 2);
    lua_Number max = luaL_checknumber(L, 3);

    if (x < min)
        x = min;
    if (x > max)
        x = max;
    lua_pushnumber(L, x);
    return 1;
}

static int l_bitnot(lua_State *L)
{
    return push_bits(L, ~check_bits(L, 1));
}

static int l_bitand(lua_State *L)
{
    return push_bits(L, check_bits(L, 1) & check_bits(L, 2));
}

static int l_bitor(lua_State *L)
{
    return push_bits(L, check_bits(L, 1) | check_bits(L, 2));
}

static const luaL_Reg math_funcs[] = {
    { "minmax", l_minmax },
    { "bitnot", l_bitnot },
    { "bitand", l_bitand },
    { "bitor",  l_bitor  },
    { NULL, NULL }
};

void olmath_api_register(lua_State *L)
{
    /* Extend the standard library rather than replacing it: scripts must keep
     * math.floor, math.sqrt, math.pi and friends. */
    lua_getglobal(L, "math");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
    }
    ol_setfuncs(L, math_funcs);
    lua_setglobal(L, "math");
}
