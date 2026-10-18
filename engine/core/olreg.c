#include "olreg.h"

void ol_setfuncs(lua_State *L, const luaL_Reg *funcs)
{
    for (; funcs && funcs->name; funcs++) {
        lua_pushcfunction(L, funcs->func);
        lua_setfield(L, -2, funcs->name);
    }
}
