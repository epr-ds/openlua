/* OpenLua - PGF font loader and renderer.
 *
 * The PSP system font library rasterizes PGF glyphs into 4-bit alpha
 * bitmaps. We render the printable ASCII range once into a <=512px RGBA
 * atlas, then draw it with the regular gfx_blit path. This keeps text
 * drawing independent of the GE texture-size and sub-rect quirks.
 */

#include "engine/font/pgf.h"

#include <pspfont.h>

#include <malloc.h>
#include <stdlib.h>
#include <string.h>

#define PGF_FIRST 32
#define PGF_LAST  126
#define PGF_COUNT (PGF_LAST - PGF_FIRST + 1)
#define PGF_ATLAS_W 512

struct OL_PgfFont {
    void       *lib;
    void       *font;
    OL_Surface *sheet;

    int atlasX[PGF_COUNT];
    int atlasY[PGF_COUNT];
    int glyphW[PGF_COUNT];
    int glyphH[PGF_COUNT];
    int shiftX[PGF_COUNT];       /* 26.6 bearing from pen to bitmap left */
    int shiftY[PGF_COUNT];       /* 26.6 bearing from baseline to bitmap top */
    int advance[PGF_COUNT];      /* 26.6 horizontal advance */

    int ascent;                  /* baseline offset from line top, pixels */
    int lineH;                   /* line height, pixels                  */
    int cellW;                   /* widest advance, pixels               */
    int spaceAdv;                /* 26.6 space/tab advance               */
};

static void *pgf_alloc(void *data, unsigned int size)
{
    (void)data;
    return memalign(16, size);
}

static void pgf_free_alloc(void *data, void *ptr)
{
    (void)data;
    free(ptr);
}

static int pgf_pen_px(int pen)
{
    return pen >> 6;
}

static void pgf_blit_glyph(OL_PgfFont *f, int pen, int yTop, int idx,
                           unsigned int tint)
{
    if (f->glyphW[idx] <= 0 || f->glyphH[idx] <= 0)
        return;
    int gx = pgf_pen_px(pen) + (f->shiftX[idx] >> 6);
    int gy = yTop + f->ascent - (f->shiftY[idx] >> 6);
    gfx_blit(f->sheet,
             gx, gy,
             f->atlasX[idx], f->atlasY[idx],
             f->glyphW[idx], f->glyphH[idx],
             255, 0, 0, 0.0f, 1.0f, 1.0f, 0, tint);
}

