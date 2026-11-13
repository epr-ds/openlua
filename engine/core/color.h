/* OpenLua - color table (public API) */

#ifndef OL_COLOR_H
#define OL_COLOR_H

#include <lua.h>

/** Register the global `color` table: new/loadpalette and named colors. */
void color_api_register(lua_State *L);

#endif
