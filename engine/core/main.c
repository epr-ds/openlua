#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspgu.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "engine/gfx/gfx.h"
#include "engine/img/image.h"
#include "engine/gfx/draw.h"
#include "engine/input/buttons.h"
#include "engine/util/timers.h"

#include "engine/font/font.h"
PSP_MODULE_INFO("OpenLua", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(20000);

static FILE *g_log = NULL;

/* ---- flash-based early-boot trace (always mounted, never fails) ---- */

/* ---- flash-based boot trace (works on real PSP + PPSSPP) ---- */

static int flash_write(const char *data, int len)
{
    FILE *f = fopen("flash0:/openlua_boot.txt", "a");
    if (!f) f = fopen("app0:/boot_trace.bin", "wb"); /* fallback: app dir */
    if (!f) f = fopen("/mnt/flash0/openlua_boot.txt", "a");
    if (f) { fwrite(data, 1, (size_t)len, f); fclose(f); }

    return 0;
}


static void debug_note(const char *msg)
{
    if (!g_log)
        g_log = fopen("openlua.log", "w");
    if (g_log) {
        fputs(msg, g_log);
        fputc('\n', g_log);
        fflush(g_log);
    }
}

static int exit_callback(int arg1, int arg2, void *common)
{
    (void)arg1; (void)arg2; (void)common;
    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void *argp)
{
    (void)args; (void)argp;
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static int setup_callbacks(void)
{
    int thid = sceKernelCreateThread("openlua_exit", callback_thread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0)
        sceKernelStartThread(thid, 0, NULL);
    return thid;
}

static int l_color_new(lua_State *L)
{
    unsigned int r = (unsigned int)(luaL_checkinteger(L, 1) & 0xFF);
    unsigned int g = (unsigned int)(luaL_checkinteger(L, 2) & 0xFF);
    unsigned int b = (unsigned int)(luaL_checkinteger(L, 3) & 0xFF);
    unsigned int a = (lua_gettop(L) > 3) ? (unsigned int)(luaL_checkinteger(L, 4) & 0xFF) : 255;
    lua_pushinteger(L, (lua_Integer)((a << 24) | (b << 16) | (g << 8) | r));
    return 1;
}

static int l_screen_clear(lua_State *L)
{
    gfx_set_clear_color((unsigned int)luaL_checkinteger(L, 1));
    return 0;
}

static int l_screen_flip(lua_State *L)
{
    static int frames = 0;
    gfx_flip_scene();
    if (++frames % 120 == 1)
        debug_note("screen.flip ~120 frames");
    return 0;
}

static void register_api(lua_State *L)
{
    lua_createtable(L, 0, 1);
    lua_pushcfunction(L, l_color_new);
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "color");

    lua_createtable(L, 0, 2);
    lua_pushcfunction(L, l_screen_clear);
    lua_setfield(L, -2, "clear");
    lua_pushcfunction(L, l_screen_flip);
    lua_setfield(L, -2, "flip");
    lua_setglobal(L, "screen");

    image_api_register(L);
    draw_api_register(L);
    buttons_api_register(L);
    timers_api_register(L);
    font_api_register(L);   /* font table + screen.print / screen.font hooks */
}

static void chdir_to_appdir(const char *argv0)
{
    debug_note("boot: enter chdir_to_appdir");
    if (!argv0) {
        debug_note("boot: no argv0");
        return;
    }
    debug_note(argv0);
    const char *slash = strrchr(argv0, '/');
    if (!slash || slash == argv0)
        return;
    size_t len = (size_t)(slash - argv0);
    char dir[512];
    if (len >= sizeof(dir))
        return;
    memcpy(dir, argv0, len);
    dir[len] = '\0';
    if (chdir(dir) != 0)
        debug_note("boot: chdir failed");
    else
        debug_note("boot: cwd set");
}

static const char *find_script(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        if (f) {
            fclose(f);
            return argv[i];
        }
    }
    static const char *candidates[] = { "script.lua", "SCRIPT.LUA", "boot.lua" };
    for (int i = 0; i < 3; i++) {
        FILE *f = fopen(candidates[i], "rb");
        if (f) {
            fclose(f);
            return candidates[i];
        }
    }
    (void)argc;
    return NULL;
}

static void fatal_show(const char *err)
{
    debug_note("boot: fatal");
    debug_note(err ? err : "(unknown)");
    sceGuTerm();
    pspDebugScreenInit();
    pspDebugScreenPrintf("OpenLua 1.0\n");
    pspDebugScreenPrintf("Fatal: %s\n", err ? err : "(unknown)");
    while (1) {
        sceDisplayWaitVblankStart();
        sceKernelDelayThread(1000);
    }
}

int main(int argc, char **argv)
{
    flash_write("boot:main  ", 12);
    // boot trace initialized
    debug_note("boot: main");
    setup_callbacks();
    chdir_to_appdir(argc > 0 ? argv[0] : NULL);
    debug_note("boot: gfx init...");
    flash_write("boot:gfx   ", 12);
    gfx_init();
    debug_note("boot: gfx ok");
    flash_write("boot:ok    ", 12);

    lua_State *L = luaL_newstate();
    if (!L)
        fatal_show("lua state");

    luaL_openlibs(L);
    register_api(L);

    const char *script = find_script(argc, argv);
    if (!script) {
        fatal_show("no script.lua found in app dir");
    }
    debug_note("boot: script found");
    flash_write("boot:script ", 12);
    debug_note(script);

    if (luaL_dofile(L, script) != 0) {
        const char *err = lua_tostring(L, -1);
        debug_note("boot: lua error");
        debug_note(err ? err : "(unknown Lua error)");
        lua_close(L);
        fatal_show(err ? err : "(unknown Lua error)");
    }
    debug_note("boot: script returned");

    lua_close(L);
    sceKernelExitGame();
    return 0;
}
