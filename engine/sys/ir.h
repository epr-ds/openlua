/* OpenLua - OneLua-compatible infrared (IR) module.
 *
 *    ir.init()      -> 1/0
 *    ir.baud(baud)  -> nothing
 *    ir.state()     -> true/false
 *    ir.get()       -> 0..255, or -1 when empty
 *    ir.put(data)   -> 1/0
 *    ir.read()      -> string + length, or nil
 *    ir.write(msg)  -> bytes sent, or -1
 *    ir.term()      -> 1/0
 *
 * Raw IR byte streams require a kernel-mode SIRC/IrDA driver.  PSPSDK only
 * provides sceSircsSend (whole remote-control frames), which is not the same
 * interface, so this user-mode build registers the documented signatures but
 * stays inactive: init is 0, state is false, get is -1, read is nil and write
 * is -1.  Only PSP-1000 models have the IR port at all.  A future kernel-mode
 * backend can replace ir_hw_* without changing the Lua surface.
 */

#ifndef OL_IR_H
#define OL_IR_H

#include <lua.h>

void ir_api_register(lua_State *L);

#endif
