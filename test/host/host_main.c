/* Host test entry: registers the OpenLua Phase-3 modules into a plain
 * Lua 5.1 state and runs a script (argv[1]) to exercise them.
 * Compiled on the host against the vendored Lua - no PSP anywhere.
 */
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <stdio.h>

#include "engine/fs/files.h"
#include "engine/fs/ini.h"
#include "engine/sys/savedata.h"
#include "engine/util/timers.h"

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s <script.lua>\n", argv[0]);
        return 2;
    }

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    files_api_register(L);
    ini_api_register(L);
    savedata_api_register(L);
    timers_api_register(L);

    if (luaL_dofile(L, argv[1]) != 0)
    {
        fprintf(stderr, "TEST FAILED: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }
    lua_close(L);
    return 0;
}
