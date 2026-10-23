/* Host tests for the PGF loader and drawing path.
 *
 * The PGF itself is synthetic: test/host/pspfont_stub.c stands in for
 * sceLibFont. PPSSPP is used to verify a real system PGF font.
 */
#include "engine/font/font.h"

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

extern int font_stub_blits;
extern OL_Surface *font_stub_last_surface;
extern int font_stub_last_x, font_stub_last_y;
extern int font_stub_last_sx, font_stub_last_sy;
extern int font_stub_last_sw, font_stub_last_sh;
extern unsigned int font_stub_last_tint;

int main(int argc, char **argv)
{
    const char *assets = argc > 1 ? argv[1] : "../assets";
    char path[512], missing[512];
    snprintf(path, sizeof(path), "%s/test.pgf", assets);
    snprintf(missing, sizeof(missing), "%s/missing.pgf", assets);

    check(font_alloc_pgf(NULL) == 0, "NULL PGF path returns 0");
    check(font_alloc_pgf(missing) == 0, "missing PGF returns 0");

    int f = font_alloc_pgf(path);
    check(f > 0, "PGF loads through sceLibFont");
    check(font_nchars(f) == 95, "PGF exposes printable ASCII");
    check(font_cellh(f) == 20, "PGF line height comes from maxFixed");
    check(font_cellw(f) == 7, "PGF cell width comes from the widest advance");
    check(font_textwidth("AB", f) == 14, "PGF width sums advances");
    check(font_textwidth(" ", f) == 5, "PGF space has an advance");
    check(font_textwidth("\t", f) == 20, "PGF tab advances four spaces");

    int before = font_stub_blits;
    int endx = font_draw_text(f, 10, 20, "AB", 0xFFFFFFFFu);
    check(endx == 24, "PGF print returns the final X cursor");
    check(font_stub_blits == before + 2, "PGF print draws one blit per glyph");
    check(font_stub_last_surface && font_stub_last_surface->w <= 512 &&
          font_stub_last_surface->h <= 512,
          "PGF atlas stays within the 512px GE texture limit");
    check(font_stub_last_sw == 6 && font_stub_last_sh == 8,
          "PGF glyph source rectangle matches the bitmap");
    check(font_stub_last_tint == 0xFFFFFFFFu, "PGF print uses white tint");

    before = font_stub_blits;
    font_draw_glyph(f, 10, 20, 'A', 0xFFFFFFFFu);
    check(font_stub_blits == before + 1, "PGF single glyph draws once");
    check(font_stub_last_x == 11 && font_stub_last_y == 24,
          "PGF glyph applies bearing and baseline");

    font_free(f);
    check(font_nchars(f) == 0, "PGF handle is released");

    printf("pgf C: %d checks, %d failure(s)\n", checks, fails);
    return fails ? 1 : 0;
}
