/* OpenLua - PGF font loader (software decoder)
 *
 * PGF fonts are the system font format used by OneLua's `font` module.
 * engine/font/pgf.c parses the container itself (no sceLibFont), rasterizes
 * the printable ASCII range once into a cached RGBA atlas and reuses the
 * normal gfx_blit path for text drawing.
 *
 * Only revision 2 PGF files are supported; pgf_load() returns NULL for
 * anything else so callers can fall back to an SFont/PNG atlas.
 */

#ifndef OL_PGF_H
#define OL_PGF_H

#include "engine/gfx/gfx.h"

typedef struct OL_PgfFont OL_PgfFont;

/** Load and rasterize a PGF font. Returns NULL on failure. */
OL_PgfFont *pgf_load(const char *path);

/** Release the font and its atlas. */
void pgf_free(OL_PgfFont *font);

int pgf_nchars(const OL_PgfFont *font);
int pgf_cellw(const OL_PgfFont *font);
int pgf_cellh(const OL_PgfFont *font);

/** Ink-box height in pixels, matching OneLua's `screen.textheight(font)`. */
int pgf_textheight(const OL_PgfFont *font);

/** Draw one ASCII glyph. (x,y) is the top-left of the text line. */
void pgf_draw_glyph(OL_PgfFont *font, int x, int y, unsigned char c,
                    unsigned int tint);

/** Scaled variant of pgf_draw_glyph. scale <= 0 is treated as 1. */
void pgf_draw_glyph_scaled(OL_PgfFont *font, int x, int y, unsigned char c,
                           unsigned int tint, float scale);

/** Draw a string. Returns the final X cursor. */
int pgf_draw_text(OL_PgfFont *font, int x, int y, const char *text,
                  unsigned int tint);

/** Scaled variant of pgf_draw_text. Returns the final X cursor. */
int pgf_draw_text_scaled(OL_PgfFont *font, int x, int y, const char *text,
                         unsigned int tint, float scale);

/** Pixel width of a string using the font's advances. */
int pgf_textwidth(OL_PgfFont *font, const char *text);

/** Scaled variant of pgf_textwidth. */
int pgf_textwidth_scaled(OL_PgfFont *font, const char *text, float scale);

#endif /* OL_PGF_H */
