/* OpenLua - bitmap font system implementation
 *
 * Fonts are loaded from a PNG spritesheet: a grid of cells, each
 * cellW x cellH pixels. Characters map sequentially starting from
 * char 0x20 (space): glyph 0 = ' ', glyph 1 = '!', glyph 2 = '"', ...
 *
 * The full sheet is uploaded to VRAM as a single power-of-two texture
 * (lazily, on first draw). Drawing uses pixel-unit UV coords so a
 * glyph blit is simply: gfx_blit(sheet, x, y, col*cellW, row*cellH,
 *                            cellW, cellH, ...).
 * Sheets wider or taller than the GE limit (512 px) are repacked cell by
 * cell into an atlas at most 512 px wide; col/row then come from the
 * atlas width instead of the source sheet's.
 *
 * SFont strips (sfont.load) reuse the same slots and blit path: row 0
 * holds magenta markers, each glyph is a non-magenta run between two
 * markers, and glyphs are drawn from rows 1..h-1 at their own width.
 * Space and characters the strip does not cover advance by the width of
 * the first glyph ('!'), as in the original SFont library.
 *
 * Pixel format matches image.c: stbi RGBA bytes read as uint32 LE,
 * i.e. pixel value = (A<<24)|(B<<16)|(G<<8)|R.
 */

#include "engine/font/font.h"
#include "engine/gfx/gfx.h"
#include "engine/core/olreg.h"

#include <string.h>
#include <stdlib.h>

/* stb_image implementation lives in engine/core/stb_impl.o */
#include "third_party/stb/stb_image.h"

#define FONT_MAX 8

#define SFONT_MAGENTA 0x00FF00FFu   /* R=FF G=00 B=FF in ABGR, alpha ignored */
#define SFONT_ATLAS_MAX 512         /* GE texture size limit (all fonts) */

static struct {
    int        used;
    char       path[256];
    int        cellW, cellH;  /* SFont: widest glyph, glyph height */
    int        nChars;
    OL_Surface *sheet;        /* VRAM texture (lazy upload) */
    int        sfont;         /* 1 = proportional SFont strip */
    int        spaceW;        /* SFont: advance for space/unknown chars */
    int        keyed;         /* SFont: colour key active */
    unsigned int key;         /* SFont: bottom-left colour made transparent */
    SFontGlyph glyphs[SFONT_MAX_GLYPHS];
    unsigned short ax[SFONT_MAX_GLYPHS], ay[SFONT_MAX_GLYPHS]; /* atlas pos */
    int        atlasW, atlasH;
    int        repack;        /* grid: cells remapped into an atlas */
} g_fonts[FONT_MAX];

static int             g_defaultFont = 0;             /* for screen.print */
static unsigned int    g_defaultTint = 0xFFFFFFFFu;   /* ABGR, FFFFFF = none */

/* =====================================================================
 * Allocation
 * ===================================================================*/

static int slot_claim(const char *path)
{
    for (int i = 0; i < FONT_MAX; i++) {
        if (!g_fonts[i].used) {
            memset(&g_fonts[i], 0, sizeof(g_fonts[i]));
            g_fonts[i].used = 1;
            strncpy(g_fonts[i].path, path, sizeof(g_fonts[i].path) - 1);
            return i;
        }
    }
    return -1;                              /* all slots full */
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

    /* Too big for the GE: lay the cells out row-major in an atlas at most
     * SFONT_ATLAS_MAX wide (cellW <= 256, so at least two columns). */
    int repack = w > SFONT_ATLAS_MAX || h > SFONT_ATLAS_MAX;
    int aw = 0, ah = 0;
    if (repack) {
        int cols = SFONT_ATLAS_MAX / cellW;
        if (cols > nChars)
            cols = nChars;
        aw = cols * cellW;
        ah = (nChars + cols - 1) / cols * cellH;
        if (ah > SFONT_ATLAS_MAX)
            return 0;
    }

    int i = slot_claim(path);
    if (i < 0)
        return 0;
    g_fonts[i].cellW  = cellW;
    g_fonts[i].cellH  = cellH;
    g_fonts[i].nChars = nChars;
    g_fonts[i].repack = repack;
    g_fonts[i].atlasW = aw;
    g_fonts[i].atlasH = ah;
    return i + 1;                           /* 1-based handle */
}

