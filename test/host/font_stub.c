/* Host graphics stubs for the font tests.  The real backend uploads the
 * sheet to the PSP GE; here we only need an allocated RGBA surface and a
 * place to record the last blit so tests can inspect source rectangles. */
#include "engine/gfx/gfx.h"

#include <stdlib.h>
#include <string.h>

int font_stub_blits;
OL_Surface *font_stub_last_surface;
int font_stub_last_x, font_stub_last_y;
int font_stub_last_sx, font_stub_last_sy;
int font_stub_last_sw, font_stub_last_sh;
unsigned int font_stub_last_tint;

OL_Surface *gfx_surface_new(int w, int h)
{
    if (w <= 0 || h <= 0)
        return NULL;
    OL_Surface *s = calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->w = w;
    s->h = h;
    s->realW = w;
    s->realH = h;
    s->pixels = calloc((size_t)w * (size_t)h, sizeof(unsigned int));
    if (!s->pixels) {
        free(s);
        return NULL;
    }
    return s;
}

void gfx_surface_free(OL_Surface *s)
{
    if (!s)
        return;
    free(s->pixels);
    free(s);
}

void gfx_blit(OL_Surface *s, int x, int y,
              int sx, int sy, int sw, int sh,
              int alpha, int flipX, int flipY, float angle,
              float scaleX, float scaleY, int z, unsigned tint)
{
    (void)alpha; (void)flipX; (void)flipY; (void)angle;
    (void)scaleX; (void)scaleY; (void)z;
    font_stub_blits++;
    font_stub_last_surface = s;
    font_stub_last_x = x;
    font_stub_last_y = y;
    font_stub_last_sx = sx;
    font_stub_last_sy = sy;
    font_stub_last_sw = sw;
    font_stub_last_sh = sh;
    font_stub_last_tint = tint;
}

/* Extra surface helpers used when the image module is linked into the host
 * binary for the game/PBP tests.  They mirror the real buffer semantics
 * (ABGR pixels, power-of-two pitch) without touching the GE. */
OL_Surface *gfx_surface_clone(OL_Surface *s)
{
    if (!s)
        return NULL;
    OL_Surface *c = gfx_surface_new(s->realW, s->realH);
    if (!c)
        return NULL;
    c->w = s->w;
    c->h = s->h;
    c->frameW = s->frameW;
    c->frameH = s->frameH;
    c->flipX = s->flipX;
    c->flipY = s->flipY;
    c->angle = s->angle;
    c->scaleX = s->scaleX;
    c->scaleY = s->scaleY;
    c->pivotX = s->pivotX;
    c->pivotY = s->pivotY;
    c->pivotSet = s->pivotSet;
    c->z = s->z;
    c->tint = s->tint;
    for (int y = 0; y < s->realH; y++)
        memcpy(c->pixels + (size_t)y * c->realW,
               s->pixels + (size_t)y * s->realW,
               (size_t)s->realW * sizeof(unsigned int));
    return c;
}

void gfx_surface_fill(OL_Surface *s, unsigned int color)
{
    if (!s)
        return;
    for (int y = 0; y < s->h; y++)
        for (int x = 0; x < s->w; x++)
            s->pixels[(size_t)y * s->realW + x] = color;
}

unsigned int gfx_surface_pixel_get(OL_Surface *s, int x, int y)
{
    if (!s || x < 0 || y < 0 || x >= s->w || y >= s->h)
        return 0;
    return s->pixels[(size_t)y * s->realW + x];
}

void gfx_surface_pixel_set(OL_Surface *s, int x, int y, unsigned int c)
{
    if (!s || x < 0 || y < 0 || x >= s->w || y >= s->h)
        return;
    s->pixels[(size_t)y * s->realW + x] = c;
}

void gfx_cache_note(void)
{
}
