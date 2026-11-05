/* OpenLua - OneLua-compatible SIO (serial) module.
 *
 *    sio.init()              -> 1/0
 *    sio.baud(baud)          -> nothing
 *    sio.available()         -> bytes waiting, or 0
 *    sio.read()              -> 0..255, or -1 when empty
 *    sio.write(data)         -> nothing
 *    sio.print(msg)          -> nothing
 *    sio.putstring(data,len) -> nothing
 *    sio.term()              -> 1/0
 *
 * PSPSDK exposes no user-mode SIO/UART system call.  The extra pspuart
 * library performs kernel-only register and interrupt-handler access, so it
 * cannot be used safely from this user-mode EBOOT.  The module is therefore
 * registered with the documented signatures but stays inactive: init reports
 * 0, available is 0, read is -1 and term succeeds.  Scripts can probe init()
 * and fall back without special-casing a missing global.  A future kernel-mode
 * build can replace sio_hw_* without changing the Lua surface.
 */

#ifndef OL_SIO_H
#define OL_SIO_H

#include <lua.h>

void sio_api_register(lua_State *L);

#endif
