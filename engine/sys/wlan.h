/* OpenLua - OneLua-compatible wlan module.
 *
 * Connection-state and read-only query subset.  Sony's network-configuration
 * dialog and the destructive profile add/copy/delete/backup/restore calls are
 * intentionally not exposed.  Without a WiFi connection every query reports
 * the documented "not connected" result.
 */

#ifndef OL_WLAN_H
#define OL_WLAN_H

#include <lua.h>

void wlan_api_register(lua_State *L);

#endif
