/* OpenLua - OneLua-compatible PBP metadata and image extraction.
 *
 * The supported subset is read-only:
 *
 *    game.info(path)      -> PARAM.SFO fields, or nil
 *    game.geticon0(path)  -> image handle, or nil
 *    game.getpic0(path)   -> image handle, or nil
 *    game.getpic1(path)   -> image handle, or nil
 *
 * Paths must point at a PBP file.  ISO/CSO images and the OneLua
 * pack/add/remove/launch operations are not exposed.
 */

#ifndef OL_GAME_H
#define OL_GAME_H

#include <lua.h>

void game_api_register(lua_State *L);

#endif
