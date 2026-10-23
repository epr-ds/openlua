/* Host-side subset of PSPSDK's <pspfont.h>.
 *
 * The real header ships with pspdev. Host tests use this stub together with
 * test/host/pspfont_stub.c so the PGF loader can be exercised without a PSP
 * system font or libpspfont.
 */
#ifndef OL_HOST_PSPFONT_H
#define OL_HOST_PSPFONT_H

#ifdef __cplusplus
extern "C" {
#endif

enum SceFontPixelFormat {
    SCE_FONT_PIXEL_FORMAT_L4 = 0,
    SCE_FONT_PIXEL_FORMAT_R4 = 1,
    SCE_FONT_PIXEL_FORMAT_8  = 2,
    SCE_FONT_PIXEL_FORMAT_24 = 3,
    SCE_FONT_PIXEL_FORMAT_32 = 4
};

typedef struct SceFontCharacterData {
    int width, height, top, bottom;
    int shiftX, shiftY, shiftVertX, shiftVertY;
    int hShift, vShift;
} SceFontCharacterData;

typedef struct SceFontCharacterDataFloat {
    float width, height, top, bottom;
    float shiftX, shiftY, shiftVertX, shiftVertY;
    float hShift, vShift;
} SceFontCharacterDataFloat;

typedef struct SceFontInfo {
    SceFontCharacterData      maxFixed;
    SceFontCharacterDataFloat maxFloat;
    unsigned short            maxWidth;
    unsigned short            maxHeight;
    unsigned int              characters;
    unsigned int              subCharacters;
} SceFontInfo;

typedef struct SceFontRect {
    unsigned short width;
    unsigned short height;
} SceFontRect;

typedef struct SceFontImageBuffer {
    unsigned int    pixelFormat;
    int             x;
    int             y;
    SceFontRect     size;
    unsigned short  bytesPerLine;
    short           unknown;
    unsigned char  *data;
} SceFontImageBuffer;

typedef struct SceFontCharacterInfo {
    unsigned int       width;
    unsigned int       height;
    int                x;
    int                y;
    SceFontCharacterData data;
    char               unknown[4];
} SceFontCharacterInfo;

typedef struct SceFontLibData {
    void        *unknown;
    unsigned int maxFonts;
    void        *cache;
    void *(*alloc)(void *data, unsigned int size);
    void  (*free)(void *data, void *ptr);
    void        *open;
    void        *close;
    void        *read;
    void        *seek;
    void        *onError;
    void        *onReadComplete;
} SceFontLibData;

void *sceFontNewLib(SceFontLibData *libData, int *error);
void *sceFontOpenUserFile(void *lib, char *path, unsigned int mode, int *error);
int   sceFontGetFontInfo(void *font, SceFontInfo *info);
int   sceFontGetCharInfo(void *font, unsigned short charCode,
                         SceFontCharacterInfo *charInfo);
int   sceFontGetCharGlyphImage(void *font, unsigned short charCode,
                               SceFontImageBuffer *glyphImage);
int   sceFontClose(void *font);
int   sceFontDoneLib(void *lib);

#ifdef __cplusplus
}
#endif

#endif /* OL_HOST_PSPFONT_H */
