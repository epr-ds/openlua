/* OpenLua - 3D texture handles and PNG loading. */

#include "texture3d.h"
#include "model3d.h"
#include "engine/gfx/gfx.h"
#include "engine/core/olreg.h"

#include "third_party/stb/stb_image.h"

#include <lauxlib.h>
#include <lualib.h>

#include <stdlib.h>
#include <string.h>

static OL_Texture3D g_textures[TEXTURE3D_MAX];

static int claim_texture(void)
{
    for (int i = 0; i < TEXTURE3D_MAX; i++) {
        if (!g_textures[i].used) {
            memset(&g_textures[i], 0, sizeof(g_textures[i]));
            g_textures[i].used = 1;
            g_textures[i].frames = 1;
            g_textures[i].scalex = 1.0f;
            g_textures[i].scaley = 1.0f;
            return i + 1;
        }
    }
    return 0;
}

OL_Texture3D *texture3d_get(int handle)
{
    if (handle < 1 || handle > TEXTURE3D_MAX)
        return NULL;
    return g_textures[handle - 1].used ? &g_textures[handle - 1] : NULL;
}

int texture3d_load_internal(const char *path, int location)
{
    if (!path || !*path)
        return 0;

    int w = 0, h = 0, comp = 0;
    stbi_uc *data = stbi_load(path, &w, &h, &comp, STBI_rgb_alpha);
    if (!data)
        return 0;
    if (w <= 0 || h <= 0 || w > 512 || h > 512) {
        stbi_image_free(data);
        return 0;
    }

    OL_Surface *s = gfx_surface_new(w, h);
    if (!s) {
        stbi_image_free(data);
        return 0;
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            unsigned int c = ((unsigned int *)data)[y * w + x];
            s->pixels[y * s->realW + x] = c;
        }
    }
    stbi_image_free(data);

    int handle = claim_texture();
    if (!handle) {
        gfx_surface_free(s);
        return 0;
    }

    OL_Texture3D *t = &g_textures[handle - 1];
    t->surface = s;
    t->location = location;
#if defined(__PSP__)
    /* CPU-written texture memory must be visible before the GE reads it. */
    gfx_cache_note();
#endif
    return handle;
}

static int l_load(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    int location = (int)luaL_optinteger(L, 2, 0);
    int handle = texture3d_load_internal(path, location);
    if (!handle)
        return luaL_error(L, "texture3d.load: cannot load '%s'", path);
    lua_pushinteger(L, handle);
    return 1;
}

static int l_new(lua_State *L)
{
    int w = (int)luaL_checkinteger(L, 1);
    int h = (int)luaL_checkinteger(L, 2);
    int location = (int)luaL_optinteger(L, 3, 0);
    if (w <= 0 || h <= 0 || w > 512 || h > 512)
        return luaL_error(L, "texture3d.new: invalid size %dx%d", w, h);

    OL_Surface *s = gfx_surface_new(w, h);
    if (!s)
        return luaL_error(L, "texture3d.new: out of memory");

    int handle = claim_texture();
    if (!handle) {
        gfx_surface_free(s);
        return luaL_error(L, "texture3d.new: texture slots full");
    }

    OL_Texture3D *t = &g_textures[handle - 1];
    t->surface = s;
    t->location = location;
    lua_pushinteger(L, handle);
    return 1;
}

static int l_setcustom(lua_State *L)
{
    int model = (int)luaL_checkinteger(L, 1);
    int object = (int)luaL_checkinteger(L, 2);
    int slot = (int)luaL_checkinteger(L, 3);
    int texture = (int)luaL_checkinteger(L, 4);
    if (!texture3d_get(texture))
        return luaL_error(L, "texture3d.setcustom: invalid texture");
    if (model3d_set_texture(model, object, slot, texture) != 0)
        return luaL_error(L, "texture3d.setcustom: invalid model or object");
    return 0;
}

static int l_quality(lua_State *L)
{
    OL_Texture3D *t = texture3d_get((int)luaL_checkinteger(L, 1));
    if (!t)
        return luaL_error(L, "texture3d.quality: invalid texture");
    t->quality = (int)luaL_checkinteger(L, 2);
    return 0;
}

