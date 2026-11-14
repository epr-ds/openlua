/* OpenLua - bitmap font system implementation
 *
 * Fonts are loaded from a PNG spritesheet: a grid of cells, each
 * cellW x cellH pixels. Characters map sequentially starting from
 * char 0x20 (space): glyph 0 = ' ', glyph 1 = '!', glyph 2 = '"', ...
 *
 * On first draw the sheet is repacked into a texture atlas no wider than
 * 512 px (the PSP GE limit) and uploaded lazily. Drawing uses pixel-unit UV
 * coords so a glyph blit is simply: gfx_blit(sheet, x, y, sx, sy,
 *                            cellW, cellH, ...).
 *
 * Pixel format matches image.c: stbi RGBA bytes read as uint32 LE,
 * i.e. pixel value = (A<<24)|(B<<16)|(G<<8)|R.
 */

#include "engine/font/font.h"
#include "engine/font/pgf.h"
#include "engine/gfx/gfx.h"
#include "engine/core/olreg.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* stb_image implementation lives in engine/core/stb_impl.o */
#include "third_party/stb/stb_image.h"

#define FONT_MAX 8
#define FONT_MAX_CHARS 96
#define FONT_SEP_RUNS  (FONT_MAX_CHARS + 2)

enum {
    FONT_KIND_FIXED = 0,
    FONT_KIND_SFONT = 1,
    FONT_KIND_PGF   = 2
};

static struct {
    int        used;
    int        kind;
    char       path[256];
    int        cellW, cellH;
    int        nChars;
    int        spaceW;                       /* classic SFont space advance */
    int        glyphX[FONT_MAX_CHARS];       /* classic SFont source x    */
    int        glyphW[FONT_MAX_CHARS];       /* classic SFont source width */
    int        glyphAdv[FONT_MAX_CHARS];     /* classic SFont advance      */
    int        glyphAtlasX[FONT_MAX_CHARS];  /* packed atlas x            */
    int        glyphAtlasY[FONT_MAX_CHARS];  /* packed atlas y            */
    int        atlasCols;                    /* fixed-grid atlas columns  */
    OL_Surface *sheet;        /* packed texture (lazy upload) */
    OL_PgfFont *pgf;          /* PGF system font (owned)       */
} g_fonts[FONT_MAX];

static int             g_defaultFont = 0;             /* for screen.print */
static unsigned int    g_defaultTint = 0xFFFFFFFFu;   /* ABGR, FFFFFF = none */

/* =====================================================================
 * Allocation
 * ===================================================================*/

static int font_claim(const char *path)
{
    for (int i = 0; i < FONT_MAX; i++) {
        if (!g_fonts[i].used) {
            memset(&g_fonts[i], 0, sizeof(g_fonts[i]));
            g_fonts[i].used = 1;
            strncpy(g_fonts[i].path, path, sizeof(g_fonts[i].path) - 1);
            g_fonts[i].path[sizeof(g_fonts[i].path) - 1] = '\0';
            return i + 1;                    /* 1-based handle */
        }
    }
    return 0;                               /* all slots full */
}

int font_alloc(const char *path, int cellW, int cellH)
{
    if (cellW <= 0 || cellH <= 0 || cellW > 256 || cellH > 256)
        return 0;

    int w = 0, h = 0, comp = 0;
    unsigned char *probe = stbi_load(path, &w, &h, &comp, 4);
    if (!probe)
        return 0;
    int nChars = (w / cellW) * (h / cellH);
    stbi_image_free(probe);
    if (nChars <= 0)
        return 0;

    int handle = font_claim(path);
    if (handle <= 0)
        return 0;

    g_fonts[handle - 1].kind   = FONT_KIND_FIXED;
    g_fonts[handle - 1].cellW  = cellW;
    g_fonts[handle - 1].cellH  = cellH;
    g_fonts[handle - 1].nChars = nChars;
    return handle;
}

static int has_pgf_ext(const char *p)
{
    size_t n = strlen(p);
    if (n < 4)
        return 0;
    p += n - 4;
    return p[0] == '.' &&
           (p[1] == 'p' || p[1] == 'P') &&
           (p[2] == 'g' || p[2] == 'G') &&
           (p[3] == 'f' || p[3] == 'F');
}

int font_alloc_pgf(const char *path)
{
    OL_PgfFont *pgf = pgf_load(path);

    if (!pgf) {
        /* Atlas fallback: a PGF that cannot be decoded (the standard
         * PSP build has no sceLibFont) falls back to a sibling PNG
         * atlas, e.g. assets/font/halo-ltn0.pgf -> halo-ltn0.png.
         * font_alloc_sfont handles both classic SFont sheets and
         * 95-cell fixed grids. */
        if (path && has_pgf_ext(path)) {
            char alt[256];
            size_t n = strlen(path);
            if (n < sizeof(alt)) {
                memcpy(alt, path, n + 1);
                alt[n - 3] = 'p';
                alt[n - 2] = 'n';
                alt[n - 1] = 'g';
                int h = font_alloc_sfont(alt);
                if (h > 0)
                    return h;
            }
        }
        return 0;
    }

    int handle = font_claim(path);
    if (handle <= 0) {
        pgf_free(pgf);
        return 0;
    }

    int i = handle - 1;
    g_fonts[i].kind   = FONT_KIND_PGF;
    g_fonts[i].cellW  = pgf_cellw(pgf);
    g_fonts[i].cellH  = pgf_cellh(pgf);
    g_fonts[i].nChars = pgf_nchars(pgf);
    g_fonts[i].pgf    = pgf;
    return handle;
}

