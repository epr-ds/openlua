/* OpenLua - OneLua-compatible 3D camera handles.
 *
 * The camera is just state.  cam3d.set() copies it into the GE renderer; this
 * matches OneLua's separation between editing the camera and applying it.
 */

#include "cam3d.h"
#include "engine/gfx/gfx3d.h"
#include "engine/core/olreg.h"

#include <lauxlib.h>
#include <lualib.h>

#include <string.h>

static OL_Cam3D g_cameras[CAM3D_MAX];

static OL_Cam3D *to_camera(lua_State *L, int idx)
{
    int h = (int)luaL_checkinteger(L, idx);
    if (h < 1 || h > CAM3D_MAX || !g_cameras[h - 1].used)
        luaL_error(L, "invalid cam3d handle");
    return &g_cameras[h - 1];
}

static int l_new(lua_State *L)
{
    (void)L;
    for (int i = 0; i < CAM3D_MAX; i++) {
        if (!g_cameras[i].used) {
            memset(&g_cameras[i], 0, sizeof(g_cameras[i]));
            g_cameras[i].used = 1;
            g_cameras[i].pz = 6.0f;
            g_cameras[i].uy = 1.0f;
            lua_pushinteger(L, i + 1);
            return 1;
        }
    }
    return luaL_error(L, "cam3d.new: camera slots full");
}

static float vec_number(lua_State *L, int idx, const char *key, int fallback,
                        float def)
{
    lua_getfield(L, idx, key);
    if (lua_isnumber(L, -1)) {
        float v = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
        return v;
    }
    lua_pop(L, 1);
    lua_rawgeti(L, idx, fallback);
    if (lua_isnumber(L, -1)) {
        float v = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
        return v;
    }
    lua_pop(L, 1);
    return def;
}

static void read_vec3(lua_State *L, int idx, float *x, float *y, float *z)
{
    luaL_checktype(L, idx, LUA_TTABLE);
    *x = vec_number(L, idx, "x", 1, 0.0f);
    *y = vec_number(L, idx, "y", 2, 0.0f);
    *z = vec_number(L, idx, "z", 3, 0.0f);
}

static int l_position(lua_State *L)
{
    OL_Cam3D *c = to_camera(L, 1);
    read_vec3(L, 2, &c->px, &c->py, &c->pz);
    return 0;
}

static int l_rotation(lua_State *L)
{
    OL_Cam3D *c = to_camera(L, 1);
    read_vec3(L, 2, &c->rx, &c->ry, &c->rz);
    return 0;
}

static int l_eye(lua_State *L)
{
    OL_Cam3D *c = to_camera(L, 1);
    read_vec3(L, 2, &c->ex, &c->ey, &c->ez);
    return 0;
}

static int l_up(lua_State *L)
{
    OL_Cam3D *c = to_camera(L, 1);
    read_vec3(L, 2, &c->ux, &c->uy, &c->uz);
    return 0;
}

static int l_set(lua_State *L)
{
    OL_Cam3D *c = to_camera(L, 1);
#if defined(__PSP__)
    gfx3d_set_camera(c->px, c->py, c->pz,
                     c->ex, c->ey, c->ez,
                     c->ux, c->uy, c->uz,
                     c->rx, c->ry, c->rz);
#else
    (void)c;
#endif
    return 0;
}

static int l_free(lua_State *L)
{
    OL_Cam3D *c = to_camera(L, 1);
    memset(c, 0, sizeof(*c));
    return 0;
}

static const luaL_Reg cam3d_funcs[] = {
    { "new",      l_new },
    { "position", l_position },
    { "rotation", l_rotation },
    { "eye",      l_eye },
    { "up",       l_up },
    { "set",      l_set },
    { "free",     l_free },
    { NULL, NULL }
};

void cam3d_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, cam3d_funcs);
    lua_setglobal(L, "cam3d");
}
