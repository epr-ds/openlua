/* OpenLua - bitmap font system (public API)
 *
 * Load a spritesheet-style font image where each glyph occupies a fixed-size
 * cell. Characters map sequentially from char 0x20 (space). */

#ifndef OL_FONT_H
#define OL_FONT_H

#include <lua.h>
#include <lauxlib.h>
#include "engine/gfx/gfx.h"

/* Per-font grid metadata (parsed at load time from PNG sheet). */
typedef struct {
    int     cellW;                     /* pixel width of one glyph         */
    int     cellH;                     /* pixel height of one glyph        */
    int     nCols;                     /* number of columns in the sheet   */
    int     nRows;                     /* number of rows in the sheet      */
    int     nChars;                    /* total = nCols * nRows            */
} OLF_Font;

/* ---- C API (used from font.c) ---- */

/** Load a PNG spritesheet font. All cells must be cellW x cellH pixels, */
OLF_Font font_load(const char *path, int cellW, int cellH);

/** Blit one glyph at char code `c` onto dst surface at (dx, dy). */
int      font_blt(OL_Surface *dst, int dx, int dy, char c, OLF_Font f);

/** Render full string onto surface starting at (x0, y0). Advances cursor. */
void     font_print(OL_Surface *dst, int x0, int y0, const char *text, OLF_Font f);

/* ---- Lua binding registration ---- */

void font_api_register(lua_State *L);    /* called by engine core at init  */

#endif /* OL_FONT_H */
