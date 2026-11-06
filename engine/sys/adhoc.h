/* OpenLua - OneLua-compatible adhoc module.
 *
 * Lifecycle, MAC and peer-list subset.  The matching-request and PDP data
 * calls are registered with their documented signatures but report "nothing
 * available" in this build; see docs/API.md.
 */

#ifndef OL_ADHOC_H
#define OL_ADHOC_H

#include <lua.h>

void adhoc_api_register(lua_State *L);

#endif
