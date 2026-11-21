/* OpenLua - software PGF font decoder.
 *
 * PGF is the bitmap font container shipped by OneLua games (usually
 * `oneFont.pgf` beside the EBOOT).  OneLua rasterizes it in its own code
 * instead of using Sony's sceLibFont, which on real hardware is a per-game
 * user module (libfont.prx) and cannot be statically imported.  OpenLua
 * follows the same approach: this file decodes the container, expands the
 * printable ASCII glyphs and packs them into one <=512x512 RGBA atlas that
 * the regular gfx_blit path draws.
 *
 * The decoder understands revision 2 PGF files, which is what PSP-era
 * OneLua games ship.  Anything else - revision 3, truncated tables, absurd
 * sizes - makes pgf_load() return NULL so callers can fall back to an
 * SFont/PNG atlas.
 */

#include "engine/font/pgf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PGF_FIRST 32
#define PGF_LAST  126
#define PGF_COUNT (PGF_LAST - PGF_FIRST + 1)

#define PGF_ATLAS_W     512
#define PGF_ATLAS_MAX_H 512

/* Revision 2 header: 392 bytes, little endian. */
#define PGF_HDR_SIZE     392

#define PGF_O_MAGIC      4
#define PGF_O_REVISION   8
#define PGF_O_CHARMAP_LEN  16
#define PGF_O_CHARPTR_LEN  20
#define PGF_O_CHARMAP_BPE  24
#define PGF_O_CHARPTR_BPE  28
#define PGF_O_FIRST_GLYPH  182
#define PGF_O_LAST_GLYPH   184
#define PGF_O_MAX_ASCENDER  212
#define PGF_O_MAX_DESCENDER 216
#define PGF_O_MAX_BASE_Y    224
#define PGF_O_DIM_LEN    258
#define PGF_O_X_LEN      259
#define PGF_O_Y_LEN      260
#define PGF_O_ADV_LEN    261
#define PGF_O_SHADOW_LEN 364
#define PGF_O_SHADOW_BPE 368

/* Glyph bitmap direction flags. */
#define PGF_BMP_H_ROWS 0x01
#define PGF_BMP_V_ROWS 0x02
#define PGF_BMP_MASK   0x03

/* Glyph metric flags. */
#define PGF_METRIC_DIM     0x04
#define PGF_METRIC_BEARING_X 0x08
#define PGF_METRIC_BEARING_Y 0x10
#define PGF_METRIC_ADVANCE 0x20

/* One decoded glyph record.  Metrics are in 26.6 fixed point. */
typedef struct {
    int w, h;                /* bitmap size in pixels                    */
    int xadj;                /* 26.6 bearing from pen to bitmap left     */
    int yadj;                /* 26.6 ascender metric (unused by drawing) */
    int topB;                /* bitmap top bearing above the baseline    */
    int adv;                 /* 26.6 horizontal advance                  */
    int hasBitmap;
    int vertical;
    int atlasX, atlasY;      /* packed into a line's row height          */
    unsigned long long recordOff;
    size_t bitmapByte;       /* bitmap start inside the record           */
} PGFCell;

/* Interleaved [horizontal, vertical] pairs of the PGF metric tables. */
typedef struct {
    int  n;
    int *pair;
} PGFTable;

/* 26.6 table entry used by the horizontal text path. */
struct OL_PgfFont {
    OL_Surface *sheet;

    int atlasX[PGF_COUNT];
    int atlasY[PGF_COUNT];
    int glyphW[PGF_COUNT];
    int glyphH[PGF_COUNT];
    int shiftX[PGF_COUNT];
    int shiftY[PGF_COUNT];
    int advance[PGF_COUNT];

    int height;              /* ink box span, pixels (OneLua text height) */
    int baseline;            /* OneLua text anchor, pixels below line top  */
    int lineH;               /* line height, pixels                       */
    int cellW;               /* widest advance, pixels                    */
    int spaceAdv;            /* 26.6 space/tab advance                    */
};

