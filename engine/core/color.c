/* OpenLua - OneLua color helpers
 *
 * `color.new(r, g, b [, a])` builds an ABGR word, matching the engine tint
 * format.  OneLua also exposes named colors and `color.loadpalette()`; the
 * latter is a no-op here because OpenLua colors are direct ABGR values.
 */

#include "engine/core/color.h"

#include <lauxlib.h>

#include "engine/core/olreg.h"

/* ABGR, matching color.new(r, g, b, a) and the gfx tint format. */
#define OL_COLOR(r, g, b, a) \
    ((lua_Integer)(((unsigned int)(a) << 24) | ((unsigned int)(b) << 16) | \
                   ((unsigned int)(g) << 8) | (unsigned int)(r)))

static int l_color_new(lua_State *L)
{
    unsigned int r = (unsigned int)(luaL_checkinteger(L, 1) & 0xFF);
    unsigned int g = (unsigned int)(luaL_checkinteger(L, 2) & 0xFF);
    unsigned int b = (unsigned int)(luaL_checkinteger(L, 3) & 0xFF);
    unsigned int a = (lua_gettop(L) > 3)
                     ? (unsigned int)(luaL_checkinteger(L, 4) & 0xFF)
                     : 255;
    lua_pushinteger(L, (lua_Integer)((a << 24) | (b << 16) | (g << 8) | r));
    return 1;
}

static int l_color_loadpalette(lua_State *L)
{
    (void)L;
    return 0;
}

static void set_color_field(lua_State *L, const char *name, lua_Integer abgr)
{
    lua_pushinteger(L, abgr);
    lua_setfield(L, -2, name);
}

static const luaL_Reg color_funcs[] = {
    { "new",         l_color_new },
    { "loadpalette", l_color_loadpalette },
    { NULL,          NULL }
};

void color_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, color_funcs);

    set_color_field(L, "transparent", OL_COLOR(0, 0, 0, 0));
    set_color_field(L, "black",       OL_COLOR(0, 0, 0, 255));
    set_color_field(L, "white",       OL_COLOR(255, 255, 255, 255));
    set_color_field(L, "red",         OL_COLOR(255, 0, 0, 255));
    set_color_field(L, "green",       OL_COLOR(0, 255, 0, 255));
    set_color_field(L, "blue",        OL_COLOR(0, 0, 255, 255));
    set_color_field(L, "yellow",      OL_COLOR(255, 255, 0, 255));
    set_color_field(L, "cyan",        OL_COLOR(0, 255, 255, 255));
    set_color_field(L, "magenta",     OL_COLOR(255, 0, 255, 255));
    set_color_field(L, "orange",      OL_COLOR(255, 128, 0, 255));
    set_color_field(L, "gray",        OL_COLOR(128, 128, 128, 255));
    set_color_field(L, "grey",        OL_COLOR(128, 128, 128, 255));
    set_color_field(L, "lightgray",   OL_COLOR(192, 192, 192, 255));
    set_color_field(L, "lightgrey",   OL_COLOR(192, 192, 192, 255));
    set_color_field(L, "darkgray",    OL_COLOR(64, 64, 64, 255));
    set_color_field(L, "darkgrey",    OL_COLOR(64, 64, 64, 255));

    lua_setglobal(L, "color");
}
