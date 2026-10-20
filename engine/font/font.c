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

static struct {
    int        used;
    char       path[256];
    int        cellW, cellH;
    int        nChars;
    OL_Surface *sheet;        /* VRAM texture (lazy upload) */
} g_fonts[FONT_MAX];

static int             g_defaultFont = 0;             /* for screen.print */
static unsigned int    g_defaultTint = 0xFFFFFFFFu;   /* ABGR, FFFFFF = none */

/* =====================================================================
 * Allocation
 * ===================================================================*/

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

    for (int i = 0; i < FONT_MAX; i++) {
        if (!g_fonts[i].used) {
            g_fonts[i].used   = 1;
            strncpy(g_fonts[i].path, path, sizeof(g_fonts[i].path) - 1);
            g_fonts[i].path[sizeof(g_fonts[i].path) - 1] = '\0';
            g_fonts[i].cellW  = cellW;
            g_fonts[i].cellH  = cellH;
            g_fonts[i].nChars = nChars;
            g_fonts[i].sheet  = 0;
            return i + 1;                    /* 1-based handle */
        }
    }
    return 0;                               /* all slots full */
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

    {
        int i = h - 1;
        if (g_fonts[i].sheet)
            return 1;

        int w = 0, hh = 0, comp = 0;
        unsigned char *rgba = stbi_load(g_fonts[i].path, &w, &hh, &comp, 4);
        if (!rgba)
            return 0;

        OL_Surface *s = gfx_surface_new(w, hh);
        if (!s) {
            stbi_image_free(rgba);
            return 0;
        }

        for (int y = 0; y < hh; y++) {
            const unsigned int *row = (const unsigned int *)(rgba + (size_t)y * w * 4);
            for (int x = 0; x < w; x++)
                s->pixels[y * s->realW + x] = row[x];
        }
        /* Zero the power-of-two padding strip outside the logical size. */
        for (int i2 = w * hh; i2 < s->realW * s->realH; i2++)
            s->pixels[i2] = 0;

        g_fonts[i].sheet = s;
        stbi_image_free(rgba);
    }
    return 1;
}

/* =====================================================================
 * Drawing (goes directly into the GE scene list, like image.blit)
 * ===================================================================*/

void font_draw_glyph(int h, int dx, int dy, char c, unsigned int tint)
{
    if (h < 1 || h > FONT_MAX || !g_fonts[h - 1].used)
        return;
    if ((unsigned int)(c - ' ') >= (unsigned int)g_fonts[h - 1].nChars)
        return;
    if (!ensure_sheet(h))
        return;

    int i     = h - 1;
    int cellW = g_fonts[i].cellW;
    int cellH = g_fonts[i].cellH;

    /* Grid math (same rule as image.c blitsprite/frame_rect): the
     * column count comes from the sheet's hardware-aligned width. */
    int sheetCols = g_fonts[i].sheet->w / cellW;
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

    int cellW = g_fonts[h - 1].cellW;
    int cellH = g_fonts[h - 1].cellH;
    int x = x0, y = y0;

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

int font_textwidth(const char *text, int h)
{
    if (!text || h < 1 || h > FONT_MAX || !g_fonts[h - 1].used)
        return 0;
    int w = 0;
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

    /* ---- hooks onto existing `screen` table ---- */
    lua_getglobal(L, "screen");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;                           /* screen table not created yet */
    }
    ol_setfuncs(L, screen_font_funcs);
    lua_pop(L, 1);
}
