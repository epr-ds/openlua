/* OpenLua - OneLua-compatible 3D texture module.
 *
 * Textures are global 1-based handles so a model can keep using one after the
 * Lua value that created it has gone out of scope.  The actual pixel storage
 * is an ordinary OL_Surface, shared with the 2D image pipeline.
 */

#ifndef OL_TEXTURE3D_H
#define OL_TEXTURE3D_H

#include <lua.h>

#include "engine/gfx/gfx.h"

#define TEXTURE3D_MAX 64

typedef struct {
    int used;
    OL_Surface *surface;
    int location;       /* __RAM / __VRAM; both upload from RAM today */
    int quality;        /* __8888/__4444/__5551/__5650, stored for parity */
    int frames;
    int frame;
    float posx, posy;
    float scalex, scaley;
    float transu, transv;
} OL_Texture3D;

void texture3d_api_register(lua_State *L);

/* Internal loader used by model3d for MTL map_Kd textures. */
int texture3d_load_internal(const char *path, int location);
OL_Texture3D *texture3d_get(int handle);
void texture3d_finish_all(void);

#endif
