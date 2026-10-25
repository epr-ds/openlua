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
#include "engine/data/json.h"
#include "engine/font/font.h"
#include "engine/snd/sound.h"
#include "engine/sys/savedata.h"
#include "engine/sys/battery.h"
#include "engine/sys/power.h"
#include "engine/sys/os.h"
#include "engine/sys/hardware.h"
#include "engine/util/olmath.h"
#include "engine/util/timers.h"
#include "engine/3d/amg.h"
#include "engine/3d/cam3d.h"
#include "engine/3d/model3d.h"
#include "engine/3d/texture3d.h"

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
    json_api_register(L);

    /* The PSP core creates `screen` before the font hooks are installed.
     * Mirror that here so screen.print / screen.textwidth are testable. */
    lua_newtable(L);
    lua_setglobal(L, "screen");
    font_api_register(L);
    savedata_api_register(L);
    battery_api_register(L);
    power_api_register(L);
    os_api_register(L);
    hardware_api_register(L);
    sound_api_register(L);
    timers_api_register(L);
    olmath_api_register(L);

    amg_api_register(L);
    cam3d_api_register(L);
    texture3d_api_register(L);
    model3d_api_register(L);

    if (luaL_dofile(L, argv[1]) != 0)
    {
        fprintf(stderr, "TEST FAILED: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }
    lua_close(L);
    return 0;
}
