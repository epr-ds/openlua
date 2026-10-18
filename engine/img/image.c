#include "stb_image.h"
#include "stb_image_write.h"

#include "image.h"
#include "engine/gfx/gfx.h"
#include "engine/core/olreg.h"

#include <lauxlib.h>
#include <lualib.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define OL_IMAGE "OL.Image"

static OL_Surface *to_surface(lua_State *L, int idx)
{
    OL_Surface **p = (OL_Surface **)luaL_checkudata(L, idx, OL_IMAGE);
    if (!p || !*p)
        luaL_error(L, "bad image");
    return *p;
}

static void push_surface(lua_State *L, OL_Surface *s)
{
    OL_Surface **p = (OL_Surface **)lua_newuserdata(L, sizeof(OL_Surface *));
    *p = s;
    luaL_getmetatable(L, OL_IMAGE);
    lua_setmetatable(L, -2);
}

static int l_load(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    int frameW = 0, frameH = 0;
    unsigned int mask = 0;
    int hasMask = 0;
    int nargs = lua_gettop(L);

    if (nargs >= 3) {
        frameW = (int)luaL_checkinteger(L, 2);
        frameH = (int)luaL_checkinteger(L, 3);
        if (nargs >= 4) {
            mask = (unsigned int)luaL_checkinteger(L, 4);
            hasMask = 1;
        }
    } else if (nargs == 2) {
        mask = (unsigned int)luaL_checkinteger(L, 2);
        hasMask = 1;
    }

    int w, h, comp;
    stbi_uc *data = stbi_load(path, &w, &h, &comp, STBI_rgb_alpha);
    if (!data)
        return luaL_error(L, "image.load: cannot load '%s'", path);

    OL_Surface *s = gfx_surface_new(w, h);
    if (!s) {
        stbi_image_free(data);
        return luaL_error(L, "image.load: invalid size %dx%d", w, h);
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            unsigned int c = ((unsigned int *)data)[y * w + x];
            if (hasMask && (c & 0xFFFFFF) == (mask & 0xFFFFFF))
                c &= 0x00FFFFFFu;
            s->pixels[y * s->realW + x] = c;
        }
    }
    stbi_image_free(data);

    if (frameW > 0 && frameH > 0) {
        s->frameW = frameW;
        s->frameH = frameH;
    }

    gfx_cache_note();
    push_surface(L, s);
    return 1;
}

static int l_loadv(lua_State *L)
{
    return l_load(L);
}

static int l_new(lua_State *L)
{
    int w = (int)luaL_checkinteger(L, 1);
    int h = (int)luaL_checkinteger(L, 2);
    unsigned int color = (unsigned int)luaL_optinteger(L, 3, 0x00000000);
    OL_Surface *s = gfx_surface_new(w, h);
    if (!s)
        return luaL_error(L, "image.new: invalid size %dx%d", w, h);
    gfx_surface_fill(s, color);
    push_surface(L, s);
    return 1;
}

static void frame_rect(OL_Surface *s, int frame, int *sx, int *sy, int *sw, int *sh)
{
    if (s->frameW > 0 && s->frameH > 0) {
        int cols = s->realW / s->frameW;
        if (cols < 1)
            cols = 1;
        *sx = (frame % cols) * s->frameW;
        *sy = (frame / cols) * s->frameH;
        *sw = s->frameW;
        *sh = s->frameH;
    } else {
        *sx = 0;
        *sy = 0;
        *sw = s->w;
        *sh = s->h;
    }
}

