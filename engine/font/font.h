/* OpenLua - bitmap font system (public API)
 *
 * Load a spritesheet-style font image (PNG) where each glyph occupies
 * a fixed-size cell. Characters map sequentially from char 0x20 (space). */

#ifndef OL_FONT_H
#define OL_FONT_H

#include <lua.h>
#include <lauxlib.h>
#include "engine/gfx/gfx.h"

/* Per-font grid metadata + texture state. */
typedef struct {
    /* Grid layout (set by font_load) */
    int     cellW;                     /* pixel width of one glyph         */
    int     cellH;                     /* pixel height of one glyph        */
    int     nCols;                     /* number of columns in sheet       */
    int     nRows;                     /* number of rows in sheet          */
    int     nChars;                    /* total glyphs = nCols × nRows     */

    /* Sheet source pixels (raw from PNG) — lazy-uploaded to GPU texture. */
    int     texReady;                  /* 1 if sheet is uploaded           */
    int     sheetW, sheetH;            /* raw PNG pixel dimensions         */
    struct OL_Surface *sheet;          /* GPU-backed texture surface       */
    char    texPath[512];              /* original path (for reload)       */

    /* Draw-time transform state — copied from user-set values on load. */
    unsigned int flipX;                /* toggle draw-time UV U-flip       */
    unsigned int flipY;                /* toggle draw-time UV V-flip       */
    float    angle;                    /* rotation in degrees, CCW         */
    float    scaleX;                   /* horizontal scale (>0)            */
    float    scaleY;                   /* vertical scale (>0)              */
    int     pivotX, pivotY;            /* rotation/center anchor (px)      */
    int     z;                         /* depth order                      */
    unsigned int tint;                 /* ABGR color modulation            */
} OLF_Font;

/* ---- C API (used from engine code and Lua bindings) ---- */

/** Load a PNG spritesheet font. `cellW × cellH` = pixel size of each glyph. */
OLF_Font font_load(const char *path, int cellW, int cellH);

/** Blit one glyph (index `(char code - 0x20)`) onto dst at (dx, dy). >0 = ok. */
int      font_blt(OL_Surface *dst, int dx, int dy, char c, OLF_Font f);

/** Render string on surface starting at (x0, y0). Advances cursor per glyph.*/
void     font_print(OL_Surface *dst, int x0, int y0, const char *text, OLF_Font f);

/* ---- Lua binding registration (called once during engine init) ---- */

void font_api_register(lua_State *L);

#endif /* OL_FONT_H */
