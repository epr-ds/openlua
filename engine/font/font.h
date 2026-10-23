/* OpenLua - bitmap font system (public API)
 *
 * Load a spritesheet-style font image where each glyph occupies a fixed-size
 * cell. Characters map sequentially from char 0x20 (space). The `sfont` table
 * additionally accepts classic variable-width SFont sheets.
 *
 * Fonts are handled by numeric (1-based) font handles, OneLua-style:
 *
 *   local f = font.new("assets/font.png", 8, 16)   -- handle
 *   screen.print(10, 20, "Hello PSP", f)    -- draw text with that font
 *   screen.print(10, 40, "default font")    -- uses screen.font(f) default
 *   font.textwidth(f, "Hello")              -- measure string width
 */

#ifndef OL_FONT_H
#define OL_FONT_H

#include <lua.h>
#include <lauxlib.h>
#include "engine/gfx/gfx.h"

/* ---- C API (used from engine code and the Lua bindings) ---- */

/** Load a PNG spritesheet font. cellW x cellH = pixel size of each glyph.
 *  Returns a 1-based font handle, or 0 on error.
 *  The sheet is repacked into a <=512px texture atlas and uploaded lazily
 *  on first draw. */
int font_alloc(const char *path, int cellW, int cellH);

/** Load a OneLua-style SFONT image without explicit cell dimensions.
 *  Classic SFonts use magenta separators on the top row and start at '!';
 *  fixed-grid sheets with 95 cells are accepted as a compatibility fallback.
 *  Returns a 1-based handle, or 0 on error. */
int font_alloc_sfont(const char *path);

/** Number of glyphs in the font (0 = invalid handle). */
int font_nchars(int h);

/** Glyph cell width/height in pixels. */
int font_cellw(int h);
int font_cellh(int h);

/** Draw one glyph (ASCII 0x20..0x7E) at scene coords (dx,dy).
 *  tint = ABGR color to modulate glyph pixels (0xFFFFFFFF = no tint). */
void font_draw_glyph(int h, int dx, int dy, char c, unsigned int tint);

/** Draw a string starting at (x0,y0); advances cursor per glyph.
 *  Handles '\n' (wrap to x0, y += cellH) and '\t' (indent 4 cells).
 *  tint = ABGR color or 0xFFFFFFFF for no tint.
 *  Returns the final X cursor position. */
int font_draw_text(int h, int x0, int y0, const char *text, unsigned int tint);

/** Pixel width of a string (each cell advances by cellW). */
int font_textwidth(const char *text, int h);

/** Release a font handle and free its VRAM sheet. */
void font_free(int h);

/** Current default font + tint for screen.print (OneLua screen.font style). */
void font_set_default(int h);
int  font_default(void);
void font_set_tint(unsigned int abgr);
unsigned int font_tint(void);

/* ---- Lua bindings ---- */

/** Registers the `font` and `sfont` global tables and hooks `screen.print` /
 *  `screen.font` / `screen.textwidth` onto the existing `screen` table.
 *  Call after the `screen` table has been created in core init. */
void font_api_register(lua_State *L);

#endif /* OL_FONT_H */
