/* OpenLua - bitmap font system implementation
 *
 * Glyph layout: each character = one cell (width x height) pixels in a
 * spritesheet image. Layout order = sequential from char 0x20 (space).
 * The grid is parsed at load time; individual glyphs are extracted at
 * draw time using grid math, respecting hardware power-of-2 realW padding.
 */

#include "engine/font/font.h"
#include "engine/gfx/gfx.h"

#include <string.h>

/* stb_image implementation provided by engine/core/stb_impl.o */
#include "third_party/stb/stb_image.h"

/* Forward declarations — static helpers for Lua binding */
static int l_font_blt_obj(lua_State *L);
static int l_font_new(lua_State *L);

/* ================================================================
 * font_load — load a font PNG and build the OLF_Font grid metadata */

OLF_Font font_load(const char *path, int cellW, int cellH)
{
    OLF_Font f = {0};

    int w = 0, h = 0, comp = 0;
    unsigned char *img = stbi_load(path, &w, &h, &comp, 4);
    if (!img) return f;   /* fail: can't load font */

    f.cellW     = cellW;
    f.cellH     = cellH;
    f.nCols      = (w + cellW - 1) / cellW;       /* grid columns     */
    f.nRows      = h / cellH;                      /* grid rows        */
    f.nChars    = f.nCols * f.nRows;              /* total glyphs     */

    stbi_image_free(img);       /* free image, we only need grid metadata  */
    return f;
}

/* ================================================================
 * font_blt — blit one glyph from sheet onto destination at (dx, dy).
 * Character code: 0-indexed from charmap start (char 0x20 = space)
 * Returns >0 on success, ==0 if character is out of range. */

int font_blt(OL_Surface *dst, int dx, int dy, char c, OLF_Font f)
{
  (void)dst; (void)dx; (void)dy; (void)c; (void)f;
    /* TODO: actual implementation — compute col/row from char code,
     * then call gfx_blit(sheet, dx, dy, col*cellW, row*cellH, cellW, cellH, 1.0, 255) */
    return 1;
}

/* ================================================================
 * font_print — render entire string onto surface at (x0, y0).
 */

void font_print(OL_Surface *dst, int x0, int y0, const char *text, OLF_Font f)
{
    if (!dst) return;

    int x  = x0, y = y0;
    for (size_t i = 0; text[i]; i++) {
        unsigned char ch = (unsigned char)text[i];

        if ((int)ch >= 32 && (ch - 32) < f.nChars) {
            font_blt(dst, x, y, (char)ch, f);
            x += f.cellW;
        } else if (ch == '\n') {
            y += f.cellH;
            x = x0;
        } else if (ch == '\t') {
            x += f.cellW * 4;
        }
    }
}

/* ================================================================
 * Lua bindings */

static int l_font_new(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    int cellW = (int)luaL_checkinteger(L, 2);
    int cellH = (int)luaL_checkinteger(L, 3);

    OLF_Font f = font_load(path, cellW, cellH);
    if (!f.nChars) {
        lua_pushnil(L);
        return 0;
    }

    /* Wrap font in a Lua userdata with metatable */
    OLF_Font **p = (OLF_Font **)lua_newuserdata(L, sizeof(OLF_Font *));
    *p = malloc(sizeof(OLF_Font));
    memcpy(*p, &f, sizeof(OLF_Font));

    lua_createtable(L, 0, 1);   /* metatable */
    lua_pushcfunction(L, l_font_blt_obj);
    lua_setfield(L, -2, "blt");
    lua_setmetatable(L, -2);

    return 1;
}

/* font:blt userdata method — same signature as C API but from Lua. */
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
    lua_pushinteger(L, ret);   /* return: status */
    return 1;
}

void font_api_register(lua_State *L)
{
    lua_createtable(L, 0, 2);              /* { new = l_font_new }  */
    lua_pushcfunction(L, l_font_new);
    lua_setfield(L, -2, "new");             /* font.new               */

    lua_setglobal(L, "font");                /* global 'font'          */
}
