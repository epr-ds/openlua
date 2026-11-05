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
#include <stddef.h>

void game_api_register(lua_State *L);

/* Decode a complete PARAM.SFO buffer and push it as a fresh table of fields.
 * Returns 1 on success and pushes one table; returns 0 on malformed input and
 * pushes nothing.  Shared with umd.info so both paths use the same parser. */
int game_push_sfo_table(lua_State *L, const unsigned char *data, size_t len);

#endif