static int pixel_is_pink(const unsigned char *p)
{
    return p[0] > 200 && p[1] < 64 && p[2] > 200 && p[3] > 0;
}

int font_alloc_sfont(const char *path)
{
    int w = 0, h = 0, comp = 0;
    unsigned char *rgba = stbi_load(path, &w, &h, &comp, 4);
    if (!rgba || w <= 0 || h <= 1) {
        if (rgba)
            stbi_image_free(rgba);
        return 0;
    }

    /* Classic SFont: magenta separator runs on the top row.  The glyph
     * areas are the spaces between adjacent runs and characters start at
     * '!'.  The source rectangle uses separator midpoints, as in the
     * original SFont library, while the advance uses the clear gap. */
    struct { int start, end; } runs[FONT_SEP_RUNS];
    int nRuns = 0;
    for (int x = 0; x < w; ) {
        if (pixel_is_pink(rgba + (size_t)x * 4)) {
            int start = x;
            while (x < w && pixel_is_pink(rgba + (size_t)x * 4))
                x++;
            if (nRuns < FONT_SEP_RUNS) {
                runs[nRuns].start = start;
                runs[nRuns].end = x;
            }
            nRuns++;
        } else {
            x++;
        }
    }

    if (nRuns >= 2 && nRuns <= FONT_SEP_RUNS) {
        /* A missing trailing separator is legal: use the sheet edge. */
        if (runs[nRuns - 1].end < w && nRuns < FONT_SEP_RUNS) {
            runs[nRuns].start = w;
            runs[nRuns].end = w;
            nRuns++;
        }

        int letters = nRuns - 1;
        if (letters > 0 && letters <= FONT_MAX_CHARS) {
            int maxAdvance = 0;
            int glyphH = h - 1;

            for (int i = 0; i < letters; i++) {
                int advance = runs[i + 1].start - runs[i].end;
                if (advance < 1)
                    advance = 1;
                if (advance > maxAdvance)
                    maxAdvance = advance;
            }

            int handle = font_claim(path);
            if (handle > 0) {
                int i = handle - 1;
                /* Copy the parsed metrics into the claimed slot. */
                for (int g = 0; g < letters; g++) {
                    int midL = (runs[g].start + runs[g].end) / 2;
                    int midR = (runs[g + 1].start + runs[g + 1].end) / 2;
                    int srcW = midR - midL;
                    int advance = runs[g + 1].start - runs[g].end;

                    if (srcW < 1)
                        srcW = 1;
                    if (advance < 1)
                        advance = 1;
                    g_fonts[i].glyphX[g] = midL;
                    g_fonts[i].glyphW[g] = srcW;
                    g_fonts[i].glyphAdv[g] = advance;
                }
                g_fonts[i].kind   = FONT_KIND_SFONT;
                g_fonts[i].cellW  = maxAdvance;
                g_fonts[i].cellH  = glyphH;
                g_fonts[i].nChars = letters;
                g_fonts[i].spaceW = g_fonts[i].glyphAdv[0];
                stbi_image_free(rgba);
                return handle;
            }
        }
    }

    /* Fixed-grid fallback: a 95-cell ASCII sheet (0x20..0x7E) can be loaded
     * without cell dimensions by deriving the cell width from the image. */
    if (w % 95 == 0) {
        int cellW = w / 95;
        if (cellW > 0 && cellW <= 256 && h <= 256) {
            int handle = font_claim(path);
            if (handle > 0) {
                g_fonts[handle - 1].kind   = FONT_KIND_FIXED;
                g_fonts[handle - 1].cellW  = cellW;
                g_fonts[handle - 1].cellH  = h;
                g_fonts[handle - 1].nChars = 95;
                stbi_image_free(rgba);
                return handle;
            }
        }
    }

    stbi_image_free(rgba);
    return 0;
}

int font_nchars(int h)
{
    if (h < 1 || h > FONT_MAX || !g_fonts[h - 1].used) return 0;
    return g_fonts[h - 1].nChars;
}

int font_cellw(int h)
{
    if (h < 1 || h > FONT_MAX || !g_fonts[h - 1].used) return 0;
    return g_fonts[h - 1].cellW;
}

int font_cellh(int h)
{
    if (h < 1 || h > FONT_MAX || !g_fonts[h - 1].used) return 0;
    return g_fonts[h - 1].cellH;
}

void font_free(int h)
{
    if (h < 1 || h > FONT_MAX || !g_fonts[h - 1].used)
        return;
    if (g_fonts[h - 1].kind == FONT_KIND_PGF) {
        pgf_free(g_fonts[h - 1].pgf);
    } else if (g_fonts[h - 1].sheet) {
        gfx_surface_free(g_fonts[h - 1].sheet);
    }
    memset(&g_fonts[h - 1], 0, sizeof(g_fonts[h - 1]));
    if (g_defaultFont == h)
        g_defaultFont = 0;
}

