/* Host tests for the font module: SFont marker parsing, atlas packing,
 * proportional layout and the unchanged grid-font path. The GE calls are
 * stubbed and every gfx_blit is recorded. With argv[1] (a Lua script) and
 * argv[2] (the assets directory) it then runs the Lua sfont suite. */
#include "engine/font/font.c"
#include "third_party/stb/stb_image_write.h"

#include <lualib.h>
#include <stdio.h>

static int checks;
static int fails;

static void check(int cond, const char *msg)
{
    checks++;
    if (!cond) {
        fails++;
        printf("  FAIL: %s\n", msg);
    }
}

/* ---- gfx stubs ---- */

static int next_pow2(int v)
{
    int p = 1;
    while (p < v)
        p <<= 1;
    return p;
}

OL_Surface *gfx_surface_new(int w, int h)
{
    OL_Surface *s = (OL_Surface *)calloc(1, sizeof(*s));
    s->w = w;
    s->h = h;
    s->realW = next_pow2(w);
    s->realH = next_pow2(h);
    s->pixels = (unsigned int *)calloc((size_t)s->realW * s->realH, 4);
    return s;
}

void gfx_surface_free(OL_Surface *s)
{
    free(s->pixels);
    free(s);
}

static struct {
    int x, y, sx, sy, sw, sh;
    unsigned tint;
} g_blit[64];
static int g_nblit;

void gfx_blit(OL_Surface *s, int x, int y, int sx, int sy, int sw, int sh,
              int alpha, int flipX, int flipY, float angle, float scaleX,
              float scaleY, int z, unsigned tint)
{
    (void)s; (void)alpha; (void)flipX; (void)flipY; (void)angle;
    (void)scaleX; (void)scaleY; (void)z;
    if (g_nblit < 64) {
        g_blit[g_nblit].x = x;
        g_blit[g_nblit].y = y;
        g_blit[g_nblit].sx = sx;
        g_blit[g_nblit].sy = sy;
        g_blit[g_nblit].sw = sw;
        g_blit[g_nblit].sh = sh;
        g_blit[g_nblit].tint = tint;
    }
    g_nblit++;
}

/* ---- helpers ---- */

#define MAG 0xFFFF00FFu   /* opaque magenta, ABGR */
#define BLK 0xFF000000u   /* opaque black background (colour key) */
#define WHT 0xFFFFFFFFu

static void test_parse(void)
{
    SFontGlyph g[8];

    unsigned int a[] = { MAG, 0, 0, 0, MAG, 0, 0, MAG, MAG, 0, MAG };
    int n = sfont_parse_markers(a, 11, g, 8);
    check(n == 3, "three runs between markers");
    check(g[0].x == 1 && g[0].w == 3, "first glyph x/w");
    check(g[1].x == 5 && g[1].w == 2, "second glyph x/w");
    check(g[2].x == 9 && g[2].w == 1, "glyph after double-width marker");

    unsigned int b[] = { 0, 0, MAG, 0, 0, MAG, 0, 0 };
    n = sfont_parse_markers(b, 8, g, 8);
    check(n == 1, "leading and unclosed trailing runs ignored");
    check(g[0].x == 3 && g[0].w == 2, "only the enclosed run counts");

    unsigned int c[] = { 0x00FF00FFu, 0, 0x80FF00FFu };
    n = sfont_parse_markers(c, 3, g, 8);
    check(n == 1 && g[0].x == 1 && g[0].w == 1, "marker alpha ignored");

    unsigned int d[] = { 0, WHT, 0xFFFF00FEu, 0 };
    check(sfont_parse_markers(d, 4, g, 8) == 0, "no markers = no glyphs");
    check(sfont_parse_markers(a, 0, g, 8) == 0, "empty row");
    check(sfont_parse_markers(a, 11, g, 2) == 2, "max caps glyph count");
}

