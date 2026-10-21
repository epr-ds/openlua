/* OpenLua - graphic pipeline using PSP GE for sprite rendering.
 * Handles nearest-neighbor upscale, flip X/Y, rotation with pivot center,
 * color tint/modulation, and per-blit z-depth ordering. */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspge.h>
#include <psputils.h>

#include "gfx.h"
#include "transform.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272

static unsigned int __attribute__((aligned(16))) gu_list[0x40000 / 4];
static int gu_started = 0;
static unsigned int clear_color = 0xFF000000;
static void *vram_fb[2] = { NULL, NULL };

/*=========================================================================
 * Helpers
 *=========================================================================*/

static void *vram_alloc(int size)
{
    /* GU buffer pointers are offsets from the start of EDRAM.  Keep them
     * relative here; gfx_vram_fb() adds sceGeEdramGetAddr() for callers that
     * need a real display address.  GE frame buffers must be 8192-byte
     * aligned. */
    static unsigned int head = 0;
    void *p = (void *)head;
    head += ((unsigned int)size + 0x1FFF) & ~0x1FFFu;
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

    vram_fb[0] = fb0;
    vram_fb[1] = fb1;

    sceGuInit();
    sceGuStart(GU_DIRECT, gu_list);
    sceGuDrawBuffer(GU_PSM_8888, fb0, BUF_WIDTH);
    sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, fb1, BUF_WIDTH);
    sceGuDepthBuffer(zbd, BUF_WIDTH);
    sceGuOffset(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
    /* 2D depth ordering: z=0 is the default plane, larger z is nearer.
     * Clear depth to 0 so z=0 passes GEQUAL, and keep culling off because
     * flips/rotation can reverse a quad's winding. */
    sceGuDepthRange(0, 0xFFFF);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
    sceGuDisable(GU_CULL_FACE);
    sceGuShadeModel(GU_SMOOTH);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuClearDepth(0);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexScale(1.0f, 1.0f);
    sceGuAmbientColor(0xffffffff);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuClearColor(clear_color);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
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
    sceGuSwapBuffers();
    sceDisplayWaitVblankStart();
    sceGuStart(GU_DIRECT, gu_list);
    sceGuClearColor(clear_color);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(0, 0);
}

int gfx_gu_started(void)
{
    return gu_started;
}

void *gfx_list(void)
{
    return gu_list;
}

void *gfx_vram_fb(int index)
{
    return (void *)((unsigned int)sceGeEdramGetAddr() +
                    (unsigned int)vram_fb[index ? 1 : 0]);
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
    s->pivotSet = 0;
    s->z = 0;
    s->centerOffX = 0;
    s->centerOffY = 0;
    s->tint = 0xFFFFFFFFu;
    s->pixels = (unsigned int *)memalign(16, (size_t)s->realW * s->realH * 4);
    if (!s->pixels) {
        free(s);
        return NULL;
    }
    memset(s->pixels, 0, (size_t)s->realW * s->realH * 4);
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
    c->pivotSet = s->pivotSet;
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
    sceKernelDcacheWritebackAll();
}

unsigned int gfx_surface_pixel_get(OL_Surface *s, int x, int y)
{
    if (x < 0 || y < 0 || x >= s->w || y >= s->h)
        return 0;
    /* Flush L2 before read — GE DMA reads uncached memory only */
    sceKernelDcacheWritebackAll();
    return s->pixels[y * s->realW + x];
}

void gfx_surface_pixel_set(OL_Surface *s, int x, int y, unsigned int c)
{
    if (x < 0 || y < 0 || x >= s->w || y >= s->h)
        return;
    s->pixels[y * s->realW + x] = c;
    sceKernelDcacheWritebackAll();
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
    sceKernelDcacheWritebackAll();
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
    sceKernelDcacheWritebackAll();
}

void gfx_cache_note(void)
{
    sceKernelDcacheWritebackAll();
}

typedef struct {
    float u, v;
    unsigned int color;
    short x, y, z;
} Vert;

void gfx_blit(OL_Surface *s, int x, int y,
              int sx, int sy, int w, int h,
              int alpha, int flipX, int flipY, float angle,
              float scaleX, float scaleY, int depth, unsigned int tint)
{
    if (!gu_started || !s || !s->pixels) return;
    if (sx < 0) { w += sx; sx = 0; }
    if (sy < 0) { h += sy; sy = 0; }
    if (sx >= s->w || sy >= s->h) return;
    if (sx + w > s->w) w = s->w - sx;
    if (sy + h > s->h) h = s->h - sy;
    if (w <= 0 || h <= 0) return;
    if (scaleX <= 0.0f || scaleY <= 0.0f) return;
    if (alpha < 0) alpha = 0;
    if (alpha > 255) alpha = 255;

    float px = s->pivotSet ? (float)s->pivotX : (float)w * 0.5f;
    float py = s->pivotSet ? (float)s->pivotY : (float)h * 0.5f;

    OL_BlitParams p;
    p.x = (float)x;
    p.y = (float)y;
    p.scaleX = scaleX;
    p.scaleY = scaleY;
    p.angle = angle;
    p.pivotX = px;
    p.pivotY = py;
    p.srcX = sx;
    p.srcY = sy;
    p.srcW = w;
    p.srcH = h;
    p.texW = s->realW;
    p.texH = s->realH;
    p.flipX = flipX;
    p.flipY = flipY;

    OL_BlitVertex q[4];
    ol_blit_quad(&p, q);

    unsigned int tintA = (tint >> 24) & 0xFFu;
    unsigned int outA = ((unsigned int)alpha * tintA + 127u) / 255u;
    unsigned int color = (outA << 24) | (tint & 0x00FFFFFFu);

    sceKernelDcacheWritebackAll();

    sceGuStart(GU_DIRECT, gu_list);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexScale(1.0f, 1.0f);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuTexImage(0, s->realW, s->realH, s->realW, s->pixels);

    Vert *v = (Vert *)sceGuGetMemory(4 * sizeof(Vert));
    for (int i = 0; i < 4; i++) {
        /* transform.c produces normalised UVs; the GE samples in texels. */
        v[i].u = q[i].u * (float)s->realW;
        v[i].v = q[i].v * (float)s->realH;
        v[i].color = color;
        v[i].x = (short)lrintf(q[i].x);
        v[i].y = (short)lrintf(q[i].y);
        v[i].z = (short)depth;
    }

    sceGuDrawArray(GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_COLOR_8888 |
                   GU_VERTEX_16BIT | GU_TRANSFORM_2D, 4, 0, v);
    flush_gu();
}
