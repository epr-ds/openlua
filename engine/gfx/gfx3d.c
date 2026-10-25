/* OpenLua - PSP GE 3D renderer. */

#include <pspkernel.h>
#include <pspgu.h>
#include <pspgum.h>

#include "gfx3d.h"
#include "gfx.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SCR_ASPECT (480.0f / 272.0f)

/* GPU-side vertex layout.  Must follow the order of the vertex flags passed
 * to sceGuDrawArray: texture, colour, position. */
typedef struct {
    float u, v;
    unsigned int color;
    float x, y, z;
} Gpu3DVertex;

static int active;
static int mode2d;
static int renderstyle = 2;
static float fov = 75.0f;
static int fog_enabled;
static float fog_near = 1.0f;
static float fog_far = 100.0f;
static unsigned int fog_color = 0xFF000000u;

static struct {
    float px, py, pz;
    float ex, ey, ez;
    float ux, uy, uz;
    float rx, ry, rz;
} cam = {
    0.0f, 0.0f, 6.0f,      /* position */
    0.0f, 0.0f, 0.0f,      /* look-at */
    0.0f, 1.0f, 0.0f,      /* up */
    0.0f, 0.0f, 0.0f       /* scene rotation */
};

static float deg2rad(float d)
{
    return d * (float)(M_PI / 180.0);
}

static void apply_camera(void)
{
    ScePspFVector3 eye = { cam.px, cam.py, cam.pz };
    ScePspFVector3 center = { cam.ex, cam.ey, cam.ez };
    ScePspFVector3 up = { cam.ux, cam.uy, cam.uz };

    /* A zero-length view direction makes sceGumLookAt produce NaNs.  Nudge
     * the target if a script accidentally sets eye == position. */
    float dx = cam.ex - cam.px;
    float dy = cam.ey - cam.py;
    float dz = cam.ez - cam.pz;
    if (dx * dx + dy * dy + dz * dz < 0.000001f)
        center.z = eye.z + 1.0f;

    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(fov, SCR_ASPECT, 0.5f, 1000.0f);

    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    sceGumLookAt(&eye, &center, &up);

    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGumUpdateMatrix();
}

static void apply_fog(void)
{
    if (fog_enabled) {
        sceGuEnable(GU_FOG);
        sceGuFog(fog_near, fog_far, fog_color);
    } else {
        sceGuDisable(GU_FOG);
    }
}

static void set_3d_state(void)
{
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDepthFunc(GU_LEQUAL);
    sceGuDepthRange(0, 0xFFFF);
    sceGuClearDepth(0xFFFF);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexScale(1.0f, 1.0f);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    apply_fog();
}

static void set_2d_state(void)
{
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_FOG);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexScale(1.0f, 1.0f);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
}

