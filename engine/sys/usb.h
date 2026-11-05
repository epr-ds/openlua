/* OpenLua - OneLua-compatible USB mass-storage module.
 *
 *    usb.mstick()      -> nothing: expose the memory stick
 *    usb.umd()         -> nothing: expose the UMD (read-only ISO9660)
 *    usb.flash0..3()   -> nothing: expose flash0..3 read-only
 *    usb.stop()        -> nothing: stop the connection
 *    usb.isconnected() -> true when a PC has taken the connection
 *    usb.isactive()    -> true when a mode was activated (even without a PC)
 *
 * The flash and UMD modes need Sony's optional `_usbdevice.prx`, which is not
 * part of PSPSDK and is usually supplied by custom firmware.  If it is absent
 * the mode fails safely: nothing is exposed and isactive() stays false.
 *
 * USB storage and the GO! CAM camera cannot run at the same time.  Call
 * cam.term() (or usb.stop()) before switching.
 */

#ifndef OL_USB_H
#define OL_USB_H

#include <lua.h>

void usb_api_register(lua_State *L);

#endif