int sfont_parse_markers(const unsigned int *row, int w,
                        SFontGlyph *out, int max)
{
    int n = 0, x = 0;

    /* Anything left of the first marker is not a glyph. */
    while (x < w && (row[x] & 0x00FFFFFFu) != SFONT_MAGENTA)
        x++;
    while (x < w && n < max) {
        while (x < w && (row[x] & 0x00FFFFFFu) == SFONT_MAGENTA)
            x++;
        int start = x;
        while (x < w && (row[x] & 0x00FFFFFFu) != SFONT_MAGENTA)
            x++;
        if (x >= w)
            break;                          /* run not closed by a marker */
        out[n].x = (unsigned short)start;
        out[n].w = (unsigned short)(x - start);
        n++;
    }
    return n;
}

/* Shelf-pack n glyphs of height gh into an atlas at most maxW wide, since
 * SFont strips are usually far wider than the GE can sample. Writes each
 * glyph's atlas origin and the atlas size; returns 0 if it cannot fit. */
static int sfont_pack(const SFontGlyph *g, int n, int gh, int maxW,
                      unsigned short *ax, unsigned short *ay,
                      int *outW, int *outH)
{
    int x = 0, y = 0, w = 0;
    for (int i = 0; i < n; i++) {
        if (g[i].w > maxW)
            return 0;
        if (x + g[i].w > maxW) {
            x = 0;
            y += gh;
        }
        ax[i] = (unsigned short)x;
        ay[i] = (unsigned short)y;
        x += g[i].w;
        if (x > w)
            w = x;
    }
    if (y + gh > maxW)
        return 0;
    *outW = w > 0 ? w : 1;
    *outH = y + gh;
    return 1;
}

int sfont_alloc(const char *path)
{
    int w = 0, h = 0, comp = 0;
    unsigned char *rgba = stbi_load(path, &w, &h, &comp, 4);
    if (!rgba)
        return 0;
    if (h < 2 || w > 0xFFFF) {
        stbi_image_free(rgba);
        return 0;
    }

    SFontGlyph glyphs[SFONT_MAX_GLYPHS];
    const unsigned int *px = (const unsigned int *)rgba;
    int n = sfont_parse_markers(px, w, glyphs, SFONT_MAX_GLYPHS);
    unsigned int key = px[(size_t)(h - 1) * w];
    stbi_image_free(rgba);
    if (n <= 0)
        return 0;

    unsigned short ax[SFONT_MAX_GLYPHS], ay[SFONT_MAX_GLYPHS];
    int aw = 0, ah = 0;
    if (!sfont_pack(glyphs, n, h - 1, SFONT_ATLAS_MAX, ax, ay, &aw, &ah))
        return 0;

    int i = slot_claim(path);
    if (i < 0)
        return 0;
    int widest = 1;
    for (int g = 0; g < n; g++)
        if (glyphs[g].w > widest)
            widest = glyphs[g].w;
    memcpy(g_fonts[i].glyphs, glyphs, sizeof(glyphs[0]) * (size_t)n);
    memcpy(g_fonts[i].ax, ax, sizeof(ax[0]) * (size_t)n);
    memcpy(g_fonts[i].ay, ay, sizeof(ay[0]) * (size_t)n);
    g_fonts[i].sfont  = 1;
    g_fonts[i].atlasW = aw;
    g_fonts[i].atlasH = ah;
    g_fonts[i].nChars = n;
    g_fonts[i].cellW  = widest;
    g_fonts[i].cellH  = h - 1;
    g_fonts[i].spaceW = glyphs[0].w;
    /* SFont's transparent colour is the bottom-left pixel; PNGs that
     * already carry alpha there need no key. */
    g_fonts[i].keyed  = (key >> 24) == 0xFF;
    g_fonts[i].key    = key;
    return i + 1;
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
    if (g_fonts[h - 1].sheet)
        gfx_surface_free(g_fonts[h - 1].sheet);
    g_fonts[h - 1].sheet = 0;
    g_fonts[h - 1].used  = 0;
    if (g_defaultFont == h)
        g_defaultFont = 0;
}

