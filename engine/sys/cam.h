/* OpenLua - OneLua-compatible USB camera (GO! CAM) module.
 *
 * The documented surface is:
 *
 *    cam.init()                  -> 1/0
 *    cam.begin()                 -> 1/0
 *    cam.state()                 -> true/false
 *    cam.available()             -> true/false
 *    cam.render(x, y [, w, h])   -> nothing
 *    cam.shot(path)              -> 1/0 (PNG snapshot)
 *    cam.toimage()               -> image handle, or nil
 *    cam.direction()             -> 1/0
 *    cam.brightness([value])     -> 0..255, or -1
 *    cam.contrast([value])       -> 0..255, or -1
 *    cam.sharpness([value])      -> 0..255, or -1
 *    cam.saturation([value])     -> 0..255, or -1
 *    cam.zoom([value])           -> 1..5, or -1
 *    cam.evlevel([value])        -> 1..17, or -1
 *    cam.effect([value])         -> 1..7, or -1
 *    cam.autoreverse([mode])     -> 1/0
 *    cam.mirror([value])         -> 1/0, or -1
 *    cam.term()                  -> 1/0
 *
 * All calls are safe without a camera: init/begin return 0, state/available
 * return false, getters return -1, render is a no-op, and shot/toimage return
 * 0/nil.  The camera is a USB accessory and cannot be used at the same time as
 * the USB storage module.
 */

#ifndef OL_CAM_H
#define OL_CAM_H

#include <lua.h>

void cam_api_register(lua_State *L);

/* Pure value converters shared by the PSP and host builds.  OneLua exposes
 * 1-based effect/EV enums while the PSPSDK enums are 0-based; zoom is 1..5 in
 * Lua and 10..50 in the driver.  Out-of-range values are clamped. */
int cam_effect_to_psp(int value);
int cam_ev_to_psp(int value);
int cam_zoom_to_psp(int value);
int cam_effect_from_psp(int value);
int cam_ev_from_psp(int value);
int cam_zoom_from_psp(int value);

#endif