/* =====================================================================
 * Lazy VRAM upload: decode PNG -> power-of-two GPU surface.
 * Identical pixel convention to image.c (RGBA bytes as uint32 LE).
 * ===================================================================*/

/* Copy a w x h rectangle from a tightly packed RGBA source into a surface.
 * Source pixels use the same ABGR word order as gfx_surface_* on the PSP. */
static void copy_rect(OL_Surface *dst, int dx, int dy,
                      const unsigned int *src, int srcW, int sx, int sy,
                      int w, int h)
{
    for (int y = 0; y < h; y++) {
        const unsigned int *srow = src + (size_t)(sy + y) * (size_t)srcW + sx;
        unsigned int *drow = dst->pixels +
                             (size_t)(dy + y) * (size_t)dst->realW + dx;
        memcpy(drow, srow, (size_t)w * sizeof(unsigned int));
    }
}

/* Build a <=512px-wide texture atlas from the source sheet.  The PSP GE
 * cannot address textures wider than 512 pixels, so both the fixed-grid
 * sheets (95 x 8px = 760px wide) and the classic SFont strip are repacked
 * before upload.  Glyph metrics and public handles stay unchanged. */
static int ensure_sheet(int h)
{
    if (h < 1 || h > FONT_MAX || !g_fonts[h - 1].used)
        return 0;

    int i = h - 1;
    if (g_fonts[i].sheet)
        return 1;

    int w = 0, hh = 0, comp = 0;
    unsigned char *rgba = stbi_load(g_fonts[i].path, &w, &hh, &comp, 4);
    if (!rgba)
        return 0;

    const unsigned int *src = (const unsigned int *)rgba;
    OL_Surface *s = NULL;

    if (g_fonts[i].kind == FONT_KIND_FIXED) {
        int cellW = g_fonts[i].cellW;
        int cellH = g_fonts[i].cellH;
        int cols = w / cellW;
        int rows = hh / cellH;
        int n = g_fonts[i].nChars;
        int acols = 512 / cellW;

        if (cols < 1 || rows < 1 || acols < 1)
            goto fail;
        if (acols > cols)
            acols = cols;
        if (n > cols * rows)
            n = cols * rows;

        int arows = (n + acols - 1) / acols;
        int aw = acols * cellW;
        int ah = arows * cellH;
        if (n < 1 || aw > 512 || ah > 512)
            goto fail;

        s = gfx_surface_new(aw, ah);
        if (!s)
            goto fail;

        for (int idx = 0; idx < n; idx++) {
            int sc = idx % cols;
            int sr = idx / cols;
            int dc = idx % acols;
            int dr = idx / acols;
            copy_rect(s, dc * cellW, dr * cellH, src, w,
                      sc * cellW, sr * cellH, cellW, cellH);
        }
        g_fonts[i].atlasCols = acols;
    } else {
        int glyphH = g_fonts[i].cellH;
        int ax = 0, ay = 0, atlasH = glyphH;

        if (glyphH < 1 || glyphH > 512)
            goto fail;

        for (int g = 0; g < g_fonts[i].nChars; g++) {
            int gw = g_fonts[i].glyphW[g];
            if (gw < 1 || gw > 512)
                goto fail;
            if (ax + gw > 512) {
                ax = 0;
                ay += glyphH;
            }
            g_fonts[i].glyphAtlasX[g] = ax;
            g_fonts[i].glyphAtlasY[g] = ay;
            ax += gw;
            if (ay + glyphH > atlasH)
                atlasH = ay + glyphH;
        }
        if (atlasH > 512)
            goto fail;

        s = gfx_surface_new(512, atlasH);
        if (!s)
            goto fail;

        for (int g = 0; g < g_fonts[i].nChars; g++) {
            int gw = g_fonts[i].glyphW[g];
            int gx = g_fonts[i].glyphX[g];
            if (gx < 0) gx = 0;
            if (gx + gw > w) gw = w - gx;
            if (gw > 0)
                copy_rect(s, g_fonts[i].glyphAtlasX[g], g_fonts[i].glyphAtlasY[g],
                          src, w, gx, 1, gw, glyphH);
        }
    }

    g_fonts[i].sheet = s;
    stbi_image_free(rgba);
    return 1;

fail:
    if (s)
        gfx_surface_free(s);
    stbi_image_free(rgba);
    return 0;
}

/* =====================================================================
 * Drawing (goes directly into the GE scene list, like image.blit)
 * ===================================================================*/

