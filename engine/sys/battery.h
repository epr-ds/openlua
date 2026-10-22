#ifndef OL_BATTERY_H
#define OL_BATTERY_H

#include <stddef.h>

#include <lua.h>

void battery_api_register(lua_State *L);

/* Pure formatting helpers, kept separate from the Lua bindings so the host
 * tests can exercise the edge cases without a PSP battery. */
void batt_format_lifetime(int minutes, char *out, size_t cap);
void batt_format_volt(int millivolts, char *out, size_t cap);
void batt_format_capacity(int milliamps, char *out, size_t cap);

#endif