/* ----------------------------------------------------------------------
 * Little-endian / bit readers
 * ------------------------------------------------------------------- */

static unsigned int pgf_u16(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static int pgf_i32(const unsigned char *p)
{
    unsigned int u = (unsigned int)p[0] |
                     ((unsigned int)p[1] << 8) |
                     ((unsigned int)p[2] << 16) |
                     ((unsigned int)p[3] << 24);
    long long v = (long long)u;
    if (v & 0x80000000LL)
        v -= 0x100000000LL;
    return (int)v;
}

typedef struct {
    const unsigned char *data;
    size_t nbits;
    size_t pos;
    int    ok;
} PGFBits;

static unsigned int pgf_bits(PGFBits *b, int n)
{
    unsigned int v = 0;

    if (!b->ok || n < 0 || n > 32 || b->pos + (size_t)n > b->nbits) {
        b->ok = 0;
        return 0;
    }
    for (int i = 0; i < n; i++) {
        v |= (unsigned int)((b->data[b->pos >> 3] >> (b->pos & 7)) & 1u) << i;
        b->pos++;
    }
    return v;
}

static int pgf_sign7(unsigned int v)
{
    return v >= 64u ? (int)v - 128 : (int)v;
}

static int pgf_sign32(unsigned int v)
{
    long long s = (long long)v;
    if (s & 0x80000000LL)
        s -= 0x100000000LL;
    return (int)s;
}

/* File access helpers.  PSP-era PGFs are a few megabytes at most, so the
 * 32-bit `long` offsets of fseek() are plenty. */
static int pgf_seek(FILE *fp, unsigned long long off)
{
    if (off > 0x7FFFFFFFull)
        return 0;
    return fseek(fp, (long)off, SEEK_SET) == 0;
}

static size_t pgf_read_some_at(FILE *fp, unsigned long long off, void *buf,
                               size_t n)
{
    if (!pgf_seek(fp, off))
        return 0;
    return fread(buf, 1, n, fp);
}

static int pgf_read_bits_at(FILE *fp, unsigned long long base,
                            unsigned long long bitPos, int n,
                            unsigned int *out)
{
    unsigned char buf[8];
    int bitOff = (int)(bitPos & 7);
    int need = (bitOff + n + 7) / 8;
    unsigned int v = 0;

    if (n <= 0 || n > 32 || need > (int)sizeof(buf))
        return 0;
    if (pgf_read_some_at(fp, base + (bitPos >> 3), buf, (size_t)need) !=
        (size_t)need)
        return 0;
    for (int i = 0; i < n; i++) {
        int bit = bitOff + i;
        v |= (unsigned int)((buf[bit >> 3] >> (bit & 7)) & 1u) << i;
    }
    *out = v;
    return 1;
}

/* ----------------------------------------------------------------------
 * Metric tables
 * ------------------------------------------------------------------- */

static int pgf_table_read(FILE *fp, PGFTable *t, int n)
{
    unsigned char buf[255 * 8];

    t->n = 0;
    t->pair = NULL;
    if (n == 0)
        return 1;
    if (n < 0 || n > 255)
        return 0;
    if (fread(buf, 1, (size_t)n * 8, fp) != (size_t)n * 8)
        return 0;

    t->pair = (int *)malloc((size_t)n * 2u * sizeof(int));
    if (!t->pair)
        return 0;
    t->n = n;
    for (int i = 0; i < n; i++) {
        t->pair[i * 2]     = pgf_i32(buf + (size_t)i * 8);
        t->pair[i * 2 + 1] = pgf_i32(buf + (size_t)i * 8 + 4);
    }
    return 1;
}

static void pgf_table_free(PGFTable *t)
{
    free(t->pair);
    t->pair = NULL;
    t->n = 0;
}

static int pgf_metric(PGFBits *b, const PGFTable *t, int indexed)
{
    if (indexed) {
        unsigned int i = pgf_bits(b, 8);
        if (i < (unsigned int)t->n)
            return t->pair[i * 2];
        return 0;
    }

    unsigned int h = pgf_bits(b, 32);
    (void)pgf_bits(b, 32);            /* vertical value, unused */
    return pgf_sign32(h);
}

/* ----------------------------------------------------------------------
 * Glyph records and RLE bitmaps
 * ------------------------------------------------------------------- */

static int pgf_parse_record(const unsigned char *rec, size_t recLen,
                            const PGFTable *dimT, const PGFTable *xT,
                            const PGFTable *yT, const PGFTable *advT,
                            PGFCell *c)
{
    PGFBits b;
    int flags;

    b.data = rec;
    b.nbits = recLen * 8;
    b.pos = 0;
    b.ok = 1;

    (void)pgf_bits(&b, 14);           /* uncompressed bitmap size */
    c->w = (int)pgf_bits(&b, 7);
    c->h = (int)pgf_bits(&b, 7);
    (void)pgf_sign7(pgf_bits(&b, 7)); /* left bearing  */
    c->topB = pgf_sign7(pgf_bits(&b, 7)); /* top bearing */
    flags = (int)pgf_bits(&b, 6);
    (void)pgf_bits(&b, 2);            /* shadow flags  */
    (void)pgf_bits(&b, 2);
    (void)pgf_bits(&b, 3);
    (void)pgf_bits(&b, 9);            /* shadow glyph  */

    (void)pgf_metric(&b, dimT, flags & PGF_METRIC_DIM);
    c->xadj = pgf_metric(&b, xT, flags & PGF_METRIC_BEARING_X);
    c->yadj = pgf_metric(&b, yT, flags & PGF_METRIC_BEARING_Y);
    c->adv  = pgf_metric(&b, advT, flags & PGF_METRIC_ADVANCE);

    if (!b.ok)
        return 0;

    /* Metrics always end on a byte boundary: they are either four 8-bit
     * indexes or four 64-bit raw pairs after the fixed 64-bit prefix. */
    c->bitmapByte = b.pos / 8;
    c->vertical = ((flags & PGF_BMP_MASK) == PGF_BMP_V_ROWS);
    c->hasBitmap = ((flags & PGF_BMP_MASK) == PGF_BMP_H_ROWS ||
                    (flags & PGF_BMP_MASK) == PGF_BMP_V_ROWS) &&
                   c->w > 0 && c->h > 0;
    return 1;
}

static void pgf_store_pixel(unsigned char *px, const PGFCell *c, size_t i,
                            unsigned char v)
{
    if (c->vertical)
        px[(i % (size_t)c->h) * (size_t)c->w + i / (size_t)c->h] = v;
    else
        px[i] = v;
}

/* Expand the nibble RLE stream into c->w * c->h 4-bit samples. */
static int pgf_decode_bitmap(const unsigned char *buf, size_t bufLen,
                             const PGFCell *c, unsigned char *px)
{
    PGFBits b;
    size_t total = (size_t)c->w * (size_t)c->h;
    size_t i = 0;

    b.data = buf;
    b.nbits = bufLen * 8;
    b.pos = 0;
    b.ok = 1;

    while (i < total) {
        unsigned int nib = pgf_bits(&b, 4);

        if (!b.ok)
            return 0;

        if (nib < 8) {
            unsigned int v = pgf_bits(&b, 4);
            unsigned int run = nib + 1;
            if (!b.ok)
                return 0;
            while (run-- && i < total)
                pgf_store_pixel(px, c, i++, (unsigned char)v);
        } else {
            unsigned int run = 16 - nib;
            while (run-- && i < total) {
                unsigned int v = pgf_bits(&b, 4);
                if (!b.ok)
                    return 0;
                pgf_store_pixel(px, c, i++, (unsigned char)v);
            }
        }
    }
    return 1;
}

/* ----------------------------------------------------------------------
 * Loading
 * ------------------------------------------------------------------- */

OL_PgfFont *pgf_load(const char *path)
{
    OL_PgfFont *f = NULL;
    FILE *fp = NULL;
    unsigned char hdr[PGF_HDR_SIZE];
    unsigned char tmp[64];
    PGFTable dimT = { 0, NULL }, xT = { 0, NULL };
    PGFTable yT = { 0, NULL }, advT = { 0, NULL };
    PGFCell *cells = NULL;
    unsigned char *raw = NULL;
    unsigned char *px = NULL;

    unsigned long long fileSize = 0, glyphOff;
    unsigned long long cmOff, cpOff;
    int cmLen, cpLen, cmBpe, cpBpe, shadowLen, shadowBpe;
    int dimLen, xLen, yLen, advLen;
    int firstGlyph, lastGlyph;
    int maxH = 0, maxW = 0, maxAdv = 0;
    int inkTop = 0, inkBottom = 0;
    int atlasH = 0;

    if (!path || !*path)
        return NULL;

    fp = fopen(path, "rb");
    if (!fp)
        return NULL;

    /* Header. */
    if (fread(hdr, 1, sizeof(hdr), fp) != sizeof(hdr))
        goto fail;
    if (memcmp(hdr + PGF_O_MAGIC, "PGF0", 4) != 0)
        goto fail;
    if (pgf_i32(hdr + PGF_O_REVISION) != 2)
        goto fail;

    if (fseek(fp, 0, SEEK_END) != 0)
        goto fail;
    {
        long n = ftell(fp);
        if (n < 0)
            goto fail;
        fileSize = (unsigned long long)n;
    }

    cmLen = pgf_i32(hdr + PGF_O_CHARMAP_LEN);
    cpLen = pgf_i32(hdr + PGF_O_CHARPTR_LEN);
    cmBpe = pgf_i32(hdr + PGF_O_CHARMAP_BPE);
    cpBpe = pgf_i32(hdr + PGF_O_CHARPTR_BPE);
    shadowLen = pgf_i32(hdr + PGF_O_SHADOW_LEN);
    shadowBpe = pgf_i32(hdr + PGF_O_SHADOW_BPE);

    dimLen = hdr[PGF_O_DIM_LEN];
    xLen   = hdr[PGF_O_X_LEN];
    yLen   = hdr[PGF_O_Y_LEN];
    advLen = hdr[PGF_O_ADV_LEN];

    firstGlyph = (int)pgf_u16(hdr + PGF_O_FIRST_GLYPH);
    lastGlyph  = (int)pgf_u16(hdr + PGF_O_LAST_GLYPH);

    if (cmLen <= 0 || cmLen > 65536 || cpLen <= 0 || cpLen > 65536)
        goto fail;
    if (cmBpe <= 0 || cmBpe > 32 || cpBpe <= 0 || cpBpe > 32)
        goto fail;
    if (shadowLen < 0 || shadowLen > 65536 || shadowBpe < 0 || shadowBpe > 32)
        goto fail;

    /* Every table is 32-bit aligned, so the bitmap payload starts right
     * after the char pointer table. */
    {
        unsigned long long tablesBytes =
            ((unsigned long long)dimLen + (unsigned long long)xLen +
             (unsigned long long)yLen + (unsigned long long)advLen) * 8ull;
        unsigned long long shadowBytes =
            (((unsigned long long)shadowLen * (unsigned long long)shadowBpe +
              31ull) / 32ull) * 4ull;
        unsigned long long cmBytes =
            (((unsigned long long)cmLen * (unsigned long long)cmBpe +
              31ull) / 32ull) * 4ull;
        unsigned long long cpBytes =
            (((unsigned long long)cpLen * (unsigned long long)cpBpe +
              31ull) / 32ull) * 4ull;

        cmOff = (unsigned long long)PGF_HDR_SIZE + tablesBytes + shadowBytes;
        cpOff = cmOff + cmBytes;
        glyphOff = cpOff + cpBytes;
        if (glyphOff > fileSize)
            goto fail;
    }

    if (!pgf_seek(fp, PGF_HDR_SIZE))
        goto fail;
    if (!pgf_table_read(fp, &dimT, dimLen) ||
        !pgf_table_read(fp, &xT, xLen) ||
        !pgf_table_read(fp, &yT, yLen) ||
        !pgf_table_read(fp, &advT, advLen))
        goto fail;

    cells = (PGFCell *)calloc(PGF_COUNT, sizeof(*cells));
    if (!cells)
        goto fail;

    /* Pass 1: metrics and atlas layout. */
    for (int i = 0; i < PGF_COUNT; i++) {
        PGFCell *c = &cells[i];
        unsigned int gi = 0, ptr = 0;
        unsigned long long recOff;
        size_t got;
        int code = PGF_FIRST + i;
        long mi;

        if (code < firstGlyph || code > lastGlyph)
            continue;
        mi = (long)code - (long)firstGlyph;
        if (mi >= cmLen)
            continue;

        if (!pgf_read_bits_at(fp, cmOff,
                              (unsigned long long)mi * (unsigned long long)cmBpe,
                              cmBpe, &gi))
            continue;
        if (gi == 0xFFFFu || gi >= (unsigned int)cpLen)
            continue;

        if (!pgf_read_bits_at(fp, cpOff,
                              (unsigned long long)gi * (unsigned long long)cpBpe,
                              cpBpe, &ptr))
            continue;

        recOff = glyphOff + (unsigned long long)ptr * 4ull;
        if (recOff >= fileSize)
            continue;

        got = pgf_read_some_at(fp, recOff, tmp, sizeof(tmp));
        if (got == 0)
            continue;
        if (!pgf_parse_record(tmp, got, &dimT, &xT, &yT, &advT, c))
            continue;

        c->recordOff = recOff;

        if (c->hasBitmap) {
            if (c->h > maxH) maxH = c->h;
            if (c->w > maxW) maxW = c->w;
            if (c->topB > inkTop) inkTop = c->topB;
            if (c->topB - c->h < inkBottom) inkBottom = c->topB - c->h;
        }
        if (c->adv > maxAdv)
            maxAdv = c->adv;
    }

    if (maxH < 1 || maxW < 1 || maxW > PGF_ATLAS_W)
        goto fail;

    /*
     * Pack cells into rows of maxH.  Packing against the tallest glyph
     * keeps the atlas small without breaking the per-glyph cell size.
     */
    {
        int ax = 0, ay = 0;

        for (int i = 0; i < PGF_COUNT; i++) {
            PGFCell *c = &cells[i];

            c->atlasX = -1;
            c->atlasY = -1;
            if (!c->hasBitmap)
                continue;
            if (ax + c->w > PGF_ATLAS_W) {
                ax = 0;
                ay += maxH;
            }
            if (ay + c->h > PGF_ATLAS_MAX_H)
                goto fail;
            c->atlasX = ax;
            c->atlasY = ay;
            ax += c->w + 1;
            if (ay + c->h > atlasH)
                atlasH = ay + c->h;
        }
    }
    if (atlasH < 1)
        goto fail;

    f = (OL_PgfFont *)calloc(1, sizeof(*f));
    if (!f)
        goto fail;

    f->sheet = gfx_surface_new(PGF_ATLAS_W, atlasH);
    if (!f->sheet)
        goto fail;

    raw = (unsigned char *)malloc((size_t)maxW * (size_t)maxH + 2u);
    px  = (unsigned char *)malloc((size_t)maxW * (size_t)maxH);
    if (!raw || !px)
        goto fail;

    /* Pass 2: decode and expand the bitmaps into the atlas. */
    for (int i = 0; i < PGF_COUNT; i++) {
        PGFCell *c = &cells[i];
        size_t bytes, got;

        if (!c->hasBitmap)
            continue;
        bytes = (size_t)c->w * (size_t)c->h;

        got = pgf_read_some_at(fp, c->recordOff + c->bitmapByte, raw,
                               bytes + 1u);
        if (got == 0)
            continue;
        if (got < bytes + 1u)
            memset(raw + got, 0, bytes + 1u - got);
        memset(px, 0, bytes);

        if (!pgf_decode_bitmap(raw, bytes + 1u, c, px))
            continue;

        for (int y = 0; y < c->h; y++) {
            unsigned int *dst = f->sheet->pixels +
                (size_t)(c->atlasY + y) * (size_t)f->sheet->realW +
                (size_t)c->atlasX;
            const unsigned char *src = px + (size_t)y * (size_t)c->w;
            for (int x = 0; x < c->w; x++)
                dst[x] = ((unsigned int)src[x] * 17u) << 24 | 0x00FFFFFFu;
        }
    }

    /* Vertical metrics.
     *
     * OneLua's `screen.textheight(font)` reports the font's ink span and
     * ignores the size argument.  The PGF header's vertical metrics do not
     * reproduce that value (notably for fonts with degenerate adjust
     * tables), so derive it from the glyph data: the tallest bitmap top
     * above the baseline minus the lowest bitmap bottom below it. */
    {
        int baseY = pgf_i32(hdr + PGF_O_MAX_BASE_Y);
        int desc  = pgf_i32(hdr + PGF_O_MAX_DESCENDER);
        int lineH  = (baseY - desc + 63) >> 6;
        int cellW  = (maxAdv + 63) >> 6;
        int height = inkTop - inkBottom;

        if (height < 1)
            height = maxH;
        if (lineH < maxH)
            lineH = maxH;
        if (lineH < 1)
            lineH = 1;
        if (cellW < maxW)
            cellW = maxW;
        if (cellW < 1)
            cellW = 1;

        f->height = height;
        /* OneLua anchors glyphs at 0.7 * maxAscender below the requested y
         * (0.7 is its default text size); the size argument scales the
         * glyphs but not the anchor. */
        int baseline = (int)((long)pgf_i32(hdr + PGF_O_MAX_ASCENDER) * 7 / 10);
        f->baseline = (baseline + 32) >> 6;
        if (f->baseline < 1)
            f->baseline = maxH;
        f->lineH = lineH;
        f->cellW = cellW;
    }

    for (int i = 0; i < PGF_COUNT; i++) {
        const PGFCell *c = &cells[i];

        f->atlasX[i] = c->atlasX;
        f->atlasY[i] = c->atlasY;
        f->glyphW[i] = c->hasBitmap ? c->w : 0;
        f->glyphH[i] = c->hasBitmap ? c->h : 0;
        f->shiftX[i] = c->xadj;
        f->shiftY[i] = c->topB;
        f->advance[i] = c->adv;
    }

    f->spaceAdv = cells[0].adv;
    if (f->spaceAdv <= 0)
        f->spaceAdv = maxAdv > 0 ? maxAdv / 2 : (4 << 6);

    free(raw);
    free(px);
    free(cells);
    pgf_table_free(&dimT);
    pgf_table_free(&xT);
    pgf_table_free(&yT);
    pgf_table_free(&advT);
    fclose(fp);
    return f;

fail:
    free(raw);
    free(px);
    free(cells);
    pgf_table_free(&dimT);
    pgf_table_free(&xT);
    pgf_table_free(&yT);
    pgf_table_free(&advT);
    if (f)
        pgf_free(f);
    if (fp)
        fclose(fp);
    return NULL;
}

void pgf_free(OL_PgfFont *font)
{
    if (!font)
        return;
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

int pgf_textheight(const OL_PgfFont *font)
{
    return font ? font->height : 0;
}

/* ----------------------------------------------------------------------
 * Drawing
 * ------------------------------------------------------------------- */

static int pgf_pen_px(int pen)
{
    return pen >> 6;
}

static void pgf_blit_glyph(OL_PgfFont *f, int pen, int yTop, int idx,
                           unsigned int tint)
{
    if (f->glyphW[idx] <= 0 || f->glyphH[idx] <= 0)
        return;
    gfx_blit(f->sheet,
             pgf_pen_px(pen) + (f->shiftX[idx] >> 6),
             yTop + f->baseline - f->shiftY[idx],
             f->atlasX[idx], f->atlasY[idx],
             f->glyphW[idx], f->glyphH[idx],
             255, 0, 0, 0.0f, 1.0f, 1.0f, 0, tint);
}

/* Scaled blit used by the OneLua `size` argument. (x,y) is already in
 * pixels and includes the glyph bearing. */
static void pgf_blit_glyph_scaled(OL_PgfFont *f, float x, float y, int idx,
                                  unsigned int tint, float scale)
{
    if (f->glyphW[idx] <= 0 || f->glyphH[idx] <= 0)
        return;
    gfx_blit(f->sheet,
             (int)(x + 0.5f), (int)(y + 0.5f),
             f->atlasX[idx], f->atlasY[idx],
             f->glyphW[idx], f->glyphH[idx],
             255, 0, 0, 0.0f, scale, scale, 0, tint);
}

void pgf_draw_glyph(OL_PgfFont *font, int x, int y, unsigned char c,
                    unsigned int tint)
{
    if (!font || !font->sheet || c < PGF_FIRST || c > PGF_LAST)
        return;
    pgf_blit_glyph(font, x << 6, y, (int)c - PGF_FIRST, tint);
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
            int idx = (int)ch - PGF_FIRST;
            pgf_blit_glyph(font, pen, lineY, idx, tint);
            pen += font->advance[idx];
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

void pgf_draw_glyph_scaled(OL_PgfFont *font, int x, int y, unsigned char c,
                           unsigned int tint, float scale)
{
    if (!font || !font->sheet || c < PGF_FIRST || c > PGF_LAST)
        return;
    if (!(scale > 0.0f))
        scale = 1.0f;
    int idx = (int)c - PGF_FIRST;
    float gx = (float)x + (float)(font->shiftX[idx] >> 6) * scale;
    float gy = (float)y + (float)font->baseline -
               (float)font->shiftY[idx] * scale;
    pgf_blit_glyph_scaled(font, gx, gy, idx, tint, scale);
}

/* Keep the pen in 26.6 units so advances stay exact, but scale the pixels. */
int pgf_draw_text_scaled(OL_PgfFont *font, int x, int y, const char *text,
                         unsigned int tint, float scale)
{
    if (!font || !font->sheet || !text)
        return x;
    if (!(scale > 0.0f))
        scale = 1.0f;

    int pen = x << 6;
    int lineY = y;

    for (size_t i = 0; text[i]; i++) {
        unsigned char ch = (unsigned char)text[i];
        if (ch == '\n') {
            lineY += (int)((float)font->lineH * scale + 0.5f);
            pen = x << 6;
        } else if (ch == '\t') {
            pen += (int)((float)font->spaceAdv * 4.0f * scale + 0.5f);
        } else if (ch >= PGF_FIRST && ch <= PGF_LAST) {
            int idx = (int)ch - PGF_FIRST;
            float gx = (float)(pen >> 6) +
                       (float)(font->shiftX[idx] >> 6) * scale;
            float gy = (float)lineY + (float)font->baseline -
                       (float)font->shiftY[idx] * scale;
            pgf_blit_glyph_scaled(font, gx, gy, idx, tint, scale);
            pen += (int)((float)font->advance[idx] * scale + 0.5f);
        }
    }
    return pgf_pen_px(pen);
}

int pgf_textwidth_scaled(OL_PgfFont *font, const char *text, float scale)
{
    if (!(scale > 0.0f))
        scale = 1.0f;
    return (int)((float)pgf_textwidth(font, text) * scale + 0.5f);
}
