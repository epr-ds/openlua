#ifndef OL_IMAGE_H
#define OL_IMAGE_H

#include "engine/gfx/gfx.h"
#include <lua.h>
#include <stddef.h>

void image_api_register(lua_State *L);

/* Decode PNG/JPEG/BMP data into a new surface.  Returns NULL on decode or
 * allocation failure.  The caller owns the surface and frees it with
 * gfx_surface_free(). */
OL_Surface *image_surface_from_memory(const void *data, size_t len);

/* Decode PNG/JPEG/BMP data and push it as a normal image userdata.
 * Returns 1 on success, 0 on decode/allocation failure (no Lua error is
 * raised).  Used by the PBP extractor and camera module. */
int image_push_memory(lua_State *L, const void *data, size_t len);

/* Borrow the surface behind an image userdata (validates OL.Image).
 * Errors out if the argument is not an image.  Ownership stays with the
 * image; used by maps to blit tileset frames. */
OL_Surface *image_as_surface(lua_State *L, int idx);

#endif
