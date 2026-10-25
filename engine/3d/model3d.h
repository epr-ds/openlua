/* OpenLua - OneLua-compatible 3D model module. */

#ifndef OL_MODEL3D_H
#define OL_MODEL3D_H

#include <lua.h>

#include "engine/3d/obj.h"
#include "engine/gfx/gfx3d.h"

#define MODEL3D_MAX 16
#define MODEL3D_MAX_TEXTURES 16

typedef struct {
    OL_3DVertex *vertices;
    int count;
    int capacity;
    int material;       /* OBJ material index, or -1 */
    int texture;        /* texture3d handle, 0 means untextured */
    int filter;         /* 0 nearest, 1 linear */
} OL_Model3DBatch;

typedef struct {
    float px, py, pz;
    float rx, ry, rz;
    float sx, sy, sz;
    int first_batch;
    int batch_count;
    int texture_override;   /* texture3d handle, 0 = material default */
    int filter;             /* -1 = material/default, 0 near, 1 linear */
} OL_Model3DObject;

typedef struct {
    int used;
    OL_ObjModel obj;
    OL_Model3DObject *objects;
    int object_count;
    OL_Model3DBatch *batches;
    int batch_count;
    int *material_textures; /* texture3d handles per OBJ material */
    int material_count;
    int css;
} OL_Model3D;

void model3d_api_register(lua_State *L);

/* Used by texture3d.setcustom and texture3d.filter.  Returns 0 on success. */
int model3d_set_texture(int model, int object, int slot, int texture);
int model3d_set_filter(int model, int object, int filter);

#endif
