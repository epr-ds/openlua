/* OpenLua - maps module (OneLua-compatible).
 *
 *    local m = map.new(tileset, mapData, tileW, tileH)
 *        tileset - an image (OL.Image): width in {8,16,32,64,128,256,512},
 *                  divided into tileW x tileH tiles
 *        mapData - 2-D table: array of rows, each row an array of tile idx
 *        tileW/tileH - tile size in pixels (8..512)
 *
 *    map.blit(m, x, y)   - draw the whole grid at screen (x, y)
 *    map.sizex(m)        - map width  in pixels (cols * tileW)
 *    map.sizey(m)        - map height in pixels (rows * tileH)
 *    map.set(m, tx, ty, v)  - set tile at 1-based (tx, ty)
 *    map.cols(m), map.rows(m) - grid dimensions in tiles (extra helpers)
 *
 * Tile index 0 renders nothing (standard GBA/OneLua convention).
 */

#include "engine/maps/maps.h"
#include "engine/img/image.h"
#include "engine/gfx/gfx.h"
#include "engine/core/olreg.h"

#include <lauxlib.h>
#include <lualib.h>
#include <stdlib.h>

#define MAP_MAX 8

typedef struct {
    int          used;
    OL_Surface  *tileset;     /* borrowed from the image; image must outlive */
    int          rows, cols;
    int          tileW, tileH;
    int         *data;        /* rows*cols tile indices                     */
    int          tcols;       /* tileset columns of tiles                   */
} OLEmap;

static OLEmap g_maps[MAP_MAX];

/* ---- validation -------------------------------------------------------- */

static int valid_tilesize(int v)
{
    return v == 8 || v == 16 || v == 32 || v == 64 || v == 128 ||
           v == 256 || v == 512;
}

/* ---- internal ops (pure logic, unit-testable) --------------------------- */

OLEmap *olmap_get(int h)
{
    if (h < 1 || h > MAP_MAX || !g_maps[h - 1].used)
        return NULL;
    return &g_maps[h - 1];
}

int olmap_at(OLEmap *m, int tx, int ty)
{
    if (tx < 1 || tx > m->cols || ty < 1 || ty > m->rows)
        return -1;
    return m->data[(ty - 1) * m->cols + (tx - 1)];
}

int olmap_set(OLEmap *m, int tx, int ty, int v)
{
    if (tx < 1 || tx > m->cols || ty < 1 || ty > m->rows)
        return -1;
    m->data[(ty - 1) * m->cols + (tx - 1)] = v;
    return 0;
}

/* tile-index -> tileset source rect */
int olmap_tile_rect(OLEmap *m, int tile, int *sx, int *sy)
{
    int col = tile % m->tcols;
    int row = tile / m->tcols;
    if (row * m->tileH >= m->tileset->h)
        return -1;
    *sx = col * m->tileW;
    *sy = row * m->tileH;
    return 0;
}

/* ---- Lua bindings ------------------------------------------------------ */

