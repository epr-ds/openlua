/* Host graphics stubs for the font tests.  The real backend uploads the
 * sheet to the PSP GE; here we only need an allocated RGBA surface and a
 * place to record the last blit so tests can inspect source rectangles. */
#include "engine/gfx/gfx.h"

#include <stdlib.h>

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