void font_draw_glyph(int h, int dx, int dy, char c, unsigned int tint)
{
    if (h < 1 || h > FONT_MAX || !g_fonts[h - 1].used)
        return;

    int i = h - 1;
    if (g_fonts[i].kind == FONT_KIND_PGF) {
        pgf_draw_glyph(g_fonts[i].pgf, dx, dy, (unsigned char)c, tint);
        return;
    }
    if (g_fonts[i].kind == FONT_KIND_SFONT) {
        int idx = (int)(unsigned char)c - 33;
        if (idx < 0 || idx >= g_fonts[i].nChars)
            return;
        if (!ensure_sheet(h))
            return;
        gfx_blit(g_fonts[i].sheet,
                 dx, dy,
                 g_fonts[i].glyphAtlasX[idx], g_fonts[i].glyphAtlasY[idx],
                 g_fonts[i].glyphW[idx], g_fonts[i].cellH,
                 255, 0, 0, 0.0f, 1.0f, 1.0f, 0, tint);
        return;
    }

    if ((unsigned int)(c - ' ') >= (unsigned int)g_fonts[h - 1].nChars)
        return;
    if (!ensure_sheet(h))
        return;

    int cellW = g_fonts[i].cellW;
    int cellH = g_fonts[i].cellH;

    /* Grid math uses the packed atlas, whose width is capped at 512px
     * (the PSP GE texture limit). */
    int sheetCols = g_fonts[i].atlasCols;
    if (sheetCols < 1) sheetCols = 1;
    int idx = (int)(unsigned char)(c - ' ');
    int col = idx % sheetCols;
    int row = idx / sheetCols;

    gfx_blit(g_fonts[i].sheet,
             dx, dy,                     /* dest position on screen       */
             col * cellW, row * cellH,   /* source rect (glyph cell)      */
             cellW, cellH,               /* source size                   */
             255,                        /* alpha                         */
             0, 0,                       /* no UV flips for text          */
             0.0f,                       /* no rotation                   */
             1.0f, 1.0f,                 /* no scaling                    */
             0,                          /* no z-ordering                 */
             tint);                      /* ABGR tint or 0xFFFFFFFF       */
}

int font_draw_text(int h, int x0, int y0, const char *text, unsigned int tint)
{
    if (h < 1 || h > FONT_MAX || !g_fonts[h - 1].used || !text)
        return x0;

    if (g_fonts[h - 1].kind == FONT_KIND_PGF)
        return pgf_draw_text(g_fonts[h - 1].pgf, x0, y0, text, tint);

    int cellW = g_fonts[h - 1].cellW;
    int cellH = g_fonts[h - 1].cellH;
    int x = x0, y = y0;

    if (g_fonts[h - 1].kind == FONT_KIND_SFONT) {
        for (size_t i = 0; text[i]; i++) {
            unsigned char ch = (unsigned char)text[i];
            if (ch == '\n') {
                y += cellH;
                x = x0;
            } else if (ch == '\t') {
                x += g_fonts[h - 1].spaceW * 4;
            } else if (ch == ' ') {
                x += g_fonts[h - 1].spaceW;
            } else if (ch >= 33 &&
                       (unsigned int)(ch - 33) < (unsigned int)g_fonts[h - 1].nChars) {
                int idx = (int)ch - 33;
                font_draw_glyph(h, x, y, (char)ch, tint);
                x += g_fonts[h - 1].glyphAdv[idx];
            }
        }
        return x;
    }

    for (size_t i = 0; text[i]; i++) {
        unsigned char ch = (unsigned char)text[i];
        if (ch >= 32 && (unsigned int)(ch - 32) < (unsigned int)g_fonts[h - 1].nChars) {
            font_draw_glyph(h, x, y, (char)ch, tint);
            x += cellW;
        } else if (ch == '\n') {
            y += cellH;
            x = x0;
        } else if (ch == '\t') {
            x += cellW * 4;
        }
    }
    return x;
}

/* =====================================================================
 * Scaled drawing (OneLua `size` argument)
 *
 * `scale` is a multiplier around each glyph's top-left corner, so
 * size 1.0 keeps the legacy pixel metrics, 0.5 halves them, etc.
 * ===================================================================*/

void font_draw_glyph_scaled(int h, int dx, int dy, char c, unsigned int tint,
                            float scale)
{
    if (h < 1 || h > FONT_MAX || !g_fonts[h - 1].used)
        return;
    if (!(scale > 0.0f))
        scale = 1.0f;

    int i = h - 1;
    if (g_fonts[i].kind == FONT_KIND_PGF) {
        pgf_draw_glyph_scaled(g_fonts[i].pgf, dx, dy, (unsigned char)c, tint,
                              scale);
        return;
    }
    if (g_fonts[i].kind == FONT_KIND_SFONT) {
        int idx = (int)(unsigned char)c - 33;
        if (idx < 0 || idx >= g_fonts[i].nChars)
            return;
        if (!ensure_sheet(h))
            return;
        gfx_blit(g_fonts[i].sheet,
                 dx, dy,
                 g_fonts[i].glyphAtlasX[idx], g_fonts[i].glyphAtlasY[idx],
                 g_fonts[i].glyphW[idx], g_fonts[i].cellH,
                 255, 0, 0, 0.0f, scale, scale, 0, tint);
        return;
    }

    if ((unsigned int)(c - ' ') >= (unsigned int)g_fonts[i].nChars)
        return;
    if (!ensure_sheet(h))
        return;

    int cellW = g_fonts[i].cellW;
    int cellH = g_fonts[i].cellH;
    int sheetCols = g_fonts[i].atlasCols;
    if (sheetCols < 1) sheetCols = 1;
    int idx = (int)(unsigned char)(c - ' ');
    int col = idx % sheetCols;
    int row = idx / sheetCols;

    gfx_blit(g_fonts[i].sheet,
             dx, dy,
             col * cellW, row * cellH,
             cellW, cellH,
             255, 0, 0, 0.0f, scale, scale, 0, tint);
}