static void test_pack(void)
{
    SFontGlyph g[4] = { { 0, 5 }, { 0, 4 }, { 0, 6 }, { 0, 3 } };
    unsigned short ax[4], ay[4];
    int w = 0, h = 0;

    check(sfont_pack(g, 4, 4, 10, ax, ay, &w, &h), "pack fits");
    check(ax[0] == 0 && ay[0] == 0, "glyph 0 at origin");
    check(ax[1] == 5 && ay[1] == 0, "glyph 1 on first shelf");
    check(ax[2] == 0 && ay[2] == 4, "glyph 2 wraps to second shelf");
    check(ax[3] == 6 && ay[3] == 4, "glyph 3 follows on second shelf");
    check(w == 9 && h == 8, "atlas size");

    check(sfont_pack(g, 4, 10, 100, ax, ay, &w, &h) && w == 18 && h == 10,
          "single shelf when wide enough");
    check(!sfont_pack(g, 4, 10, 5, ax, ay, &w, &h), "glyph wider than atlas");
    check(!sfont_pack(g, 4, 6, 10, ax, ay, &w, &h), "atlas taller than limit");
}

/* Strip: marker row + 4 glyph rows, glyphs '!'(3) '"'(5) '#'(2) on black. */
static int write_sfont(const char *path)
{
    enum { W = 14, H = 5 };
    unsigned int px[W * H];
    for (int i = 0; i < W * H; i++)
        px[i] = BLK;
    int marks[] = { 0, 4, 10, 13 };
    for (int i = 0; i < 4; i++)
        px[marks[i]] = MAG;
    px[1 * W + 1] = WHT;          /* a pixel inside '!' */
    px[4 * W + 12] = WHT;         /* bottom row of '#' */
    return stbi_write_png(path, W, H, 4, px, W * 4);
}

static void test_sfont(const char *dir)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/sfont_small.png", dir);
    check(write_sfont(path) != 0, "write synthetic strip");

    int h = sfont_alloc(path);
    check(h > 0, "sfont_alloc");
    check(font_nchars(h) == 3, "letters");
    check(font_cellh(h) == 4, "height excludes marker row");
    check(font_cellw(h) == 5, "widest glyph");

    check(font_textwidth("!\"#", h) == 10, "proportional width");
    check(font_textwidth(" ", h) == 3, "space = '!' width");
    check(font_textwidth("A", h) == 3, "uncovered char = space width");
    check(font_textwidth("!\n!", h) == 6, "newline adds no width");
    check(font_textwidth("", h) == 0, "empty string");

    g_nblit = 0;
    int end = font_draw_text(h, 10, 20, "!\" #", 0xFF00FF00u);
    check(end == 10 + 3 + 5 + 3 + 2, "print returns final x");
    check(g_nblit == 3, "space is not drawn");
    check(g_blit[0].x == 10 && g_blit[0].y == 20, "glyph 0 position");
    check(g_blit[0].sx == 0 && g_blit[0].sy == 0 &&
          g_blit[0].sw == 3 && g_blit[0].sh == 4, "glyph 0 atlas rect");
    check(g_blit[1].x == 13 && g_blit[1].sx == 3 && g_blit[1].sw == 5,
          "glyph 1 atlas rect");
    check(g_blit[2].x == 21 && g_blit[2].sx == 8 && g_blit[2].sw == 2,
          "glyph 2 after space");
    check(g_blit[0].tint == 0xFF00FF00u, "tint passed through");

    g_nblit = 0;
    end = font_draw_text(h, 5, 0, "!\n#", WHT);
    check(g_nblit == 2 && g_blit[1].x == 5 && g_blit[1].y == 4,
          "newline returns to x0, y += height");
    check(end == 7, "x after newline");

    OL_Surface *s = g_fonts[h - 1].sheet;
    check(s && s->w == 10 && s->h == 4, "atlas holds only glyph rows");
    check(s && s->pixels[0] == WHT, "glyph pixel kept");
    check(s && s->pixels[1] == 0, "colour key made background clear");
    check(s && s->pixels[3 * s->realW + 9] == WHT, "last row of '#' kept");

    font_free(h);
    check(font_nchars(h) == 0, "freed handle");
    check(sfont_alloc("/nonexistent/sfont.png") == 0, "missing file");
}