OL_PgfFont *pgf_load(const char *path)
{
    if (!path || !*path)
        return NULL;

    OL_PgfFont *f = (OL_PgfFont *)calloc(1, sizeof(*f));
    if (!f)
        return NULL;

    SceFontLibData libData;
    memset(&libData, 0, sizeof(libData));
    libData.maxFonts = 1;
    libData.alloc = pgf_alloc;
    libData.free = pgf_free_alloc;

    int error = 0;
    f->lib = sceFontNewLib(&libData, &error);
    if (!f->lib)
        goto fail;

    f->font = sceFontOpenUserFile(f->lib, (char *)path, 1, &error);
    if (!f->font)
        goto fail;

    SceFontInfo info;
    memset(&info, 0, sizeof(info));
    if (sceFontGetFontInfo(f->font, &info) != 0)
        goto fail;

    SceFontCharacterInfo ci[PGF_COUNT];
    memset(ci, 0, sizeof(ci));
    for (int i = 0; i < PGF_COUNT; i++) {
        if (sceFontGetCharInfo(f->font, (unsigned short)(PGF_FIRST + i), &ci[i]) != 0)
            memset(&ci[i], 0, sizeof(ci[i]));
    }

    int maxH = 0;
    int maxW = 0;
    int maxAdv = 0;
    for (int i = 0; i < PGF_COUNT; i++) {
        if (ci[i].height > (unsigned int)maxH)
            maxH = (int)ci[i].height;
        if (ci[i].width > (unsigned int)maxW)
            maxW = (int)ci[i].width;
        int adv = ci[i].data.hShift;
        if (adv > 0)
            maxAdv = (adv > maxAdv) ? adv : maxAdv;
    }
    if (maxH < 1)
        maxH = 1;

    int ax = 0, ay = 0, atlasH = 0;
    for (int i = 0; i < PGF_COUNT; i++) {
        int w = (int)ci[i].width;
        int h = (int)ci[i].height;
        if (w < 0) w = 0;
        if (h < 0) h = 0;
        if (w > PGF_ATLAS_W)
            goto fail;

        f->shiftX[i] = ci[i].data.shiftX;
        f->shiftY[i] = ci[i].data.shiftY;
        f->advance[i] = ci[i].data.hShift;
        if (f->advance[i] <= 0 && w > 0)
            f->advance[i] = w << 6;

        if (w > 0 && h > 0) {
            if (ax + w > PGF_ATLAS_W) {
                ax = 0;
                ay += maxH;
            }
            f->atlasX[i] = ax;
            f->atlasY[i] = ay;
            f->glyphW[i] = w;
            f->glyphH[i] = h;
            ax += w + 1;
            if (ay + h > atlasH)
                atlasH = ay + h;
        } else {
            f->atlasX[i] = -1;
            f->atlasY[i] = -1;
        }
    }
    if (atlasH < 1 || atlasH > 512)
        goto fail;

    int stride = PGF_ATLAS_W / 2;
    unsigned char *bits = (unsigned char *)calloc(1, (size_t)stride * (size_t)atlasH);
    if (!bits)
        goto fail;

    SceFontImageBuffer image;
    memset(&image, 0, sizeof(image));
    image.pixelFormat = SCE_FONT_PIXEL_FORMAT_L4;
    image.size.width = PGF_ATLAS_W;
    image.size.height = (unsigned short)atlasH;
    image.bytesPerLine = (unsigned short)stride;
    image.data = bits;

    for (int i = 0; i < PGF_COUNT; i++) {
        if (f->glyphW[i] <= 0 || f->glyphH[i] <= 0)
            continue;
        image.x = f->atlasX[i] << 6;
        image.y = f->atlasY[i] << 6;
        sceFontGetCharGlyphImage(f->font,
                                 (unsigned short)(PGF_FIRST + i),
                                 &image);
    }

    OL_Surface *sheet = gfx_surface_new(PGF_ATLAS_W, atlasH);
    if (!sheet) {
        free(bits);
        goto fail;
    }

    /* Expand the 4-bit coverage mask into white ABGR pixels with alpha. */
    for (int y = 0; y < atlasH; y++) {
        const unsigned char *row = bits + (size_t)y * (size_t)stride;
        unsigned int *dst = sheet->pixels + (size_t)y * (size_t)sheet->realW;
        for (int x = 0; x < PGF_ATLAS_W; x++) {
            unsigned int v = (row[x >> 1] >> ((x & 1) ? 4 : 0)) & 0xFu;
            dst[x] = ((v * 17u) << 24) | 0x00FFFFFFu;
        }
    }
    free(bits);

    int top = info.maxFixed.top;
    int bottom = info.maxFixed.bottom;
    int ascent = (top + 63) >> 6;
    if (ascent < 0)
        ascent = 0;
    int lineH = ((top - bottom + 63) >> 6);
    if (lineH < maxH)
        lineH = maxH;
    if (lineH < 1)
        lineH = 1;

    int cellW = (maxAdv + 63) >> 6;
    if (cellW < maxW)
        cellW = maxW;
    if (cellW < 1)
        cellW = 1;

    f->ascent = ascent;
    f->lineH = lineH;
    f->cellW = cellW;
    f->spaceAdv = (ci[0].data.hShift > 0) ? ci[0].data.hShift : (4 << 6);
    f->sheet = sheet;

    return f;

fail:
    pgf_free(f);
    return NULL;
}

void pgf_free(OL_PgfFont *font)
{
    if (!font)
        return;
    if (font->font)
        sceFontClose(font->font);
    if (font->lib)
        sceFontDoneLib(font->lib);
    if (font->sheet)
        gfx_surface_free(font->sheet);
    free(font);
}

int pgf_nchars(const OL_PgfFont *font)
{
    return font ? PGF_COUNT : 0;
}

int pgf_cellw(const OL_PgfFont *font)
{
    return font ? font->cellW : 0;
}

int pgf_cellh(const OL_PgfFont *font)
{
    return font ? font->lineH : 0;
}

void pgf_draw_glyph(OL_PgfFont *font, int x, int y, unsigned char c,
                    unsigned int tint)
{
    if (!font || !font->sheet || c < PGF_FIRST || c > PGF_LAST)
        return;
    int idx = (int)c - PGF_FIRST;
    pgf_blit_glyph(font, x << 6, y, idx, tint);
}

int pgf_draw_text(OL_PgfFont *font, int x, int y, const char *text,
                  unsigned int tint)
{
    if (!font || !font->sheet || !text)
        return x;

    int pen = x << 6;
    int lineY = y;

    for (size_t i = 0; text[i]; i++) {
        unsigned char ch = (unsigned char)text[i];
        if (ch == '\n') {
            lineY += font->lineH;
            pen = x << 6;
        } else if (ch == '\t') {
            pen += font->spaceAdv * 4;
        } else if (ch >= PGF_FIRST && ch <= PGF_LAST) {
            pgf_blit_glyph(font, pen, lineY, (int)ch - PGF_FIRST, tint);
            pen += font->advance[(int)ch - PGF_FIRST];
        }
    }
    return pgf_pen_px(pen);
}

int pgf_textwidth(OL_PgfFont *font, const char *text)
{
    if (!font || !text)
        return 0;

    int pen = 0;
    for (size_t i = 0; text[i]; i++) {
        unsigned char ch = (unsigned char)text[i];
        if (ch == '\n')
            continue;
        if (ch == '\t')
            pen += font->spaceAdv * 4;
        else if (ch >= PGF_FIRST && ch <= PGF_LAST)
            pen += font->advance[(int)ch - PGF_FIRST];
    }
    return pgf_pen_px(pen);
}
