/* Host tests for the font/SFONT loading and measurement C API. */
#include "engine/font/font.h"

#include <stdio.h>
#include <string.h>

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

extern int font_stub_blits;
extern OL_Surface *font_stub_last_surface;
extern int font_stub_last_sx, font_stub_last_sy;
extern int font_stub_last_sw, font_stub_last_sh;
extern unsigned int font_stub_last_tint;

int main(int argc, char **argv)
{
    const char *assets = argc > 1 ? argv[1] : "../assets";
    char fixed_path[512], classic_path[512], missing_path[512];
    snprintf(fixed_path, sizeof(fixed_path), "%s/font.png", assets);
    snprintf(classic_path, sizeof(classic_path), "%s/sfont.png", assets);
    snprintf(missing_path, sizeof(missing_path), "%s/not-a-font.png", assets);

    /* Fixed-grid 95-cell sheet: sfont.load can derive the cell size. */
    int fixed = font_alloc_sfont(fixed_path);
    check(fixed > 0, "fixed-grid SFONT loads");
    check(font_nchars(fixed) == 95, "fixed-grid has 95 glyphs");
    check(font_cellw(fixed) == 8, "fixed-grid cell width is 8");
    check(font_cellh(fixed) == 16, "fixed-grid cell height is 16");
    check(font_textwidth("AB", fixed) == 16, "fixed-grid width is 2 cells");
    check(font_draw_text(fixed, 10, 20, "AB", 0xFFFFFFFFu) == 26,
          "fixed-grid print returns final X");

    /* Classic SFont: magenta separators, variable advances, glyphs from '!'. */
    int classic = font_alloc_sfont(classic_path);
    check(classic > 0, "classic SFONT loads");
    check(font_nchars(classic) == 94, "classic font has 94 glyphs");
    check(font_cellh(classic) == 7, "classic glyph height excludes separator");
    check(font_cellw(classic) > 0, "classic max advance is positive");

    int wi = font_textwidth("I", classic);
    int ww = font_textwidth("W", classic);
    check(wi > 0 && ww > wi, "classic SFont is variable width");
    check(font_textwidth(" ", classic) > 0, "classic space has an advance");
    check(font_textwidth("II", classic) == wi * 2, "classic width sums advances");

    int before = font_stub_blits;
    int endx = font_draw_text(classic, 10, 20, "AB", 0xFFFFFFFFu);
    check(endx == 10 + font_textwidth("AB", classic),
          "classic print returns final X");
    check(font_stub_blits == before + 2, "classic print draws each glyph");
    check(font_stub_last_sy == 0, "classic glyph is packed below the separator");
    check(font_stub_last_sh == 7, "classic source height excludes separator");
    check(font_stub_last_surface && font_stub_last_surface->w <= 512 &&
          font_stub_last_surface->h <= 512,
          "font atlas stays within the 512px GE texture limit");
    check(font_stub_last_tint == 0xFFFFFFFFu, "classic print uses white tint");

    check(font_alloc_sfont(missing_path) == 0,
          "missing SFONT returns 0");

    font_free(classic);
    font_free(fixed);

    printf("sfont C: %d checks, %d failure(s)\n", checks, fails);
    return fails ? 1 : 0;
}
