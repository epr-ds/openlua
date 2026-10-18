#ifndef OL_REG_H
#define OL_REG_H

#include <lua.h>
#include <lauxlib.h>

void ol_setfuncs(lua_State *L, const luaL_Reg *funcs);

#endif
