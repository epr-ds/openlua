/* OpenLua - surface abstraction over PSP VRAM/RAM textures.
 * Surfaces are plain 32-bit ABGR pixel buffers padded to power-of-two
 * dimensions (realW/realH as PSP GE requires), so any image can be
 * uploaded straight via sceGuTexImage.
 */

#ifndef OL_GFX_H
#define OL_GFX_H

#include <pspgu.h>

typedef struct OL_Surface {
    int w;              /* logical width  */
    int h;              /* logical height */
    int realW;          /* pow2-padded pitch (hardware requirement) */
    int realH;
    unsigned int *pixels;   /* ABGR 0xAABBGGRR, row-major with realW stride */
    int frameW;         /* > 0 when loaded as a spritesheet */
    int frameH;
    unsigned int flipX;
    unsigned int flipY;
    float angle;        /* rotation in degrees (centered on pivot) */
    float scaleX, scaleY;
    int pivotX, pivotY;       /* origin/anchor point for transform (0,0 = top-left of frame) */
    int pivotSet;           /* nonzero once image.pivot() has been called */
    int z;                  /* depth order 0..271 (PSP screen coords: y-down) */
    int centerOffX;
    int centerOffY;
    unsigned int tint;      /* color modulation (ABGR), 0xFFFFFFFF = no tint */
} OL_Surface;

void gfx_init(void);
void gfx_set_clear_color(unsigned int c);
void gfx_flip_scene(void);
int gfx_gu_started(void);
void *gfx_list(void);

OL_Surface *gfx_surface_new(int w, int h);
OL_Surface *gfx_surface_clone(OL_Surface *s);
void gfx_surface_free(OL_Surface *s);
void gfx_surface_fill(OL_Surface *s, unsigned int color);
unsigned int gfx_surface_pixel_get(OL_Surface *s, int x, int y);
void gfx_surface_pixel_set(OL_Surface *s, int x, int y, unsigned int c);
void gfx_surface_hflip(OL_Surface *s);
void gfx_surface_vflip(OL_Surface *s);

/* Blit a frame of src at screen (x,y). sx/sy/sw/sh = source rect (frame).
 * alpha 0..255, angle in degrees (centered on pivot), z depth order.
 * scaleX/scaleY = uniform or per-axis scale (>0).
 * flipX/flipY swap texture coords at draw time.
 * tint = ABGR color to modulate pixels before applying alpha. */
void gfx_blit(OL_Surface *s, int x, int y,
              int sx, int sy, int sw, int sh,
              int alpha, int flipX, int flipY, float angle,
              float scaleX, float scaleY, int z, unsigned tint);

void gfx_cache_note(void);

#endif