static int l_position(lua_State *L)
{
    OL_Texture3D *t = texture3d_get((int)luaL_checkinteger(L, 1));
    if (!t)
        return luaL_error(L, "texture3d.position: invalid texture");
    t->posx = (float)luaL_checknumber(L, 2);
    t->posy = (float)luaL_checknumber(L, 3);
    return 0;
}

static int l_scaling(lua_State *L)
{
    OL_Texture3D *t = texture3d_get((int)luaL_checkinteger(L, 1));
    if (!t)
        return luaL_error(L, "texture3d.scaling: invalid texture");
    t->scalex = (float)luaL_checknumber(L, 2);
    t->scaley = (float)luaL_checknumber(L, 3);
    return 0;
}

static int l_setframes(lua_State *L)
{
    OL_Texture3D *t = texture3d_get((int)luaL_checkinteger(L, 1));
    if (!t)
        return luaL_error(L, "texture3d.setframes: invalid texture");
    int frames = (int)luaL_checkinteger(L, 2);
    if (frames < 1)
        frames = 1;
    if (frames > 64)
        frames = 64;
    t->frames = frames;
    if (t->frame >= frames)
        t->frame = 0;
    return 0;
}

static int l_frame(lua_State *L)
{
    OL_Texture3D *t = texture3d_get((int)luaL_checkinteger(L, 1));
    if (!t)
        return luaL_error(L, "texture3d.frame: invalid texture");
    int frame = (int)luaL_checkinteger(L, 2);
    if (frame < 0)
        frame = 0;
    if (frame >= t->frames)
        frame = t->frames - 1;
    t->frame = frame;
    return 0;
}

static int l_trans(lua_State *L)
{
    OL_Texture3D *t = texture3d_get((int)luaL_checkinteger(L, 1));
    if (!t)
        return luaL_error(L, "texture3d.trans: invalid texture");
    t->transu = (float)luaL_checknumber(L, 2);
    t->transv = (float)luaL_checknumber(L, 3);
    return 0;
}

static int l_filter(lua_State *L)
{
    int model = (int)luaL_checkinteger(L, 1);
    int object = -1;
    int filter;
    if (lua_gettop(L) >= 3) {
        object = (int)luaL_checkinteger(L, 2);
        filter = (int)luaL_checkinteger(L, 3);
    } else {
        filter = (int)luaL_checkinteger(L, 2);
    }
    if (model3d_set_filter(model, object, filter) != 0)
        return luaL_error(L, "texture3d.filter: invalid model or object");
    return 0;
}

static int l_draw(lua_State *L)
{
    OL_Texture3D *t = texture3d_get((int)luaL_checkinteger(L, 1));
    if (!t || !t->surface)
        return luaL_error(L, "texture3d.draw: invalid texture");

    int frames = t->frames > 0 ? t->frames : 1;
    int sh = t->surface->h / frames;
    if (sh <= 0)
        sh = t->surface->h;
    int sy = t->frame * sh;
    gfx_blit(t->surface, (int)t->posx, (int)t->posy,
             0, sy, t->surface->w, sh,
             255, 0, 0, 0.0f, t->scalex, t->scaley, 0, 0xFFFFFFFFu);
    return 0;
}

static int l_free(lua_State *L)
{
    int handle = (int)luaL_checkinteger(L, 1);
    OL_Texture3D *t = texture3d_get(handle);
    if (t) {
        gfx_surface_free(t->surface);
        memset(t, 0, sizeof(*t));
    }
    return 0;
}

static int l_finish(lua_State *L)
{
    (void)L;
    texture3d_finish_all();
    return 0;
}

static const luaL_Reg texture3d_funcs[] = {
    { "load",      l_load },
    { "new",       l_new },
    { "setcustom", l_setcustom },
    { "quality",   l_quality },
    { "position",  l_position },
    { "scaling",   l_scaling },
    { "setframes", l_setframes },
    { "frame",     l_frame },
    { "trans",     l_trans },
    { "filter",    l_filter },
    { "draw",      l_draw },
    { "free",      l_free },
    { "finish",    l_finish },
    { NULL, NULL }
};

void texture3d_finish_all(void)
{
    for (int i = 0; i < TEXTURE3D_MAX; i++) {
        if (g_textures[i].used) {
            gfx_surface_free(g_textures[i].surface);
            memset(&g_textures[i], 0, sizeof(g_textures[i]));
        }
    }
}

void texture3d_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, texture3d_funcs);
    lua_setglobal(L, "texture3d");
}
