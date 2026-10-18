#include <pspgu.h>
#include <math.h>

#include "draw.h"
#include "gfx.h"
#include "../core/olreg.h"

#include <lauxlib.h>

typedef struct VertC {
    unsigned int color;
    short x, y, z;
} VertC;

static void v_begin(void)
{
    if (!gfx_gu_started())
        return;
    sceGuStart(GU_DIRECT, gfx_list());
    sceGuDisable(GU_TEXTURE_2D);
    sceGuShadeModel(GU_FLAT);
}

static void v_end(void)
{
    if (!gfx_gu_started())
        return;
    sceGuShadeModel(GU_SMOOTH);
    sceGuFinish();
    sceGuSync(0, 0);
}

static VertC *alloc_verts(int n)
{
    return (VertC *)sceGuGetMemory(sizeof(VertC) * (size_t)n);
}

static void set_vert(VertC *v, int x, int y, unsigned int color)
{
    v->color = color | 0xFF000000;
    v->x = (short)x;
    v->y = (short)y;
    v->z = 0;
}

void ol_draw_line(int x1, int y1, int x2, int y2, unsigned int color)
{
    v_begin();
    if (!gfx_gu_started())
        return;
    VertC *v = alloc_verts(2);
    set_vert(&v[0], x1, y1, color);
    set_vert(&v[1], x2, y2, color);
    sceGuDrawArray(GU_LINES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
    v_end();
}

void ol_draw_rect(int x, int y, int w, int h, unsigned int color)
{
    if (w <= 0 || h <= 0)
        return;
    v_begin();
    if (!gfx_gu_started())
        return;
    VertC *v = alloc_verts(5);
    set_vert(&v[0], x, y, color);
    set_vert(&v[1], x + w, y, color);
    set_vert(&v[2], x + w, y + h, color);
    set_vert(&v[3], x, y + h, color);
    set_vert(&v[4], x, y, color);
    sceGuDrawArray(GU_LINE_STRIP, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 5, 0, v);
    v_end();
}

void ol_fill_rect(int x, int y, int w, int h, unsigned int color)
{
    if (w <= 0 || h <= 0)
        return;
    v_begin();
    if (!gfx_gu_started())
        return;
    VertC *v = alloc_verts(4);
    set_vert(&v[0], x, y, color);
    set_vert(&v[1], x + w, y, color);
    set_vert(&v[2], x + w, y + h, color);
    set_vert(&v[3], x, y + h, color);
    sceGuDrawArray(GU_TRIANGLE_FAN, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 4, 0, v);
    v_end();
}

static void draw_circle_impl(int x, int y, int radius, unsigned int color,
                             int sections, int filled)
{
    if (radius <= 0 || sections < 3)
        return;
    v_begin();
    if (!gfx_gu_started())
        return;
    int n = sections + (filled ? 1 : 1);
    VertC *v = alloc_verts(filled ? n : n + 1);
    int i;
    if (filled) {
        set_vert(&v[0], x, y, color);
        for (i = 0; i <= sections; i++) {
            float a = 2.0f * 3.14159265f * (float)i / (float)sections;
            set_vert(&v[i + 1], x + (int)(cosf(a) * radius),
                     y + (int)(sinf(a) * radius), color);
        }
        sceGuDrawArray(GU_TRIANGLE_FAN, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
                       sections + 2, 0, v);
    } else {
        for (i = 0; i <= sections; i++) {
            float a = 2.0f * 3.14159265f * (float)i / (float)sections;
            set_vert(&v[i], x + (int)(cosf(a) * radius),
                     y + (int)(sinf(a) * radius), color);
        }
        sceGuDrawArray(GU_LINE_STRIP, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
                       sections + 1, 0, v);
    }
    v_end();
}

void ol_draw_circle(int x, int y, int radius, unsigned int color, int sections)
{
    draw_circle_impl(x, y, radius, color, sections, 0);
}

void ol_fill_circle(int x, int y, int radius, unsigned int color, int sections)
{
    draw_circle_impl(x, y, radius, color, sections, 1);
}

static int l_line(lua_State *L)
{
    ol_draw_line((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2),
                 (int)luaL_checkinteger(L, 3), (int)luaL_checkinteger(L, 4),
                 (unsigned int)luaL_checkinteger(L, 5));
    return 0;
}

static int l_rect(lua_State *L)
{
    ol_draw_rect((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2),
                 (int)luaL_checkinteger(L, 3), (int)luaL_checkinteger(L, 4),
                 (unsigned int)luaL_checkinteger(L, 5));
    return 0;
}

static int l_fillrect(lua_State *L)
{
    ol_fill_rect((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2),
                 (int)luaL_checkinteger(L, 3), (int)luaL_checkinteger(L, 4),
                 (unsigned int)luaL_checkinteger(L, 5));
    return 0;
}

static int l_circle(lua_State *L)
{
    ol_draw_circle((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2),
                   (int)luaL_checkinteger(L, 3),
                   (unsigned int)luaL_checkinteger(L, 4),
                   luaL_optinteger(L, 5, 30));
    return 0;
}

void draw_api_register(lua_State *L)
{
    lua_createtable(L, 0, 4);

    lua_pushcfunction(L, l_line);
    lua_setfield(L, -2, "line");
    lua_pushcfunction(L, l_rect);
    lua_setfield(L, -2, "rect");
    lua_pushcfunction(L, l_fillrect);
    lua_setfield(L, -2, "fillrect");
    lua_pushcfunction(L, l_circle);
    lua_setfield(L, -2, "circle");

    lua_setglobal(L, "draw");
}