/* =====================================================================
 * Lazy VRAM upload: decode PNG -> power-of-two GPU surface.
 * Identical pixel convention to image.c (RGBA bytes as uint32 LE).
 * ===================================================================*/

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

    OL_Surface *s;
    if (g_fonts[i].sfont) {
        /* Copy each glyph (rows 1..h-1) to its packed atlas position. */
        int gh = g_fonts[i].cellH;
        s = (hh == gh + 1)
            ? gfx_surface_new(g_fonts[i].atlasW, g_fonts[i].atlasH) : 0;
        for (int g = 0; s && g < g_fonts[i].nChars; g++) {
            const SFontGlyph *gl = &g_fonts[i].glyphs[g];
            for (int y = 0; y < gh; y++) {
                const unsigned int *row = src + (size_t)(y + 1) * w + gl->x;
                unsigned int *dst = s->pixels
                    + (size_t)(g_fonts[i].ay[g] + y) * s->realW
                    + g_fonts[i].ax[g];
                for (int x = 0; x < gl->w; x++) {
                    unsigned int p = row[x];
                    dst[x] = (g_fonts[i].keyed && p == g_fonts[i].key) ? 0 : p;
                }
            }
        }
    } else if (g_fonts[i].repack) {
        /* Copy each cell from its sheet slot to its atlas slot. */
        int cw = g_fonts[i].cellW, ch = g_fonts[i].cellH;
        int srcCols = w / cw;
        int dstCols = g_fonts[i].atlasW / cw;
        s = (srcCols * (hh / ch) >= g_fonts[i].nChars)
            ? gfx_surface_new(g_fonts[i].atlasW, g_fonts[i].atlasH) : 0;
        for (int g = 0; s && g < g_fonts[i].nChars; g++) {
            const unsigned int *from = src
                + (size_t)(g / srcCols) * ch * w + (size_t)(g % srcCols) * cw;
            unsigned int *to = s->pixels
                + (size_t)(g / dstCols) * ch * s->realW + (size_t)(g % dstCols) * cw;
            for (int y = 0; y < ch; y++)
                memcpy(to + (size_t)y * s->realW, from + (size_t)y * w,
                       (size_t)cw * 4);
        }
    } else {
        /* gfx_surface_new zeroes the power-of-two padding. */
        s = gfx_surface_new(w, hh);
        for (int y = 0; s && y < hh; y++)
            memcpy(s->pixels + (size_t)y * s->realW, src + (size_t)y * w,
                   (size_t)w * 4);
    }
    stbi_image_free(rgba);
    if (!s)
        return 0;
    g_fonts[i].sheet = s;
    return 1;
}

/* =====================================================================
 * Drawing (goes directly into the GE scene list, like image.blit)
 * ===================================================================*/

/* Glyph index for byte ch, or -1 when the font has no glyph for it. */
static int glyph_index(int i, unsigned char ch)
{
    int first = g_fonts[i].sfont ? SFONT_FIRST : ' ';
    if (ch < first || ch - first >= g_fonts[i].nChars)
        return -1;
    return ch - first;
}

/* Cursor advance for byte ch ('\n' and '\t' are handled by callers). */
static int glyph_advance(int i, unsigned char ch)
{
    int idx = glyph_index(i, ch);
    if (!g_fonts[i].sfont)
        return idx < 0 ? 0 : g_fonts[i].cellW;
    if (idx >= 0)
        return g_fonts[i].glyphs[idx].w;
    return ch >= ' ' ? g_fonts[i].spaceW : 0;
}

void font_draw_glyph(int h, int dx, int dy, char c, unsigned int tint)
{
    if (h < 1 || h > FONT_MAX || !g_fonts[h - 1].used)
        return;
    int idx = glyph_index(h - 1, (unsigned char)c);
    if (idx < 0)
        return;
    if (!ensure_sheet(h))
        return;

    int i     = h - 1;
    if (g_fonts[i].sfont) {
        const SFontGlyph *g = &g_fonts[i].glyphs[idx];
        gfx_blit(g_fonts[i].sheet, dx, dy, g_fonts[i].ax[idx], g_fonts[i].ay[idx],
                 g->w, g_fonts[i].cellH, 255, 0, 0, 0.0f, 1.0f, 1.0f, 0, tint);
        return;
    }

    int cellW = g_fonts[i].cellW;
    int cellH = g_fonts[i].cellH;

    /* Grid math (same rule as image.c blitsprite/frame_rect): the
     * column count comes from the sheet's hardware-aligned width. For a
     * repacked sheet that is the atlas width, which remaps col/row. */
    int sheetCols = g_fonts[i].sheet->w / cellW;
    if (sheetCols < 1) sheetCols = 1;
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

    int f     = h - 1;
    int cellH = g_fonts[f].cellH;
    int tabW  = 4 * (g_fonts[f].sfont ? g_fonts[f].spaceW : g_fonts[f].cellW);
    int x = x0, y = y0;

    for (size_t i = 0; text[i]; i++) {
        unsigned char ch = (unsigned char)text[i];
        if (ch == '\n') {
            y += cellH;
            x = x0;
        } else if (ch == '\t') {
            x += tabW;
        } else {
            font_draw_glyph(h, x, y, (char)ch, tint);
            x += glyph_advance(f, ch);
        }
    }
    return x;
}

