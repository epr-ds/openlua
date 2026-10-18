/* OpenLua - bitmap font system implementation
 *
 * Glyph layout: each character = one cell (width x height) pixels in a
 * spritesheet image. Layout order = sequential from char 0x20 (space).
 * The grid is parsed at load time; individual glyphs are extracted at
 * draw time via GPU UV coordinate math. */

#include "engine/font/font.h"
#include <string.h>

/* stb_image implementation (from engine/core/stb_impl.o) */
#include "third_party/stb/stb_image.h"

/* Forward declarations */
static int  l_font_new(lua_State *L);
static int  l_font_blt_obj(lua_State *L);

/* ================================================================
 * font_load — parse PNG spritesheet into grid metadata.
 * ---------------------------------------------------------------
 * Input: a PNG file with glyphs laid out in a grid of cellW × cellH.
 * Returns: OLF_Font struct ready for use (sheet uploaded lazily).
 * The full sheet becomes one GPU texture, uploaded on first glyph. */

OLF_Font font_load(const char *path, int cellW, int cellH)
{
    OLF_Font f;
    memset(&f, 0, sizeof(f));

    /* Load PNG (RGBA pixels from stb_image). */
    int w = 0, h = 0, comp = 0;
    unsigned char *img = stbi_load(path, &w, &h, &comp, 4);
    if (!img) return f;   /* fail: can't load font sheet */

    f.cellW     = cellW;
    f.cellH     = cellH;
    f.sheetW    = w;          /* raw pixel width from PNG    */
    f.sheetH    = h;          /* raw pixel height from PNG  */
    f.texReady  = 0;         /* not yet uploaded to GPU     */

    /* Grid math: how many cells fit across and down? */
    int cols = (w + cellW - 1) / cellW;       /* ceil-div for width  */
    int rows = h / cellH;                       /* must divide evenly   */
    if (rows <= 0 || cols <= 0) {
        stbi_image_free(img);
        return f;
    }

    f.nCols     = cols;
    f.nRows      = rows;
    f.nChars    = cols * rows;                /* total glyph count   */

    strncpy(f.texPath, path, sizeof(f.texPath) - 1);
    f.texPath[sizeof(f.texPath) - 1] = '\0';

    stbi_image_free(img);       /* we only need grid metadata now */
    return f;
}

/* ================================================================
 * ensure_texture — lazy GPU texture upload.
 * ---------------------------------------------------------------
 * Called on first font_blt() call. Loads the PNG, converts RGBA →
 * ABGR (the PSP GE pixel format), and allocates a power-of-2 GPU
 * surface to hold the sheet. */

static int ensure_texture(OLF_Font *f)
{
    if (!f || f->texReady || !f->sheetW || f->texReady) return 1;

    /* Compute GPU-ready dimensions (power-of-two for GE). */
    /* Pass raw dimensions; gfx_surface_new computes realW/realH internally. */
    int realW = f->sheetW > 0 ? f->sheetW : 8;
    int realH = f->sheetH > 0 ? f->sheetH : 8;

    /* Create surface to hold the full font sheet. */
    OL_Surface *sheet = gfx_surface_new(realW, realH);
    if (!sheet)
        return 0;   /* heap allocation failed */

    /* Reload PNG (stb_image supports multiple opens — safe). */
    int w = 0, h = 0, comp = 0;
    unsigned char *rgba = stbi_load(f->texPath, &w, &h, &comp, 4);
    if (!rgba) {
        gfx_surface_free(sheet);
        return 0;
    }

    /* Convert RGBA → ABGR and place in the surface.
     * Each pixel is packed: (A << 24) | (B << 16) | (G << 8) | R */
    for (int y = 0; y < h; y++) {
        const unsigned char *row_src = rgba + y * w * 4;  /* RGBA row */
        for (int x = 0; x < w; x++) {
            int dx = x;                           /* dest column   */
            unsigned char r = row_src[x * 4 + 0];
            unsigned char g = row_src[x * 4 + 1];
            unsigned char b = row_src[x * 4 + 2];
            unsigned char a = row_src[x * 4 + 3];
            sheet->pixels[y * realW + dx] =
                ((unsigned int)a << 24) |     /* Alpha   */
                ((unsigned int)b << 16) |     /* Blue    */
                ((unsigned int)g << 8)  |     /* Green   */
                (unsigned int)r;              /* Red     */
        }
    }

    /* Clear padding rows/cols to black. */
    for (int y = h; y < realH; y++) {
        memset(&sheet->pixels[y * realW], 0, (size_t)realW * sizeof(unsigned int));
    }

    f->sheet      = sheet;
    f->texReady   = 1;
    f->sheetW     = w;      /* keep actual dimensions */
    f->sheetH     = h;
    f->flipX      = 0;
    f->flipY      = 0;
    f->angle      = 0.0f;
    f->scaleX     = 1.0f;
    f->scaleY     = 1.0f;
    f->pivotX     = f->cellW / 2;
    f->pivotY     = f->cellH / 2;
    f->z          = 0;
    f->tint       = 0xFFFFFFFFu;

    stbi_image_free(rgba);
    return 1;
}

/* ================================================================
 * font_blt — GPU blit one glyph from the sheet at (dx, dy).
 * ---------------------------------------------------------------
 * Character code maps to grid cell: col = (ch - 32) % nCols,
 * row = (ch - 32) / nCols. Returns >0 on success, ==0 on error. */

