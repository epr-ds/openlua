#ifndef OL_DRAW_H
#define OL_DRAW_H

#include <lua.h>

void draw_api_register(lua_State *L);

void ol_draw_line(int x1, int y1, int x2, int y2, unsigned int color);
void ol_draw_rect(int x, int y, int w, int h, unsigned int color);
void ol_fill_rect(int x, int y, int w, int h, unsigned int color);
void ol_draw_circle(int x, int y, int radius, unsigned int color, int sections);
void ol_fill_circle(int x, int y, int radius, unsigned int color, int sections);

#endif