int font_textwidth(const char *text, int h)
{
    if (!text || h < 1 || h > FONT_MAX || !g_fonts[h - 1].used)
        return 0;
    int w = 0;
    for (size_t i = 0; text[i]; i++) {
        unsigned char ch = (unsigned char)text[i];
        if (ch == '\n' || ch == '\t')
            continue;
        w += glyph_advance(h - 1, ch);
    }
    return w;
}

/* ---- default font / tint (screen.font semantics) ---- */

void font_set_default(int h) { g_defaultFont = h; }
int  font_default(void)      { return g_defaultFont; }
void font_set_tint(unsigned int abgr) { g_defaultTint = abgr; }
unsigned int font_tint(void)          { return g_defaultTint; }

/* =====================================================================
 * Lua bindings
 *
 * Font API (global table `font`):
 *   font.new(path, cellW, cellH)        -> handle (int) or nil
 *   font.nchars(h)                      -> number of glyphs
 *   font.cellw(h) / font.cellh(h)       -> glyph pixel size
 *   font.draw(h, x, y, code, [tint])    -> draw single glyph
 *   font.print(h, x, y, text [, tint])  -> draw string
 *   font.textwidth(h or text, ...)      -> width in px
 *   font.free(h)                        -> release handle
 *
 * OneLua SFont API (global table `sfont`, same handles as `font`):
 *   sfont.load(path)                    -> handle (int) or nil
 *   sfont.print(h, x, y, text [, tint]) -> final cursor X
 *   sfont.letters(h)                    -> number of glyphs in the strip
 *   sfont.width(h, text)                -> width in px
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

static int l_sfont_load(lua_State *L)
{
    int h = sfont_alloc(luaL_checkstring(L, 1));
    if (h <= 0)
        lua_pushnil(L);
    else
        lua_pushinteger(L, h);
    return 1;
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

static int l_screen_print(lua_State *L)
{
    int x    = (int)luaL_checkinteger(L, 1);
    int y    = (int)luaL_checkinteger(L, 2);
    const char *text = luaL_checkstring(L, 3);

    int h = g_defaultFont;
    if (lua_isnumber(L, 4))
        h = (int)luaL_checkinteger(L, 4);
    if (h <= 0)
        return luaL_error(L, "screen.print: no font set (call screen.font(h) or pass a font handle)");

    unsigned int tint = g_defaultTint;
    if (lua_isnumber(L, 5))
        tint = (unsigned int)luaL_checkinteger(L, 5);

    int endX = font_draw_text(h, x, y, text, tint);
    lua_pushinteger(L, endX);
    return 1;
}

static int l_screen_textwidth(lua_State *L)
{
    const char *text = luaL_checkstring(L, 1);
    int h = g_defaultFont;
    if (lua_isnumber(L, 2))
        h = (int)luaL_checkinteger(L, 2);
    lua_pushinteger(L, font_textwidth(text, h));
    return 1;
}

static const luaL_Reg font_funcs[] = {
    { "new",          l_font_new },
    { "load",         l_font_new },
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
    { "print",        l_font_print },
    { "letters",      l_font_nchars },
    { "width",        l_font_textwidth },
    { NULL,           NULL }
};

static const luaL_Reg screen_font_funcs[] = {
    { "font",         l_screen_font },
    { "setcolor",     l_screen_setcolor },
    { "print",        l_screen_print },
    { "textwidth",    l_screen_textwidth },
    { NULL,           NULL }
};

void font_api_register(lua_State *L)
{
    /* ---- font table ---- */
    lua_newtable(L);
    ol_setfuncs(L, font_funcs);
    lua_setglobal(L, "font");

    lua_newtable(L);
    ol_setfuncs(L, sfont_funcs);
    lua_setglobal(L, "sfont");

    /* ---- hooks onto existing `screen` table ---- */
    lua_getglobal(L, "screen");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;                           /* screen table not created yet */
    }
    ol_setfuncs(L, screen_font_funcs);
    lua_pop(L, 1);
}