static void test_grid(const char *dir)
{
    enum { W = 16, H = 8 };
    unsigned int px[W * H];
    for (int i = 0; i < W * H; i++)
        px[i] = 0;
    px[0] = WHT;
    char path[512];
    snprintf(path, sizeof(path), "%s/grid_small.png", dir);
    stbi_write_png(path, W, H, 4, px, W * 4);

    check(sfont_alloc(path) == 0, "grid sheet without markers is no SFont");

    int h = font_alloc(path, 8, 8);
    check(h > 0 && font_nchars(h) == 2, "grid nchars");
    check(font_cellw(h) == 8 && font_cellh(h) == 8, "grid cell");
    check(font_textwidth(" !\"", h) == 16, "grid width skips missing glyph");

    g_nblit = 0;
    int end = font_draw_text(h, 0, 0, " !\t!\"", WHT);
    check(end == 8 + 8 + 32 + 8, "grid advance, tab = 4 cells");
    check(g_nblit == 3, "grid draws covered glyphs only");
    check(g_blit[1].sx == 8 && g_blit[1].sy == 0 && g_blit[1].sw == 8,
          "grid source cell");
    OL_Surface *s = g_fonts[h - 1].sheet;
    check(s && s->pixels[0] == WHT, "grid sheet copied");
    font_free(h);
}

static void test_wide_asset(const char *assets)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/sfont.png", assets);
    int h = sfont_alloc(path);
    check(h > 0 && font_nchars(h) == 94, "assets/sfont.png has '!'..'~'");
    g_nblit = 0;
    font_draw_text(h, 0, 0, "~", WHT);
    OL_Surface *s = g_fonts[h - 1].sheet;
    check(s && s->realW <= SFONT_ATLAS_MAX && s->realH <= SFONT_ATLAS_MAX,
          "wide strip packed within GE texture limit");
    check(g_nblit == 1 && g_blit[0].sy > 0, "late glyph on a lower shelf");
    font_free(h);
}

/* Grid sheet wider than the GE limit: 96 cells of 8x16 in one 768 px row,
 * glyph g marked by pixel value 0xFF000000 | g at its cell's top-left. */
