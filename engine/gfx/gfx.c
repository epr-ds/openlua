/* OpenLua - graphic pipeline using PSP GE for sprite rendering.
 * Handles nearest-neighbor upscale, flip X/Y, rotation with pivot center,
 * color tint/modulation, and per-blit z-depth ordering. */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>

#include "gfx.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272

static unsigned int __attribute__((aligned(16))) gu_list[0x40000 / 4];
static int gu_started = 0;
static unsigned int clear_color = 0xFF000000;

/*=========================================================================
 * Helpers
 *=========================================================================*/

static void *vram_alloc(int size)
{
    /* Use long to ensure proper pointer arithmetic on all architectures */
    static long head = 0;
    void *p = (void *)(0x04000000 + head);
    head += (size + 15) & ~15; /* align to 16 bytes */
    return p;
}

static int next_pow2(int v)
{
    int p = 1;
    while (p < v)
        p <<= 1;
    return p;
}

static void flush_gu(void)
{
    sceGuFinish();
    sceGuSync(0, 0);
}

/* ---- GE initialization / scene management (must match main.c usage) ---- */

void gfx_init(void)
{
    int frame = BUF_WIDTH * SCR_HEIGHT * 4;
    void *fb0 = vram_alloc(frame);
    void *fb1 = vram_alloc(frame);
    void *zbd = vram_alloc(frame);

    if (gu_started)
        return;

    sceGuInit();
    sceGuStart(GU_DIRECT, gu_list);
    sceGuDrawBuffer(GU_PSM_8888, fb0, BUF_WIDTH);
    sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, fb1, BUF_WIDTH);
    sceGuDepthBuffer(zbd, BUF_WIDTH);
    sceGuOffset(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
    sceGuDepthRange(0xc350, 0x2710);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
    sceGuFrontFace(GU_CW);
    sceGuEnable(GU_CULL_FACE);
    sceGuShadeModel(GU_SMOOTH);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuAmbientColor(0xffffffff);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuClearColor(clear_color);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
    gu_started = 1;
}

void gfx_set_clear_color(unsigned int c)
{
    clear_color = c;
}

void gfx_flip_scene(void)
{
    if (!gu_started) return;
    sceGuStart(GU_DIRECT, gu_list);
    sceGuClearColor(clear_color);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(0, 0);
    sceGuSwapBuffers();
    sceDisplayWaitVblankStart();
}

int gfx_gu_started(void)
{
    return gu_started;
}

void *gfx_list(void)
{
    return gu_list;
}

/*=========================================================================
 * Surface allocation / pixel ops (no transform)
 *=========================================================================*/

OL_Surface *gfx_surface_new(int w, int h)
{
    if (w <= 0 || h <= 0 || w > 1024 || h > 1024)
        return NULL;
    OL_Surface *s = (OL_Surface *)calloc(1, sizeof(OL_Surface));
    if (!s)
        return NULL;
    s->w = w;
    s->h = h;
    s->realW = next_pow2(w);
    s->realH = next_pow2(h);
    s->frameW = 0;
    s->frameH = 0;
    s->flipX = 0;
    s->flipY = 0;
    s->scaleX = 1.0f;
    s->scaleY = 1.0f;
    s->angle = 0.0f;
    s->pivotX = 0;
    s->pivotY = 0;
    s->z = 0;
    s->centerOffX = 0;
    s->centerOffY = 0;
    s->tint = 0xFFFFFFFFu;
    s->pixels = (unsigned int *)calloc(1, (size_t)s->realW * s->realH * 4);
    if (!s->pixels) {
        free(s);
        return NULL;
    }
    return s;
}

OL_Surface *gfx_surface_clone(OL_Surface *s)
{
    if (!s)
        return NULL;
    OL_Surface *c = gfx_surface_new(s->w, s->h);
    if (!c)
        return NULL;
    c->frameW = s->frameW;
    c->frameH = s->frameH;
    c->flipX = s->flipX;
    c->flipY = s->flipY;
    c->scaleX = s->scaleX;
    c->scaleY = s->scaleY;
    c->angle = s->angle;
    c->pivotX = s->pivotX;
    c->pivotY = s->pivotY;
    c->z = s->z;
    c->centerOffX = s->centerOffX;
    c->centerOffY = s->centerOffY;
    c->tint = s->tint;
    memcpy(c->pixels, s->pixels, (size_t)s->realW * s->realH * 4);
    return c;
}

void gfx_surface_free(OL_Surface *s)
{
    if (!s)
        return;
    free(s->pixels);
    free(s);
}

