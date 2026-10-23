/* OpenLua - PGF font loader (PSP sceLibFont)
 *
 * PGF fonts are the system font format used by OneLua's `font` module.
 * The loader renders the printable ASCII range into a cached RGBA atlas
 * once, then reuses the normal gfx_blit path for text drawing.
 */

#ifndef OL_PGF_H
#define OL_PGF_H

#include "engine/gfx/gfx.h"

typedef struct OL_PgfFont OL_PgfFont;

/** Load and rasterize a PGF font. Returns NULL on failure. */
OL_PgfFont *pgf_load(const char *path);

/** Release the font, its PGF library and its atlas. */
void pgf_free(OL_PgfFont *font);

int pgf_nchars(const OL_PgfFont *font);
int pgf_cellw(const OL_PgfFont *font);
int pgf_cellh(const OL_PgfFont *font);

/** Draw one ASCII glyph. (x,y) is the top-left of the text line. */
void pgf_draw_glyph(OL_PgfFont *font, int x, int y, unsigned char c,
                    unsigned int tint);

/** Draw a string. Returns the final X cursor. */
int pgf_draw_text(OL_PgfFont *font, int x, int y, const char *text,
                  unsigned int tint);

/** Pixel width of a string using the font's advances. */
int pgf_textwidth(OL_PgfFont *font, const char *text);

#endif /* OL_PGF_H */