static void test_wide_grid(const char *dir)
{
    enum { CW = 8, CH = 16, N = 96, W = CW * N, H = CH };
    static unsigned int px[W * H];
    memset(px, 0, sizeof(px));
    for (int g = 0; g < N; g++)
        px[g * CW] = 0xFF000000u | (unsigned)g;
    px[(H - 1) * W + W - 1] = WHT;    /* bottom-right of the last cell */
    char path[512];
    snprintf(path, sizeof(path), "%s/grid_wide.png", dir);
    check(stbi_write_png(path, W, H, 4, px, W * 4) != 0, "write wide grid");

    int h = font_alloc(path, CW, CH);
    check(h > 0 && font_nchars(h) == N, "wide grid nchars");

    g_nblit = 0;
    font_draw_text(h, 0, 0, " ~", WHT);   /* glyphs 0, 94, 95 */
    OL_Surface *s = g_fonts[h - 1].sheet;
    check(s && s->realW <= SFONT_ATLAS_MAX && s->realH <= SFONT_ATLAS_MAX,
          "wide grid packed within GE texture limit");
    check(s && s->w == 512 && s->h == 2 * CH, "wide grid atlas: 64 cols x 2 rows");
    check(g_nblit == 3, "wide grid draws three glyphs");
    check(g_blit[0].sx == 0 && g_blit[0].sy == 0 &&
          g_blit[0].sw == CW && g_blit[0].sh == CH, "wide grid glyph 0 rect");
    check(g_blit[1].sx == 30 * CW && g_blit[1].sy == CH,
          "wide grid glyph 94 on second atlas row");
    check(g_blit[2].sx == 31 * CW && g_blit[2].sy == CH,
          "wide grid glyph 95 rect");
    for (int g = 0; s && g < N; g++) {
        unsigned int p = s->pixels[(g / 64) * CH * s->realW + (g % 64) * CW];
        if (p != (0xFF000000u | (unsigned)g)) {
            check(0, "wide grid cell copied to its atlas slot");
            break;
        }
    }
    check(s && s->pixels[(2 * CH - 1) * s->realW + 32 * CW - 1] == WHT,
          "wide grid last cell's bottom-right pixel copied");
    font_free(h);

    /* 1024 px tall single column of 64 8x16 cells -> one 512 px atlas row. */
    static unsigned int tall[CW * 1024];
    memset(tall, 0, sizeof(tall));
    snprintf(path, sizeof(path), "%s/grid_tall.png", dir);
    stbi_write_png(path, CW, 1024, 4, tall, CW * 4);
    h = font_alloc(path, CW, CH);
    check(h > 0 && font_nchars(h) == 64, "tall grid nchars");
    g_nblit = 0;
    font_draw_glyph(h, 0, 0, (char)(' ' + 63), WHT);
    s = g_fonts[h - 1].sheet;
    check(s && s->w == 512 && s->h == CH, "tall grid atlas is one row");
    check(g_nblit == 1 && g_blit[0].sx == 63 * CW && g_blit[0].sy == 0,
          "tall grid last glyph rect");
    font_free(h);

    /* 64x64 cells in one row: 8 per atlas row, so 64 cells (4096 px) fill
     * the 512x512 atlas exactly and 65 cells (4160 px) need a ninth row. */
    unsigned int *big = (unsigned int *)calloc((size_t)4160 * 64, 4);
    snprintf(path, sizeof(path), "%s/grid_fit.png", dir);
    stbi_write_png(path, 4096, 64, 4, big, 4096 * 4);
    h = font_alloc(path, 64, 64);
    check(h > 0 && font_nchars(h) == 64, "64 cells of 64x64 fill the atlas");
    g_nblit = 0;
    font_draw_glyph(h, 0, 0, (char)(' ' + 63), WHT);
    s = g_fonts[h - 1].sheet;
    check(s && s->realW == 512 && s->realH == 512, "full atlas is 512x512");
    check(g_nblit == 1 && g_blit[0].sx == 7 * 64 && g_blit[0].sy == 7 * 64,
          "last cell in the atlas corner");
    font_free(h);
    snprintf(path, sizeof(path), "%s/grid_over.png", dir);
    stbi_write_png(path, 4160, 64, 4, big, 4160 * 4);
    check(font_alloc(path, 64, 64) == 0, "grid too big for the atlas");
    free(big);
}

/* blits() -> count, last x, last y, last sw */
static int l_blits(lua_State *L)
{
    int last = g_nblit > 0 ? (g_nblit < 64 ? g_nblit : 64) - 1 : 0;
    lua_pushinteger(L, g_nblit);
    lua_pushinteger(L, g_blit[last].x);
    lua_pushinteger(L, g_blit[last].y);
    lua_pushinteger(L, g_blit[last].sw);
    g_nblit = 0;
    return 4;
}

int main(int argc, char **argv)
{
    const char *assets = argc > 2 ? argv[2] : "assets";

    test_parse();
    test_pack();
    test_sfont(".");
    test_grid(".");
    test_wide_grid(".");
    test_wide_asset(assets);

    printf("\nRESULT: %d passed, %d failed (font C tests)\n", checks, fails);
    if (fails || argc < 2)
        return fails ? 1 : 0;

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_newtable(L);
    lua_setglobal(L, "screen");
    font_api_register(L);
    lua_register(L, "blits", l_blits);
    lua_pushstring(L, assets);
    lua_setglobal(L, "ASSETS");
    if (luaL_dofile(L, argv[1]) != 0) {
        fprintf(stderr, "TEST FAILED: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }
    lua_close(L);
    return 0;
}