void gfx_surface_fill(OL_Surface *s, unsigned int color)
{
    int n = s->realW * s->realH;
    for (int i = 0; i < n; i++)
        s->pixels[i] = color;
    FlushCache(2);
}

unsigned int gfx_surface_pixel_get(OL_Surface *s, int x, int y)
{
    if (x < 0 || y < 0 || x >= s->w || y >= s->h)
        return 0;
    /* Flush L2 before read — GE DMA reads uncached memory only */
    FlushCache(2);
    return s->pixels[y * s->realW + x];
}

void gfx_surface_pixel_set(OL_Surface *s, int x, int y, unsigned int c)
{
    if (x < 0 || y < 0 || x >= s->w || y >= s->h)
        return;
    s->pixels[y * s->realW + x] = c;
    FlushCache(2);
}

void gfx_surface_hflip(OL_Surface *s)
{
    for (int y = 0; y < s->h; y++) {
        for (int x = 0; x < s->w / 2; x++) {
            unsigned int *a = &s->pixels[y * s->realW + x];
            unsigned int *b = &s->pixels[y * s->realW + (s->w - 1 - x)];
            unsigned int t = *a;
            *a = *b; *b = t;
        }
    }
    FlushCache(2);
}

void gfx_surface_vflip(OL_Surface *s)
{
    for (int y = 0; y < s->h / 2; y++) {
        unsigned int *a = &s->pixels[y * s->realW];
        unsigned int *b = &s->pixels[(s->h - 1 - y) * s->realW];
        for (int x = 0; x < s->w; x++) {
            unsigned int t = a[x];
            a[x] = b[x];
            b[x] = t;
        }
    }
    FlushCache(2);
}

void gfx_cache_note(void)
{
    FlushCache(2);
}

/*=========================================================================
 * GU type alias + draw helpers
 *=========================================================================*/

typedef unsigned int uint32;

typedef struct {
    float u, v;
    short x, y, z;
    unsigned char r, g, b, a;     /* optional vertex color (set by caller) */
} Vert;

/* Helper: get flipped U endpoints */
static void flip_u(float u, float w, int fliph, float *a, float *b)
{
    if (fliph) { *a = u + w; *b = u; } else { *a = u;     *b = u + w; }
}

/* Helper: get flipped V endpoints */
static void flip_v(float v, float h, int flipv, float *a, float *b)
{
    if (flipv) { *a = v + h; *b = v; } else { *a = v;     *b = v + h; }
}

/* nearest-neighbor CPU upscale + tint -> returns malloc'd buffer, caller must free */
static unsigned int *nearest_scale(const unsigned int *src, int srcW,
                                   int *outW, int *outH, float sx, float sy,
                                   int srcH, unsigned int tint)
{
    *outW = (int)(srcW / sx + 0.5f);
    *outH = (int)(srcH / sy + 0.5f);
    if (*outW <= 0 || *outH <= 0) return NULL;
    unsigned int *dst = (unsigned int *)malloc(*outW * *outH * sizeof(unsigned int));
    if (!dst) return NULL;

    float tr = (tint >> 16) & 0xFFu;
    float tg = (tint >> 8) & 0xFFu;
    float tb = tint & 0xFFu;

    for (int dy = 0; dy < *outH; dy++) {
        int sy2 = (int)(dy / sy);
        if (sy2 >= srcH) sy2 = srcH - 1;
        for (int dx = 0; dx < *outW; dx++) {
            int sx2 = (int)(dx / sx);
            if (sx2 >= srcW) sx2 = srcW - 1;
            unsigned int sp = src[sy2 * srcW + sx2];
            float sa = ((unsigned int)(sp >> 24 & 0xFFu)) & 0xFFu;
            dst[dy * *outW + dx] =
                ((unsigned int)((tr * sa) / 255.0f)) << 16 |
                ((unsigned int)((tg * sa) / 255.0f)) << 8 |
                ((unsigned int)((tb * sa) / 255.0f)) |
                ((unsigned int)sa) << 24;
        }
    }
    return dst;
}

/* ===== gfx_blit - full transform pipeline =====
 * 
 * Parameters:
 *   s       source surface (spritesheet parent)
 *   x, y    dest screen pos
 *   sx,sy   source top-left (frame on spritesheet)
 *   w, h    frame width / height
 *   alpha   0..255 per-blit transparency
 *   flipX   1=texte flipU swap; flipY=swapV endpoints
 *   angle   pivot-centered rotation (>0) in degrees
 *   scaleX,Y per-axis scale factor (>1 upscales)
 *   depth   z-buffer value (back-to-front sort)
 *   tint    ABGR color modulator; 0xFFFFFFFF=identity
 * 
 * Pivot: offset from sprite top-left to rotation centre.
 * Default (w/2, h/2) = center. Set via s->pivotX/pivotY.
 */

