/* OpenLua - OneLua-compatible AMG/3D scene control.
 *
 * The supported subset is deliberately explicit: scene lifetime, perspective,
 * render style, fog and 2D/3D switching.  Lighting, raytracing, culling helpers
 * and screenshots are not registered; docs/API.md lists them as unsupported.
 */

#include "amg.h"
#include "engine/gfx/gfx3d.h"
#include "engine/core/olreg.h"

#if defined(__PSP__)
#include "engine/snd/sound.h"
#endif

#include <lauxlib.h>
#include <lualib.h>

static struct {
    int initialized;
    int quality;
    int renderstyle;
    float fov;
    int fog;
    float fog_near;
    float fog_far;
    unsigned int fog_color;
} g_amg = {
    0, 0, 2, 75.0f, 0, 1.0f, 100.0f, 0xFF000000u
};

static void apply_state(void)
{
#if defined(__PSP__)
    gfx3d_set_perspective(g_amg.fov);
    gfx3d_set_renderstyle(g_amg.renderstyle);
    gfx3d_set_fog(g_amg.fog, g_amg.fog_near, g_amg.fog_far,
                  g_amg.fog_color);
#endif
}

static int l_init(lua_State *L)
{
    (void)L;
    g_amg.initialized = 1;
    apply_state();
    return 0;
}

static int l_quality(lua_State *L)
{
    g_amg.quality = (int)luaL_checkinteger(L, 1);
    return 0;
}

static int l_begin(lua_State *L)
{
    (void)L;
    if (!g_amg.initialized) {
        g_amg.initialized = 1;
        apply_state();
    }
#if defined(__PSP__)
    gfx3d_begin();
#endif
    return 0;
}

static int l_mode2d(lua_State *L)
{
    int state = lua_toboolean(L, 1) ? 1 : 0;
#if defined(__PSP__)
    gfx3d_mode2d(state);
#else
    (void)state;
#endif
    return 0;
}

static int l_update(lua_State *L)
{
    (void)L;
#if defined(__PSP__)
    sound_pump();
    gfx3d_update();
#endif
    return 0;
}

static int l_renderstyle(lua_State *L)
{
    int style = (int)luaL_checkinteger(L, 1);
    if (style < 0)
        style = 0;
    if (style > 2)
        style = 2;
    g_amg.renderstyle = style;
#if defined(__PSP__)
    gfx3d_set_renderstyle(style);
#endif
    return 0;
}

static int l_perspective(lua_State *L)
{
    float fov = (float)luaL_checknumber(L, 1);
    if (fov < 10.0f || fov > 170.0f)
        return luaL_error(L, "amg.perspective: FOV must be 10..170 degrees");
    g_amg.fov = fov;
#if defined(__PSP__)
    gfx3d_set_perspective(fov);
#endif
    return 0;
}

static int l_fog(lua_State *L)
{
    int n = lua_gettop(L);
    if (n == 0) {
        g_amg.fog = 0;
#if defined(__PSP__)
        gfx3d_set_fog(0, g_amg.fog_near, g_amg.fog_far, g_amg.fog_color);
#endif
        return 0;
    }

    float near_z = (float)luaL_checknumber(L, 1);
    float far_z = (float)luaL_checknumber(L, 2);
    unsigned int color = (unsigned int)luaL_checkinteger(L, 3);
    if (far_z <= near_z)
        return luaL_error(L, "amg.fog: far must be greater than near");
    g_amg.fog = 1;
    g_amg.fog_near = near_z;
    g_amg.fog_far = far_z;
    g_amg.fog_color = color;
#if defined(__PSP__)
    gfx3d_set_fog(1, near_z, far_z, color);
#endif
    return 0;
}

static int l_finish(lua_State *L)
{
    (void)L;
    g_amg.initialized = 0;
#if defined(__PSP__)
    gfx3d_finish();
#endif
    return 0;
}

static void push_constant(lua_State *L, const char *name, int value)
{
    lua_pushinteger(L, value);
    lua_setglobal(L, name);
}

static const luaL_Reg amg_funcs[] = {
    { "init",        l_init },
    { "quality",     l_quality },
    { "begin",       l_begin },
    { "mode2d",      l_mode2d },
    { "update",      l_update },
    { "renderstyle", l_renderstyle },
    { "perspective", l_perspective },
    { "fog",         l_fog },
    { "finish",      l_finish },
    { NULL, NULL }
};

void amg_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, amg_funcs);
    lua_setglobal(L, "amg");

    /* Constants used by the documented subset.  The numeric values are the
     * OpenLua/AMGLib convention, not arbitrary internal enum values. */
    push_constant(L, "__8888", 0);
    push_constant(L, "__4444", 1);
    push_constant(L, "__5551", 2);
    push_constant(L, "__5650", 3);
    push_constant(L, "__RAM", 0);
    push_constant(L, "__VRAM", 1);
    push_constant(L, "__POINTS", 0);
    push_constant(L, "__LINES", 1);
    push_constant(L, "__TRIANGLES", 2);
    push_constant(L, "__FRONT", 0);
    push_constant(L, "__TRANS", 1);
    push_constant(L, "__ALL", 2);
    push_constant(L, "__NEAR", 0);
    push_constant(L, "__LINEAR", 1);
    push_constant(L, "__ENV", 0);
    push_constant(L, "__COORDS", 1);
    push_constant(L, "__DIRECTIONAL", 0);
    push_constant(L, "__POINT", 1);
    push_constant(L, "__SPOT", 2);
    push_constant(L, "__BOX", 0);
    push_constant(L, "__SPHERE", 1);
    push_constant(L, "__CONE", 2);
    push_constant(L, "__CYLINDER", 3);
    push_constant(L, "__CONVEX", 4);
}