int font_draw_text_scaled(int h, int x0, int y0, const char *text,
                          unsigned int tint, float scale)
{
    if (h < 1 || h > FONT_MAX || !g_fonts[h - 1].used || !text)
        return x0;
    if (!(scale > 0.0f))
        scale = 1.0f;

    int i = h - 1;
    if (g_fonts[i].kind == FONT_KIND_PGF)
        return pgf_draw_text_scaled(g_fonts[i].pgf, x0, y0, text, tint, scale);

    int cellH = g_fonts[i].cellH;
    int lineStep = (int)((float)cellH * scale + 0.5f);
    if (lineStep < 1) lineStep = 1;
    float x = (float)x0;
    int y = y0;

    if (g_fonts[i].kind == FONT_KIND_SFONT) {
        for (size_t n = 0; text[n]; n++) {
            unsigned char ch = (unsigned char)text[n];
            if (ch == '\n') {
                y += lineStep;
                x = (float)x0;
            } else if (ch == '\t') {
                x += (float)g_fonts[i].spaceW * 4.0f * scale;
            } else if (ch == ' ') {
                x += (float)g_fonts[i].spaceW * scale;
            } else if (ch >= 33 &&
                       (unsigned int)(ch - 33) < (unsigned int)g_fonts[i].nChars) {
                int idx = (int)ch - 33;
                font_draw_glyph_scaled(h, (int)(x + 0.5f), y, (char)ch, tint,
                                       scale);
                x += (float)g_fonts[i].glyphAdv[idx] * scale;
            }
        }
        return (int)(x + 0.5f);
    }

    int cellW = g_fonts[i].cellW;
    for (size_t n = 0; text[n]; n++) {
        unsigned char ch = (unsigned char)text[n];
        if (ch >= 32 && (unsigned int)(ch - 32) < (unsigned int)g_fonts[i].nChars) {
            font_draw_glyph_scaled(h, (int)(x + 0.5f), y, (char)ch, tint,
                                   scale);
            x += (float)cellW * scale;
        } else if (ch == '\n') {
            y += lineStep;
            x = (float)x0;
        } else if (ch == '\t') {
            x += (float)cellW * 4.0f * scale;
        }
    }
    return (int)(x + 0.5f);
}

int font_textwidth_scaled(const char *text, int h, float scale)
{
    if (!(scale > 0.0f))
        scale = 1.0f;
    return (int)((float)font_textwidth(text, h) * scale + 0.5f);
}

int font_textwidth(const char *text, int h)
{
    if (!text || h < 1 || h > FONT_MAX || !g_fonts[h - 1].used)
        return 0;

    if (g_fonts[h - 1].kind == FONT_KIND_PGF)
        return pgf_textwidth(g_fonts[h - 1].pgf, text);

    int w = 0;
    if (g_fonts[h - 1].kind == FONT_KIND_SFONT) {
        for (size_t i = 0; text[i]; i++) {
            unsigned char ch = (unsigned char)text[i];
            if (ch == '\n')
                continue;
            if (ch == '\t')
                w += g_fonts[h - 1].spaceW * 4;
            else if (ch == ' ')
                w += g_fonts[h - 1].spaceW;
            else if (ch >= 33 &&
                     (unsigned int)(ch - 33) < (unsigned int)g_fonts[h - 1].nChars)
                w += g_fonts[h - 1].glyphAdv[ch - 33];
        }
        return w;
    }

    for (size_t i = 0; text[i]; i++) {
        unsigned char ch = (unsigned char)text[i];
        if (ch == '\n' || ch == '\t')
            continue;
        if (ch < 32 || (unsigned int)(ch - 32) >= (unsigned int)g_fonts[h - 1].nChars)
            continue;
        w += g_fonts[h - 1].cellW;
    }
    return w;
}

/* ---- default font / tint (screen.font semantics) ---- */

void font_set_default(int h) { g_defaultFont = h; }
int  font_default(void)      { return g_defaultFont; }
void font_set_tint(unsigned int abgr) { g_defaultTint = abgr; }
unsigned int font_tint(void)          { return g_defaultTint; }

/* OneLua ships a built-in default font: `oneFont.pgf` beside the EBOOT,
 * loaded before the first script runs.  OpenLua follows the same convention
 * so `screen.print`/`draw.print` have a font without an explicit handle.
 *
 * The standard PSP build has no sceLibFont, so `font_alloc_pgf` falls back to
 * the sibling `oneFont.png` atlas baked from the PGF.  Loading is only
 * attempted when the file actually exists, which keeps host tests and
 * non-OneLua apps at a nil default until they call `font.setdefault`. */
static void load_default_font(void)
{
    static const char *pgf = "oneFont.pgf";
    static const char *png = "oneFont.png";

    FILE *f = fopen(pgf, "rb");
    if (f) {
        fclose(f);
        int h = font_alloc_pgf(pgf);
        if (h > 0) {
            font_set_default(h);
            return;
        }
    }

    f = fopen(png, "rb");
    if (f) {
        fclose(f);
        int h = font_alloc_sfont(png);
        if (h > 0)
            font_set_default(h);
    }
}