int font_blt(OL_Surface *dst, int dx, int dy, char c, OLF_Font f)
{
    if (!dst || !f.nChars) return 0;
    unsigned int ch = (unsigned int)(c - 32);   /* 0-index from space  */
    if (ch >= (unsigned int)f.nChars) return 0;

    /* Lazy-init GPU texture on first use. */
    if (!ensure_texture(&f)) return 0;

    /* Grid coordinates → pixel offset in sheet. */
    int col = (int)(ch % f.nCols);
    int row = (int)(ch / f.nCols);
    int sx = col * f.cellW;   /* source X (within GPU texture) */
    int sy = row * f.cellH;   /* source Y                        */

    /* Build per-glyph transform state by mapping OLF_Font → OL_Surface fields. */
    OL_Surface glyph_src;
    memset(&glyph_src, 0, sizeof(glyph_src));
    if (f.texReady) {
        glyph_src.pixels   = f.sheet->pixels;
        glyph_src.realW    = f.sheet->realW;
        glyph_src.realH    = f.sheet->realH;
    }
    glyph_src.flipX       = f.flipX;
    glyph_src.flipY       = f.flipY;
    glyph_src.angle        = f.angle;
    glyph_src.scaleX       = f.scaleX;
    glyph_src.scaleY       = f.scaleY;
    glyph_src.pivotX       = f.pivotX;
    glyph_src.pivotY       = f.pivotY;
    glyph_src.z            = f.z;
    glyph_src.tint         = f.tint;

    gfx_blit(    /* blit from sheet to dst at (dx,dy)  */
        &glyph_src,           /* source surface (sheet)          */
        dx, dy,               /* dest coordinates                  */
        sx, sy,               /* source rect (cell offset)         */
        f.cellW, f.cellH,     /* cell dimensions                   */
        255,                  /* alpha: opaque                     */
        f.flipX, f.flipY,    /* flip flags (UV coordinate swap)   */
        0.0f,                 /* no rotation                       */
        1.0f, 1.0f,          /* identity scale                      */
        f.z,                  /* depth order                         */
        f.tint                /* ABGR color modulation               */
    );
    return 1;
}

/* ================================================================
 * font_print — render string on surface starting at (x0, y0).
 * ---------------------------------------------------------------
 * Advances cursor right by cellW per glyph; resets X to x0 on '\n'
 * and advances Y by cellH. Tabs expand to 4 cells. */

void font_print(OL_Surface *dst, int x0, int y0, const char *text, OLF_Font f)
{
    if (!dst || !f.nChars) return;

    /* Ensure texture is ready before drawing. */
    if (!ensure_texture(&f)) return;

    int x = x0, y = y0;      /* cursor starts at (x0, y0)  */
    for (size_t i = 0; text[i]; i++) {
        unsigned char ch = (unsigned char)text[i];

        if ((int)ch >= 32 && ((int)ch - 32) < (int)f.nChars) {
            /* Printable character: draw glyph and advance cursor */
            font_blt(dst, x, y, (char)ch, f);
            x += f.cellW;      /* move right one cell width  */

        } else if (ch == '\n') {
            y += f.cellH;      /* newline: drop down one row */
            x = x0;            /*   ...reset to left margin  */

        } else if (ch == '\t') {
            x += f.cellW * 4;  /* tab → indent 4 cells      */
        }
        /* Unprintable/invalid chars silently skipped. */
    }
}

/* ================================================================
 * Lua bindings for the font module
 * ---------------------------------------------------------------
 * Usage from Lua:
 *   font = require("engine/font")       -- or globally set to 'font'
 *   fnt  = font.load("font.png", 8, 16)
 *   image.blit_font(surf, x, y, 65, fnt)   -- draw 'A' at (x,y)
 */

static int l_font_new(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    int cellW  = (int)luaL_checkinteger(L, 2);
    int cellH  = (int)luaL_checkinteger(L, 3);

    OLF_Font f = font_load(path, cellW, cellH);
    if (!f.nChars) {
        lua_pushnil(L);
        return 1;
    }

    /* Wrap in Lua userdata with metatable for per-font .blt() method. */
    OLF_Font **p = (OLF_Font **)lua_newuserdata(L, sizeof(OLF_Font *));
    *p = malloc(sizeof(OLF_Font));
    memcpy(*p, &f, sizeof(OLF_Font));

    /* Metatable with 'blt' method for font:blt(dst, x, y, code) */
    lua_createtable(L, 0, 1);
    lua_pushcfunction(L, l_font_blt_obj);
    lua_setfield(L, -2, "blt");     /* __index.blt — per-instance method */
    lua_setmetatable(L, -2);

    return 1;
}

static int l_font_blt_obj(lua_State *L)
{
    OLF_Font **fn = (OLF_Font **)lua_touserdata(L, 1);
    int dx   = (int)luaL_checkinteger(L, 2);
    int dy   = (int)luaL_checkinteger(L, 3);
    char c   = (char)luaL_checkinteger(L, 4);

    if (lua_gettop(L) < 5)
        return luaL_error(L, "font:blt(dst, x, y, code): dst is required");

    OL_Surface *dst = (OL_Surface *)lua_touserdata(L, 5);
    int ret = font_blt(dst, dx, dy, c, (*fn)? **fn : (OLF_Font){0});
    lua_pushinteger(L, ret);   /* >0 = success, ==0 = error */
    return 1;
}

void font_api_register(lua_State *L)
{
    /* Create a font module table with font.load() function. */
    lua_createtable(L, 0, 2);              /* pre-allocate for 2 fields  */
    lua_pushcfunction(L, l_font_new);       /* font.new = ...             */
    lua_setfield(L, -2, "new");

    lua_pushcfunction(L, l_font_blt_obj);   /* font.blt = ... (for testing)*/
    lua_setfield(L, -2, "blt");

    lua_setglobal(L, "font");                /* global 'font' module       */
}