static int l_map_new(lua_State *L)
{
    OL_Surface *tileset = image_as_surface(L, 1);

    int tileW = (int)luaL_checkinteger(L, 3);
    int tileH = (int)luaL_checkinteger(L, 4);
    if (!valid_tilesize(tileW) || !valid_tilesize(tileH))
        luaL_error(L, "map.new: tile size must be 8,16,32,64,128,256 or 512");
    if (tileset->w % tileW != 0 || tileset->h % tileH != 0)
        luaL_error(L, "map.new: tileset %dx%d is not divisible by %dx%d",
                   tileset->w, tileset->h, tileW, tileH);

    luaL_argcheck(L, lua_istable(L, 2), 2, "table expected");
    int rows = (int)lua_objlen(L, 2);
    if (rows < 1)
        luaL_error(L, "map.new: mapData must be a non-empty table of rows");

    lua_rawgeti(L, 2, 1);
    if (!lua_istable(L, -1))
    {
        lua_pop(L, 1);
        luaL_error(L, "map.new: mapData rows must be tables");
    }
    int cols = (int)lua_objlen(L, -1);
    lua_pop(L, 1);
    if (cols < 1)
        luaL_error(L, "map.new: mapData rows must be non-empty tables");

    OLEmap *m = NULL;
    for (int i = 0; i < MAP_MAX; i++)
        if (!g_maps[i].used)
        { m = &g_maps[i]; break; }
    if (!m)
        luaL_error(L, "map.new: no free map slots (max %d)", MAP_MAX);

    m->data = malloc(sizeof(int) * (size_t)rows * cols);
    if (!m->data)
        luaL_error(L, "map.new: out of memory");

    for (int y = 1; y <= rows; y++) {
        lua_rawgeti(L, 2, y);
        for (int x = 1; x <= cols; x++) {
            lua_rawgeti(L, -1, x);
            int v = (int)luaL_checkinteger(L, -1);
            if (v < 0) v = 0;
            m->data[(y - 1) * cols + (x - 1)] = v;
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }

    m->tileset = tileset;
    m->rows    = rows;
    m->cols    = cols;
    m->tileW   = tileW;
    m->tileH   = tileH;
    m->tcols   = tileset->w / tileW;
    m->used    = 1;

    lua_pushinteger(L, (int)(m - g_maps) + 1);
    return 1;
}

static int l_map_blit(lua_State *L)
{
    OLEmap *m = olmap_get((int)luaL_checkinteger(L, 1));
    if (!m) luaL_error(L, "map.blit: unknown handle %d", (int)luaL_checkinteger(L, 1));

    int x  = (int)luaL_checkinteger(L, 2);
    int y  = (int)luaL_checkinteger(L, 3);
    int al = (lua_gettop(L) >= 4) ? (int)luaL_checkinteger(L, 4) : 255;

    for (int ty = 1; ty <= m->rows; ty++) {
        for (int tx = 1; tx <= m->cols; tx++) {
            int tile = olmap_at(m, tx, ty);
            if (tile <= 0)
                continue;                      /* 0 = empty              */
            int sx, sy;
            if (olmap_tile_rect(m, tile, &sx, &sy) != 0)
                continue;                      /* idx beyond sheet       */
            gfx_blit(m->tileset,
                     x + (tx - 1) * m->tileW, y + (ty - 1) * m->tileH,
                     sx, sy, m->tileW, m->tileH,
                     al, 0, 0, 0.0f, 1.0f, 1.0f, 0, 0xFFFFFFFF);
        }
    }
    return 0;
}

static int l_map_sizex(lua_State *L)
{
    OLEmap *m = olmap_get((int)luaL_checkinteger(L, 1));
    if (!m) luaL_error(L, "map.sizex: unknown handle %d", (int)luaL_checkinteger(L, 1));
    lua_pushinteger(L, m->cols * m->tileW);
    return 1;
}

static int l_map_sizey(lua_State *L)
{
    OLEmap *m = olmap_get((int)luaL_checkinteger(L, 1));
    if (!m) luaL_error(L, "map.sizey: unknown handle %d", (int)luaL_checkinteger(L, 1));
    lua_pushinteger(L, m->rows * m->tileH);
    return 1;
}

static int l_map_cols(lua_State *L)
{
    OLEmap *m = olmap_get((int)luaL_checkinteger(L, 1));
    if (!m) luaL_error(L, "map.cols: unknown handle %d", (int)luaL_checkinteger(L, 1));
    lua_pushinteger(L, m->cols);
    return 1;
}

static int l_map_rows(lua_State *L)
{
    OLEmap *m = olmap_get((int)luaL_checkinteger(L, 1));
    if (!m) luaL_error(L, "map.rows: unknown handle %d", (int)luaL_checkinteger(L, 1));
    lua_pushinteger(L, m->rows);
    return 1;
}

static int l_map_set(lua_State *L)
{
    OLEmap *m = olmap_get((int)luaL_checkinteger(L, 1));
    if (!m) luaL_error(L, "map.set: unknown handle %d", (int)luaL_checkinteger(L, 1));
    int tx  = (int)luaL_checkinteger(L, 2);
    int ty  = (int)luaL_checkinteger(L, 3);
    int v   = (int)luaL_checkinteger(L, 4);
    if (olmap_set(m, tx, ty, v) != 0)
        luaL_error(L, "map.set: tile (%d,%d) out of range %dx%d",
                   tx, ty, m->cols, m->rows);
    return 0;
}

static int l_map_get(lua_State *L)
{
    OLEmap *m = olmap_get((int)luaL_checkinteger(L, 1));
    if (!m) luaL_error(L, "map.get: unknown handle %d", (int)luaL_checkinteger(L, 1));
    int v = olmap_at(m, (int)luaL_checkinteger(L, 2), (int)luaL_checkinteger(L, 3));
    if (v < 0)
        lua_pushnil(L);
    else
        lua_pushinteger(L, v);
    return 1;
}

static int l_map_free(lua_State *L)
{
    int h  = (int)luaL_checkinteger(L, 1);
    OLEmap *m = olmap_get(h);
    if (m) { free(m->data); m->used = 0; }
    return 0;
}

static const luaL_Reg map_funcs[] = {
    { "new",     l_map_new },
    { "blit",    l_map_blit },
    { "sizex",   l_map_sizex },
    { "sizey",   l_map_sizey },
    { "cols",    l_map_cols },
    { "rows",    l_map_rows },
    { "set",     l_map_set },
    { "get",     l_map_get },
    { "free",    l_map_free },
    { NULL,      NULL }
};

void maps_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, map_funcs);
    lua_setglobal(L, "map");
}
