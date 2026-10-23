/* Host `sceLibFont` stub used by the PGF loader tests.
 *
 * It does not parse PGF. It returns a synthetic 7x8 ASCII font so the loader,
 * atlas packing, metric code and drawing path can be tested on the host.
 */
#include <pspfont.h>

#include <string.h>

static int fake_lib;
static int fake_font;

void *sceFontNewLib(SceFontLibData *libData, int *error)
{
    (void)libData;
    if (error)
        *error = 0;
    return &fake_lib;
}

void *sceFontOpenUserFile(void *lib, char *path, unsigned int mode, int *error)
{
    (void)lib;
    (void)mode;
    if (!path || !*path || strstr(path, "missing") || strstr(path, "not-a-font")) {
        if (error)
            *error = -1;
        return NULL;
    }
    if (error)
        *error = 0;
    return &fake_font;
}

int sceFontGetFontInfo(void *font, SceFontInfo *info)
{
    if (!font || !info)
        return -1;
    memset(info, 0, sizeof(*info));
    info->maxFixed.width = 8 << 6;
    info->maxFixed.height = 16 << 6;
    info->maxFixed.top = 16 << 6;
    info->maxFixed.bottom = -(4 << 6);
    info->maxWidth = 8;
    info->maxHeight = 16;
    info->characters = 95;
    return 0;
}

int sceFontGetCharInfo(void *font, unsigned short charCode,
                       SceFontCharacterInfo *charInfo)
{
    if (!font || !charInfo)
        return -1;
    memset(charInfo, 0, sizeof(*charInfo));
    if (charCode < 32 || charCode > 126)
        return -1;

    if (charCode == 32) {
        charInfo->data.hShift = 5 << 6;
        return 0;
    }

    charInfo->width = 6;
    charInfo->height = 8;
    charInfo->x = 0;
    charInfo->y = 0;
    charInfo->data.width = 6 << 6;
    charInfo->data.height = 8 << 6;
    charInfo->data.top = 12 << 6;
    charInfo->data.bottom = -(4 << 6);
    charInfo->data.shiftX = 1 << 6;
    charInfo->data.shiftY = 12 << 6;
    charInfo->data.hShift = 7 << 6;
    return 0;
}

static void put_pixel(unsigned char *data, int bytesPerLine,
                      unsigned short width, unsigned short height,
                      int x, int y, unsigned int value)
{
    if (!data || x < 0 || y < 0 || x >= (int)width || y >= (int)height)
        return;
    unsigned char *p = data + (size_t)y * (size_t)bytesPerLine + (size_t)(x >> 1);
    if (x & 1)
        *p = (unsigned char)((*p & 0x0F) | ((value & 0x0F) << 4));
    else
        *p = (unsigned char)((*p & 0xF0) | (value & 0x0F));
}

int sceFontGetCharGlyphImage(void *font, unsigned short charCode,
                             SceFontImageBuffer *glyphImage)
{
    if (!font || !glyphImage)
        return -1;
    if (charCode < 33 || charCode > 126)
        return 0;

    int x0 = glyphImage->x >> 6;
    int y0 = glyphImage->y >> 6;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 6; x++) {
            put_pixel(glyphImage->data, glyphImage->bytesPerLine,
                      glyphImage->size.width, glyphImage->size.height,
                      x0 + x, y0 + y, 15);
        }
    }
    return 0;
}

int sceFontClose(void *font)
{
    (void)font;
    return 0;
}

int sceFontDoneLib(void *lib)
{
    (void)lib;
    return 0;
}
