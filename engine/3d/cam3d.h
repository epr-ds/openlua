/* OpenLua - OneLua-compatible 3D camera module. */

#ifndef OL_CAM3D_H
#define OL_CAM3D_H

#include <lua.h>

#define CAM3D_MAX 16

typedef struct {
    int used;
    float px, py, pz;
    float ex, ey, ez;
    float ux, uy, uz;
    float rx, ry, rz;
} OL_Cam3D;

void cam3d_api_register(lua_State *L);

#endif
