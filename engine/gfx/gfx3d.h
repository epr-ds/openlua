/* OpenLua - 3D state and textured triangle rendering for the PSP GE.
 *
 * The 3D modules keep their data portable and call this file only for actual
 * hardware work.  All entry points are no-ops when the GE is not running, so
 * the Lua bindings can still be exercised on the host.
 */

#ifndef OL_GFX3D_H
#define OL_GFX3D_H

#include "engine/gfx/gfx.h"

typedef struct {
    float x, y, z;
    float u, v;
    unsigned int color;     /* ABGR */
} OL_3DVertex;

typedef struct {
    float px, py, pz;
    float rx, ry, rz;       /* degrees */
    float sx, sy, sz;
} OL_3DTransform;

void gfx3d_begin(void);
void gfx3d_update(void);
void gfx3d_mode2d(int on);
void gfx3d_finish(void);
int  gfx3d_active(void);

void gfx3d_set_perspective(float fov_degrees);
void gfx3d_set_renderstyle(int style);       /* 0 points, 1 lines, 2 triangles */
void gfx3d_set_fog(int enabled, float near_z, float far_z, unsigned int color);
void gfx3d_set_camera(float px, float py, float pz,
                      float ex, float ey, float ez,
                      float ux, float uy, float uz,
                      float rx, float ry, float rz);

void gfx3d_draw_batch(const OL_3DVertex *vertices, int count,
                      OL_Surface *texture, int linear_filter,
                      const OL_3DTransform *xf);

#endif
