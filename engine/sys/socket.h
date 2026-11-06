/* OpenLua - OneLua-compatible TCP socket module.
 *
 * Sockets are ordinary Lua userdata with a __gc metamethod, so a script that
 * forgets socket.close() still releases the descriptor at collection time.
 * socket.term() closes every open handle for deterministic shutdown.
 */

#ifndef OL_SOCKET_H
#define OL_SOCKET_H

#include <lua.h>

void socket_api_register(lua_State *L);

#endif
