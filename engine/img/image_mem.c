/* OpenLua - decode an in-memory image into a normal image userdata.
 *
 * Kept in its own translation unit so the hot PNG loader in image.c is
 * unchanged; the PBP extractor and other data-driven modules call
 * image_push_memory() instead of writing temporary files.
 */

#include "engine/img/image.h"
#include "engine/core/olreg.h"

#include "stb_image.h"

#include <lauxlib.h>

#include <stdlib.h>
#include <string.h>

#define OL_IMAGE "OL.Image"
#define IMAGE_MEMORY_MAX (16u * 1024u * 1024u)

static OL_Surface *surface_from_rgba(const unsigned char *data, int w, int h)
{
    OL_Surface *s = gfx_surface_new(w, h);
    if (!s)
        return NULL;

    for (int y = 0; y < h; y++) {
        const unsigned int *src = (const unsigned int *)data + (size_t)y * w;
        unsigned int *dst = s->pixels + (size_t)y * s->realW;
        memcpy(dst, src, (size_t)w * sizeof(unsigned int));
    }
    return s;
}

int image_push_memory(lua_State *L, const void *data, size_t len)
{
    if (!data || len == 0 || len > IMAGE_MEMORY_MAX)
        return 0;

    int w, h, comp;
    unsigned char *pixels = stbi_load_from_memory((const stbi_uc *)data,
                                                  (int)len, &w, &h, &comp,
                                                  STBI_rgb_alpha);
    if (!pixels)
        return 0;

    OL_Surface *s = surface_from_rgba(pixels, w, h);
    stbi_image_free(pixels);
    if (!s)
        return 0;

    gfx_cache_note();

    OL_Surface **p = (OL_Surface **)lua_newuserdata(L, sizeof(OL_Surface *));
    *p = s;
    luaL_getmetatable(L, OL_IMAGE);
    lua_setmetatable(L, -2);
    return 1;
}