/* =====================================================================
 * Lua bindings
 *
 * Font API (global table `font`):
 *   font.load(path)                      -> PGF handle (int) or nil
 *   font.load(path, cellW, cellH)       -> fixed-cell PNG handle (legacy)
 *   font.new(path, cellW, cellH)        -> fixed-cell PNG handle
 *   font.setdefault(handle_or_path)     -> select screen.print font
 *   font.setdefault()                   -> current default handle (or nil)
 *   font.nchars(h)                      -> number of glyphs
 *   font.cellw(h) / font.cellh(h)       -> glyph pixel size
 *   font.draw(h, x, y, code, [tint])    -> draw single glyph
 *   font.print(h, x, y, text [, tint])  -> draw string
 *   font.textwidth(h or text, ...)      -> width in px
 *   font.free(h)                        -> release handle
 *
 * Screen hooks (added to global `screen` table):
 *   screen.font([h])                    -> set/get default font
 *   screen.setcolor([ABGR])             -> set/get text tint
 *   screen.print(x, y, text [, h, [tint]])
 *   screen.textwidth(text [, h])        -> measure string
 * ===================================================================*/

static int l_font_new(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    int cw = (int)luaL_checkinteger(L, 2);
    int ch = (int)luaL_checkinteger(L, 3);
    int h  = font_alloc(path, cw, ch);
    if (h <= 0) {
        lua_pushnil(L);
    } else {
        lua_pushinteger(L, h);
    }
    return 1;
}

static int l_font_load(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    int h;
    if (lua_gettop(L) >= 3) {
        /* Backwards-compatible alias for the old fixed-cell loader. */
        int cw = (int)luaL_checkinteger(L, 2);
        int ch = (int)luaL_checkinteger(L, 3);
        h = font_alloc(path, cw, ch);
    } else {
        h = font_alloc_pgf(path);
    }
    if (h <= 0)
        lua_pushnil(L);
    else {
        lua_pushinteger(L, h);
    }
    return 1;
}

static int l_font_setdefault(lua_State *L)
{
    if (lua_isnoneornil(L, 1)) {
        if (g_defaultFont > 0)
            lua_pushinteger(L, g_defaultFont);
        else
            lua_pushnil(L);
        return 1;
    }

    if (lua_isnumber(L, 1)) {
        int h = (int)luaL_checkinteger(L, 1);
        if (h >= 1 && h <= FONT_MAX && g_fonts[h - 1].used)
            font_set_default(h);
        return 0;
    }

    const char *path = luaL_checkstring(L, 1);
    int h = font_alloc_pgf(path);
    if (h > 0)
        font_set_default(h);
    return 0;
}

static int l_font_nchars(lua_State *L)
{
    lua_pushinteger(L, font_nchars((int)luaL_checkinteger(L, 1)));
    return 1;
}

static int l_font_cell(lua_State *L)
{
    int h = (int)luaL_checkinteger(L, 1);
    lua_pushinteger(L, font_cellw(h));
    lua_pushinteger(L, font_cellh(h));
    return 2;
}

static int l_font_draw(lua_State *L)
{
    int h  = (int)luaL_checkinteger(L, 1);
    int x  = (int)luaL_checkinteger(L, 2);
    int y  = (int)luaL_checkinteger(L, 3);
    int c  = (int)luaL_checkinteger(L, 4);
    unsigned int tint = (lua_gettop(L) >= 5)
                         ? (unsigned int)luaL_checkinteger(L, 5)
                         : 0xFFFFFFFFu;
    font_draw_glyph(h, x, y, (char)c, tint);
    return 0;
}

static int l_font_print(lua_State *L)
{
    int h    = (int)luaL_checkinteger(L, 1);
    int x    = (int)luaL_checkinteger(L, 2);
    int y    = (int)luaL_checkinteger(L, 3);
    const char *text = luaL_checkstring(L, 4);
    unsigned int tint = (lua_gettop(L) >= 5)
                         ? (unsigned int)luaL_checkinteger(L, 5)
                         : 0xFFFFFFFFu;
    int endX = font_draw_text(h, x, y, text, tint);
    lua_pushinteger(L, endX);            /* final cursor X (OneLua print returns pos) */
    return 1;
}

static int l_font_textwidth(lua_State *L)
{
    /* one-arg form: font.textwidth(h, text)  /  two-arg: text, h */
    const char *a;
    int h;
    if (lua_isnumber(L, 1)) { h = (int)luaL_checkinteger(L, 1); a = luaL_checkstring(L, 2); }
    else                     { a = luaL_checkstring(L, 1);       h = (int)luaL_checkinteger(L, 2); }
    lua_pushinteger(L, font_textwidth(a, h));
    return 1;
}

static int l_font_free(lua_State *L)
{
    font_free((int)luaL_checkinteger(L, 1));
    return 0;
}

/* ---- OneLua SFONT compatibility table ---- */

static int l_sfont_load(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    int h = font_alloc_sfont(path);
    if (h <= 0)
        lua_pushnil(L);
    else {
        lua_pushinteger(L, h);
    }
    return 1;
}

static int l_sfont_print(lua_State *L)
{
    int h = (int)luaL_checkinteger(L, 1);
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);
    const char *text = luaL_checkstring(L, 4);
    lua_pushinteger(L, font_draw_text(h, x, y, text, 0xFFFFFFFFu));
    return 1;
}