static int l_blit(lua_State *L)
{
    OL_Surface *s = to_surface(L, 1);
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);
    int sx, sy, sw, sh;
    frame_rect(s, 0, &sx, &sy, &sw, &sh);
    int alpha = 255;

    if (lua_gettop(L) >= 8) {
        sx = (int)luaL_checkinteger(L, 4);
        sy = (int)luaL_checkinteger(L, 5);
        sw = (int)luaL_checkinteger(L, 6);
        sh = (int)luaL_checkinteger(L, 7);
        if (lua_gettop(L) >= 8)
            alpha = (int)luaL_checkinteger(L, 8);
    } else if (lua_gettop(L) == 4) {
        alpha = (int)luaL_checkinteger(L, 4);
    }

    gfx_blit(s, x + s->centerOffX, y + s->centerOffY,
             sx, sy, sw, sh, alpha,
             s->flipX, s->flipY, s->angle,
             s->scaleX, s->scaleY, s->z, s->tint);
    return 0;
}

static int l_blitsprite(lua_State *L)
{
    OL_Surface *s = to_surface(L, 1);
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);
    int frame = (int)luaL_checkinteger(L, 4);
    int alpha = (lua_gettop(L) >= 5) ? (int)luaL_checkinteger(L, 5) : 255;
    int sx, sy, sw, sh;
    frame_rect(s, frame, &sx, &sy, &sw, &sh);
    gfx_blit(s, x + s->centerOffX, y + s->centerOffY,
             sx, sy, sw, sh, alpha,
             s->flipX, s->flipY, s->angle,
             s->scaleX, s->scaleY, s->z, s->tint);
    return 0;
}

static int l_fliph(lua_State *L)
{
    OL_Surface *s = to_surface(L, 1);
    s->flipX = (lua_gettop(L) >= 2) ? (int)lua_tointeger(L, 2) : !s->flipX;
    return 0;
}

static int l_flipv(lua_State *L)
{
    OL_Surface *s = to_surface(L, 1);
    s->flipY = (lua_gettop(L) >= 2) ? (int)lua_tointeger(L, 2) : !s->flipY;
    return 0;
}

static int l_getw(lua_State *L)
{
    OL_Surface *s = to_surface(L, 1);
    lua_pushinteger(L, s->w);
    return 1;
}

static int l_geth(lua_State *L)
{
    OL_Surface *s = to_surface(L, 1);
    lua_pushinteger(L, s->h);
    return 1;
}

static int l_getrealw(lua_State *L)
{
    OL_Surface *s = to_surface(L, 1);
    lua_pushinteger(L, s->realW);
    return 1;
}

static int l_getrealh(lua_State *L)
{
    OL_Surface *s = to_surface(L, 1);
    lua_pushinteger(L, s->realH);
    return 1;
}

static int l_center(lua_State *L)
{
    OL_Surface *s = to_surface(L, 1);
    if (lua_gettop(L) >= 3) {
        s->centerOffX = (int)luaL_checkinteger(L, 2);
        s->centerOffY = (int)luaL_checkinteger(L, 3);
    } else {
        s->centerOffX = -(s->w / 2);
        s->centerOffY = -(s->h / 2);
    }
    return 0;
}

static int l_rotate(lua_State *L)
{
    OL_Surface *s = to_surface(L, 1);
    s->angle = (float)luaL_checknumber(L, 2);
    return 0;
}

/* ---- per-blit transform state: scale, z-index, tint ---- */

static int l_scale(lua_State *L)
{
    OL_Surface *s = to_surface(L, 1);
    if (lua_gettop(L) >= 3) {
        s->scaleX = (float)luaL_checknumber(L, 2);
        s->scaleY = (float)luaL_checknumber(L, 3);
    } else if (lua_gettop(L) >= 2) {
        float u = (float)luaL_checknumber(L, 2);
        s->scaleX = u;
        s->scaleY = u;
    }
    return 0;
}

static int l_zindex(lua_State *L)
{
    OL_Surface *s = to_surface(L, 1);
    if (lua_gettop(L) >= 2)
        s->z = (int)luaL_checknumber(L, 2);
    lua_pushinteger(L, s->z);
    return 1;
}

static int l_tint(lua_State *L)
{
    OL_Surface *s = to_surface(L, 1);
    if (lua_gettop(L) >= 2) {
        unsigned int c = (unsigned int)luaL_checkinteger(L, 2);
        s->tint = c;
    } else {
        lua_pushinteger(L, (lua_Integer)s->tint);
        return 1;
    }
    return 0;
}

