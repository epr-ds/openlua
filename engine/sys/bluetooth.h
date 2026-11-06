/* OpenLua - Bluetooth compatibility stub.
 *
 * OneLua's PSP module index has no Bluetooth module and PSPSDK exposes no
 * Bluetooth stack; the PSP has no built-in Bluetooth radio.  The table exists
 * so scripts can probe it and receive a documented "unavailable" answer.
 */

#ifndef OL_BLUETOOTH_H
#define OL_BLUETOOTH_H

#include <lua.h>

void bluetooth_api_register(lua_State *L);

#endif