void gfx_blit(OL_Surface *s, int x, int y,
              int sx, int sy, int w, int h,
              int alpha, int flipX, int flipY, float angle,
              float scaleX, float scaleY, int depth, unsigned int tint)
{
    if (!gu_started || !s || w <= 0 || h <= 0) return;
    if (alpha < 0) alpha = 0;
    if (alpha > 255) alpha = 255;

    sceGuStart(GU_DIRECT, gu_list);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

    /* Step 1: CPU upscale + tint pass (nearest-neighbor) */
    unsigned int texW = s->realW, texH = s->realH;
    void *texData = s->pixels;
    int needScale = (tint != 0xFFFFFFFFu) || scaleX > 1.001f || scaleY > 1.001f;

    if (needScale) {
        int dw = 0, dh = 0;
        unsigned int *buf = nearest_scale(
            &s->pixels[sy * s->realW + sx], w, &dw, &dh,
            scaleX, scaleY, h, tint);
        if (!buf) { flush_gu(); return; }

        /* L2 cache flush: GPU via DMA reads from uncached memory.
         * nearest_scale writes to heap-allocated buffer which is in the cached region.
         * Without FlushCache(2), the GPU sees stale/corrupted pixel data,
         * and rapid flipX/flipY blit cycles corrupt GE state. */
        FlushCache(2);

        texW = next_pow2(dw);
        texH = next_pow2(dh);
        texData = buf;
    } else {
        /* No scale pass — source is raw surface pixels from the CPU side.
         * Must flush L2 cache so GE DMA sees latest flipX/flipY pixel data. */
        FlushCache(2);
    }

    sceGuTexImage(0, texW, texH, texW, texData);
    sceGuTexScale(1.0f / texW, 1.0f / texH);

    /* Step 2: pivot (relative to sprite top-left) */
    float pivotX = angle > 0.0f ? (float)s->pivotX : (float)(w / 2);
    float pivotY = angle > 0.0f ? (float)s->pivotY : (float)(h / 2);

    /* Step 3: draw */
    if (angle != 0.0f) {
        /* pivot-centered rotation (GU_TRIANGLE_FAN, 4 verts) */
        float rad = angle * 3.14159265358979f / 180.0f;
        float cr = cosf(rad), sr = sinf(rad);
        float cx = x + pivotX, cy = y + pivotY;
        float hw = w / 2.0f, hh = h / 2.0f;

        Vert *v = (Vert *)sceGuGetMemory(4 * sizeof(Vert));

        /* UV endpoints: flipX swaps U; flipY swaps V endpoints */
        float u0, u1, v0, v1;
        flip_u((float)sx, (float)w, flipX, &u0, &u1);
        flip_v((float)sy, (float)h, flipY, &v0, &v1);

        /* corners: TL, TR, BR, BL around sprite center */
        v[0].u = u0;   v[0].v = v0;
        v[0].x = (short)(-hw * cr + hh * sr + cx);
        v[0].y = (short)(-hw * sr - hh * cr + cy);
        v[1].u = u1;   v[1].v = v0;
        v[1].x = (short)( hw * cr + hh * sr + cx);
        v[1].y = (short)( hw * sr - hh * cr + cy);
        v[2].u = u1;   v[2].v = v1;
        v[2].x = (short)( hw * cr - hh * sr + cx);
        v[2].y = (short)( hw * sr + hh * cr + cy);
        v[3].u = u0;   v[3].v = v1;
        v[3].x = (short)(-hw * cr - hh * sr + cx);
        v[3].y = (short)(-hw * sr + hh * cr + cy);

        for (int i = 0; i < 4; i++)
            v[i].z = (short)depth;

        sceGuDrawArray(GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_VERTEX_16BIT |
                       GU_TRANSFORM_2D, 4, 0, v);
    } else {
        /* direct blit with flipX/Y (GU_SPRITES, 2 verts) */
        float u0, v0, u1, v1;
        flip_u((float)sx, (float)w, flipX, &u0, &u1);
        flip_v((float)sy, (float)h, flipY, &v0, &v1);

        Vert *v = (Vert *)sceGuGetMemory(2 * sizeof(Vert));
        v[0].u = u0; v[0].v = v0;
        v[0].x = (short)x;  v[0].y = (short)y;  v[0].z = (short)depth;
        v[1].u = u1; v[1].v = v1;
        v[1].x = (short)(x + w);  v[1].y = (short)(y + h);  v[1].z = (short)depth;

        sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_16BIT |
                       GU_TRANSFORM_2D, 2, 0, v);
    }
    flush_gu();
}
