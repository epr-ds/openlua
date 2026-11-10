/* OpenLua - OneLua-compatible USB mass-storage module.
 *
 *    usb.mstick()      -> nothing: expose the memory stick
 *    usb.umd()         -> nothing: unavailable in the standard build
 *    usb.flash0..3()   -> nothing: unavailable in the standard build
 *    usb.stop()        -> nothing: stop the connection
 *    usb.isconnected() -> true when a PC has taken the connection
 *    usb.isactive()    -> true when a mode was activated (even without a PC)
 *
 * Only the memory-stick mode is built in.  The UMD and flash modes need Sony's
 * optional `pspUsbDevice` library (`_usbdevice.prx`), which is not part of the
 * firmware on a normal CFW boot.  Importing it statically makes the EBOOT fail
 * to load with 8002013C (SCE_KERNEL_ERROR_LIBRARY_NOTFOUND), so the standard
 * build omits it and these calls are safe no-ops that leave isactive() false.
 *
 * USB storage and the GO! CAM camera cannot run at the same time.  Call
 * cam.term() (or usb.stop()) before switching.
 */

#ifndef OL_USB_H
#define OL_USB_H

#include <lua.h>

void usb_api_register(lua_State *L);

#endif