void gfx3d_begin(void)
{
    if (!gfx_gu_started())
        return;

    active = 1;
    mode2d = 0;

    sceGuStart(GU_DIRECT, gfx_list());
    set_3d_state();
    apply_camera();
    /* The previous frame's depth data is irrelevant; clear it now so the GE
     * always starts a 3D scene from a known far plane. */
    sceGuClear(GU_DEPTH_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(0, 0);
}

void gfx3d_update(void)
{
    if (!gfx_gu_started())
        return;
    active = 0;
    mode2d = 0;
    gfx_flip_scene();
}

void gfx3d_mode2d(int on)
{
    if (!gfx_gu_started())
        return;

    mode2d = on ? 1 : 0;
    if (!mode2d)
        active = 1;
    sceGuStart(GU_DIRECT, gfx_list());
    if (mode2d)
        set_2d_state();
    else {
        set_3d_state();
        apply_camera();
    }
    sceGuFinish();
    sceGuSync(0, 0);
}

void gfx3d_finish(void)
{
    if (!gfx_gu_started())
        return;

    active = 0;
    mode2d = 0;
    sceGuStart(GU_DIRECT, gfx_list());
    /* Restore the exact 2D defaults used by gfx_init. */
    set_2d_state();
    sceGuDepthFunc(GU_GEQUAL);
    sceGuClearDepth(0);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDisable(GU_FOG);
    sceGuDisable(GU_CULL_FACE);
    sceGuFinish();
    sceGuSync(0, 0);
}

int gfx3d_active(void)
{
    return active;
}

void gfx3d_set_perspective(float fov_degrees)
{
    if (fov_degrees >= 10.0f && fov_degrees <= 170.0f)
        fov = fov_degrees;
}

void gfx3d_set_renderstyle(int style)
{
    if (style < 0)
        style = 0;
    if (style > 2)
        style = 2;
    renderstyle = style;
}

void gfx3d_set_fog(int enabled, float near_z, float far_z, unsigned int color)
{
    fog_enabled = enabled ? 1 : 0;
    if (far_z > near_z) {
        fog_near = near_z;
        fog_far = far_z;
    }
    fog_color = color;
}

void gfx3d_set_camera(float px, float py, float pz,
                      float ex, float ey, float ez,
                      float ux, float uy, float uz,
                      float rx, float ry, float rz)
{
    cam.px = px; cam.py = py; cam.pz = pz;
    cam.ex = ex; cam.ey = ey; cam.ez = ez;
    cam.ux = ux; cam.uy = uy; cam.uz = uz;
    cam.rx = rx; cam.ry = ry; cam.rz = rz;
}

void gfx3d_draw_batch(const OL_3DVertex *vertices, int count,
                      OL_Surface *texture, int linear_filter,
                      const OL_3DTransform *xf)
{
    if (!gfx_gu_started() || !vertices || count <= 0 || !xf)
        return;

    if (!active || mode2d)
        gfx3d_begin();
    if (!active)
        return;

    sceGuStart(GU_DIRECT, gfx_list());
    set_3d_state();
    apply_camera();

    /* Scene rotation is applied before object transforms, matching the
     * OneLua cam3d.rotation description. */
    ScePspFVector3 scene_rot = {
        deg2rad(cam.rx), deg2rad(cam.ry), deg2rad(cam.rz)
    };
    ScePspFVector3 pos = { xf->px, xf->py, xf->pz };
    ScePspFVector3 obj_rot = {
        deg2rad(xf->rx), deg2rad(xf->ry), deg2rad(xf->rz)
    };
    ScePspFVector3 scale = { xf->sx, xf->sy, xf->sz };

    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    if (cam.rx != 0.0f || cam.ry != 0.0f || cam.rz != 0.0f)
        sceGumRotateXYZ(&scene_rot);
    sceGumTranslate(&pos);
    if (xf->rx != 0.0f || xf->ry != 0.0f || xf->rz != 0.0f)
        sceGumRotateXYZ(&obj_rot);
    if (xf->sx != 1.0f || xf->sy != 1.0f || xf->sz != 1.0f)
        sceGumScale(&scale);
    sceGumUpdateMatrix();

    if (texture && texture->pixels) {
        sceGuEnable(GU_TEXTURE_2D);
        if (linear_filter)
            sceGuTexFilter(GU_LINEAR, GU_LINEAR);
        else
            sceGuTexFilter(GU_NEAREST, GU_NEAREST);
        sceGuTexImage(0, texture->realW, texture->realH,
                      texture->realW, texture->pixels);
    } else {
        sceGuDisable(GU_TEXTURE_2D);
    }

    /* The GE reads vertex data in the order implied by the vertex flags
     * (texture, colour, position), *not* in the order of OL_3DVertex.  Keep
     * the portable struct as-is and repack for submission. */
    Gpu3DVertex *v = (Gpu3DVertex *)sceGuGetMemory(
        (unsigned int)(count * (int)sizeof(Gpu3DVertex)));
    if (v) {
        /* GU_TRANSFORM_3D takes normalised texture coordinates (unlike the
         * texel-space coordinates used by the 2D blitter). */
        for (int i = 0; i < count; i++) {
            v[i].u = vertices[i].u;
            v[i].v = vertices[i].v;
            v[i].color = vertices[i].color;
            v[i].x = vertices[i].x;
            v[i].y = vertices[i].y;
            v[i].z = vertices[i].z;
        }

        int prim = GU_TRIANGLES;
        if (renderstyle == 0)
            prim = GU_POINTS;
        else if (renderstyle == 1)
            prim = GU_LINES;

        sceGuDrawArray(prim, GU_TEXTURE_32BITF | GU_COLOR_8888 |
                       GU_VERTEX_32BITF | GU_TRANSFORM_3D, count, NULL, v);
    }

    sceGuFinish();
    sceGuSync(0, 0);
}