static int l_sfont_letters(lua_State *L)
{
    lua_pushinteger(L, font_nchars((int)luaL_checkinteger(L, 1)));
    return 1;
}

static int l_sfont_width(lua_State *L)
{
    int h = (int)luaL_checkinteger(L, 1);
    const char *text = luaL_checkstring(L, 2);
    lua_pushinteger(L, font_textwidth(text, h));
    return 1;
}

/* ---- screen table hooks ---- */

static int l_screen_font(lua_State *L)
{
    if (lua_isnoneornil(L, 1)) {
        lua_pushinteger(L, g_defaultFont);
        return 1;                        /* getter */
    }
    int h = (int)luaL_checkinteger(L, 1);
    font_set_default(h);
    return 0;
}

static int l_screen_setcolor(lua_State *L)
{
    if (lua_isnoneornil(L, 1)) {
        lua_pushinteger(L, (lua_Integer)g_defaultTint);
        return 1;
    }
    font_set_tint((unsigned int)luaL_checkinteger(L, 1));
    return 0;
}

/* OneLua alignment / shadow-style constants. */
enum {
    OL_ALIGN_LEFT   = 0,
    OL_ALIGN_CENTER = 1,
    OL_ALIGN_RIGHT  = 2,
    OL_ALIGN_FULL   = 3
};

static int ol_is_font_handle(lua_State *L, int idx)
{
    if (!lua_isnumber(L, idx))
        return 0;
    lua_Number n = lua_tonumber(L, idx);
    int h = (int)n;
    if ((lua_Number)h != n)
        return 0;
    return h >= 1 && h <= FONT_MAX && g_fonts[h - 1].used;
}

/* Draw a (possibly multi-line) string with per-line alignment. */
static int print_block(int h, const char *text, int x, int y,
                       unsigned int tint, float scale, int align)
{
    int step = (int)((float)font_cellh(h) * scale + 0.5f);
    if (step < 1)
        step = 1;

    char stackbuf[256];
    const char *p = text;
    int lineY = y;
    int endX = x;

    for (;;) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);

        /* Keep the common short-line case allocation-free, but do not
         * truncate long lines: measure/draw the whole line. */
        char *buf = stackbuf;
        if (len >= sizeof(stackbuf)) {
            buf = (char *)malloc(len + 1);
            if (!buf) {
                /* Out of memory: draw the unaligned remainder rather than
                 * dropping text. */
                endX = font_draw_text_scaled(h, x, lineY, p, tint, scale);
                break;
            }
        }
        memcpy(buf, p, len);
        buf[len] = '\0';

        int lx = x;
        if (align == OL_ALIGN_CENTER || align == OL_ALIGN_RIGHT) {
            int w = font_textwidth_scaled(buf, h, scale);
            lx -= (align == OL_ALIGN_CENTER) ? w / 2 : w;
        }

        int ex = font_draw_text_scaled(h, lx, lineY, buf, tint, scale);
        if (lx == x)
            endX = ex;

        if (buf != stackbuf)
            free(buf);

        if (!nl)
            break;
        p = nl + 1;
        lineY += step;
    }
    return endX;
}

static int l_screen_print(lua_State *L)
{
    int top = lua_gettop(L);
    int x, y, h = 0, first;
    const char *text;

    /* OneLua font-first form:
     *   screen.print(font, x, y, text [, size [, color [, shadow [, align]]]])
     * Legacy OpenLua form:
     *   screen.print(x, y, text [, handle [, tint]]) */
    if (top >= 4 && lua_type(L, 1) == LUA_TNUMBER &&
        lua_type(L, 4) == LUA_TSTRING) {
        h = (int)luaL_checkinteger(L, 1);
        x = (int)luaL_checkinteger(L, 2);
        y = (int)luaL_checkinteger(L, 3);
        text = luaL_checkstring(L, 4);
        first = 5;
    } else {
        x = (int)luaL_checkinteger(L, 1);
        y = (int)luaL_checkinteger(L, 2);
        text = luaL_checkstring(L, 3);
        first = 4;
    }

    float scale = 1.0f;
    unsigned int tint = g_defaultTint;
    unsigned int shadow = 0;
    int align = OL_ALIGN_LEFT;

    if (h <= 0 && top >= first && ol_is_font_handle(L, first) &&
        top <= first + 1) {
        /* Legacy: handle, optionally followed by a tint. */
        h = (int)luaL_checkinteger(L, first);
        if (top >= first + 1 && lua_isnumber(L, first + 1))
            tint = (unsigned int)luaL_checkinteger(L, first + 1);
    } else {
        if (top >= first && lua_isnumber(L, first))
            scale = (float)lua_tonumber(L, first);
        if (top >= first + 1 && lua_isnumber(L, first + 1))
            tint = (unsigned int)luaL_checkinteger(L, first + 1);
        if (top >= first + 2 && lua_isnumber(L, first + 2))
            shadow = (unsigned int)luaL_checkinteger(L, first + 2);
        if (top >= first + 3 && lua_isnumber(L, first + 3))
            align = (int)luaL_checkinteger(L, first + 3);
    }

    if (h <= 0)
        h = g_defaultFont;
    if (h <= 0)
        return luaL_error(L, "screen.print: no font set (call screen.font(h) or pass a font handle)");
    if (!(scale > 0.0f))
        scale = 1.0f;

    if (shadow != 0) {
        int off = (int)(scale + 0.5f);
        if (off < 1)
            off = 1;
        print_block(h, text, x + off, y + off, shadow, scale, align);
    }

    int endX = print_block(h, text, x, y, tint, scale, align);
    lua_pushinteger(L, endX);
    return 1;
}

