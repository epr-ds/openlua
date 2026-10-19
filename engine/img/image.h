#ifndef OL_IMAGE_H
#define OL_IMAGE_H

#include "engine/gfx/gfx.h"
#include <lua.h>

void image_api_register(lua_State *L);

/* Borrow the surface behind an image userdata (validates OL.Image).
 * Errors out if the argument is not an image.  Ownership stays with the
 * image; used by maps to blit tileset frames. */
OL_Surface *image_as_surface(lua_State *L, int idx);

#endif