static int l_pivot(lua_State *L)
{
    OL_Surface *s = to_surface(L, 1);
    if (lua_gettop(L) >= 3) {
        s->pivotX = (int)luaL_checkinteger(L, 2);
        s->pivotY = (int)luaL_checkinteger(L, 3);
    } else {
        /* default = center of frame */
        s->pivotX = s->w / 2;
        s->pivotY = s->h / 2;
    }
    return 0;
}

static int l_reset(lua_State *L)
{
    OL_Surface *s = to_surface(L, 1);
    s->flipX = 0;
    s->flipY = 0;
    s->angle = 0.0f;
    s->centerOffX = 0;
    s->centerOffY = 0;
    return 0;
}

static int l_copy(lua_State *L)
{
    OL_Surface *s = to_surface(L, 1);
    OL_Surface *c = gfx_surface_clone(s);
    if (!c)
        return luaL_error(L, "image.copy failed");
    push_surface(L, c);
    return 1;
}

static int l_clear(lua_State *L)
{
    OL_Surface *s = to_surface(L, 1);
    unsigned int color = (unsigned int)luaL_checkinteger(L, 2);
    gfx_surface_fill(s, color);
    return 0;
}

static int l_pixel(lua_State *L)
{
    OL_Surface *s = to_surface(L, 1);
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);
    if (lua_gettop(L) >= 4) {
        unsigned int color = (unsigned int)luaL_checkinteger(L, 4);
        gfx_surface_pixel_set(s, x, y, color);
        return 0;
    }
    lua_pushinteger(L, (lua_Integer)gfx_surface_pixel_get(s, x, y));
    return 1;
}

static int l_save(lua_State *L)
{
    OL_Surface *s = to_surface(L, 1);
    const char *path = luaL_checkstring(L, 2);
    stbi_uc *flat = (stbi_uc *)malloc((size_t)s->w * s->h * 4);
    if (!flat)
        return luaL_error(L, "image.save: OOM");
    for (int y = 0; y < s->h; y++) {
        memcpy(flat + (size_t)y * s->w * 4,
               &s->pixels[y * s->realW], (size_t)s->w * 4);
    }
    stbi_write_png(path, s->w, s->h, 4, flat, s->w * 4);
    free(flat);
    return 0;
}

static int gc_image(lua_State *L)
{
    OL_Surface **p = (OL_Surface **)luaL_checkudata(L, 1, OL_IMAGE);
    if (p && *p) {
        gfx_surface_free(*p);
        *p = NULL;
    }
    return 0;
}

static const luaL_Reg image_funcs[] = {
    { "load",        l_load },
    { "loadv",       l_loadv },
    { "new",         l_new },
    { "blit",        l_blit },
    { "blitsprite",  l_blitsprite },
    { "fliph",       l_fliph },
    { "flipv",       l_flipv },
    { "getw",        l_getw },
    { "geth",        l_geth },
    { "getrealw",    l_getrealw },
    { "getrealh",    l_getrealh },
    { "center",      l_center },
    { "rotate",      l_rotate },
    { "scale",       l_scale },
    { "zindex",      l_zindex },
    { "tint",        l_tint },
    { "pivot",       l_pivot },
    { "reset",       l_reset },
    { "copy",        l_copy },
    { "clear",       l_clear },
    { "pixel",       l_pixel },
    { "save",        l_save },
    { NULL, NULL }
};

static const luaL_Reg image_meta[] = {
    { "__gc", gc_image },
    { NULL, NULL }
};

void image_api_register(lua_State *L)
{
    luaL_newmetatable(L, OL_IMAGE);
    ol_setfuncs(L, image_meta);
    lua_newtable(L);
    ol_setfuncs(L, image_funcs);
    lua_setglobal(L, "image");
}