static int l_screen_textwidth(lua_State *L)
{
    int top = lua_gettop(L);
    const char *text;
    int h = g_defaultFont;
    float scale = 1.0f;

    if (lua_type(L, 1) == LUA_TNUMBER && top >= 2 &&
        lua_type(L, 2) == LUA_TSTRING) {
        /* OneLua: screen.textwidth(font, text [, size]) */
        h = (int)luaL_checkinteger(L, 1);
        text = luaL_checkstring(L, 2);
        if (top >= 3 && lua_isnumber(L, 3))
            scale = (float)lua_tonumber(L, 3);
    } else {
        text = luaL_checkstring(L, 1);
        if (top >= 2 && lua_isnumber(L, 2)) {
            if (ol_is_font_handle(L, 2))
                h = (int)luaL_checkinteger(L, 2);   /* legacy text, handle */
            else
                scale = (float)lua_tonumber(L, 2);  /* OneLua text, size */
        }
    }

    lua_pushinteger(L, font_textwidth_scaled(text, h, scale));
    return 1;
}

static int l_screen_textheight(lua_State *L)
{
    int top = lua_gettop(L);
    int h = g_defaultFont;
    float scale = 1.0f;

    if (top >= 2 && lua_type(L, 1) == LUA_TNUMBER && lua_isnumber(L, 2)) {
        /* OneLua: screen.textheight(font, size) */
        h = (int)luaL_checkinteger(L, 1);
        scale = (float)lua_tonumber(L, 2);
    } else if (top >= 1 && lua_isnumber(L, 1)) {
        /* OneLua: screen.textheight(font) or screen.textheight(size) */
        if (ol_is_font_handle(L, 1))
            h = (int)luaL_checkinteger(L, 1);
        else
            scale = (float)lua_tonumber(L, 1);
    }

    lua_pushinteger(L, (int)((float)font_cellh(h) * scale + 0.5f));
    return 1;
}

static const luaL_Reg font_funcs[] = {
    { "new",          l_font_new },
    { "load",         l_font_load },
    { "setdefault",   l_font_setdefault },
    { "nchars",       l_font_nchars },
    { "cell",         l_font_cell },
    { "draw",         l_font_draw },
    { "print",        l_font_print },
    { "textwidth",    l_font_textwidth },
    { "free",         l_font_free },
    { NULL,           NULL }
};

static const luaL_Reg sfont_funcs[] = {
    { "load",         l_sfont_load },
    { "print",        l_sfont_print },
    { "letters",      l_sfont_letters },
    { "width",        l_sfont_width },
    { NULL,           NULL }
};

static const luaL_Reg screen_font_funcs[] = {
    { "font",         l_screen_font },
    { "setcolor",     l_screen_setcolor },
    { "print",        l_screen_print },
    { "textwidth",    l_screen_textwidth },
    { "textheight",   l_screen_textheight },
    { NULL,           NULL }
};

void font_api_register(lua_State *L)
{
    /* ---- font table ---- */
    lua_newtable(L);
    ol_setfuncs(L, font_funcs);
    lua_setglobal(L, "font");

    /* ---- OneLua SFONT compatibility table ---- */
    lua_newtable(L);
    ol_setfuncs(L, sfont_funcs);
    lua_setglobal(L, "sfont");

    /* OneLua-style built-in default: oneFont.pgf beside the EBOOT. */
    load_default_font();

    /* ---- hooks onto existing `screen` table ---- */
    lua_getglobal(L, "screen");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;                           /* screen table not created yet */
    }
    ol_setfuncs(L, screen_font_funcs);
    lua_pop(L, 1);

    /* OneLua alignment constants (used as the last screen.print argument). */
    lua_pushinteger(L, OL_ALIGN_LEFT);   lua_setglobal(L, "__ALEFT");
    lua_pushinteger(L, OL_ALIGN_CENTER); lua_setglobal(L, "__ACENTER");
    lua_pushinteger(L, OL_ALIGN_RIGHT);  lua_setglobal(L, "__ARIGHT");
    lua_pushinteger(L, OL_ALIGN_FULL);   lua_setglobal(L, "__AFULL");

    /* OneLua shadow-style constants.  OpenLua treats the shadow argument
     * as a color, so these remain available for scripts that reference
     * them but have no geometric meaning here. */
    lua_pushinteger(L, 0); lua_setglobal(L, "__SLEFT");
    lua_pushinteger(L, 1); lua_setglobal(L, "__SRIGHT");
    lua_pushinteger(L, 2); lua_setglobal(L, "__STHROUGH");
    lua_pushinteger(L, 3); lua_setglobal(L, "__SSEESAW");
}
