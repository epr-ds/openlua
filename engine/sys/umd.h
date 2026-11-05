/* OpenLua - OneLua-compatible UMD module (read-only subset).
 *
 *    umd.init()      -> true/false: mount disc0: and wait for READY
 *    umd.present()   -> true/false: disc in the drive
 *    umd.type()      -> "GAME" / "VIDEO" / "AUDIO" / "-"
 *    umd.gameid()    -> DISC_ID string, or ""
 *    umd.info()      -> PARAM.SFO table, or nil
 *    umd.size()      -> total mounted size in GB, 4 decimals, as a string
 *    umd.term()      -> nothing: unmount disc0:
 *
 * The destructive OneLua calls (dumpiso and launch) are deliberately not
 * registered: they write disc images or hand control to another application
 * and need a separate, explicit safety review.
 *
 * umd.size() walks the mounted filesystem instead of reading raw sectors.  It
 * is therefore an estimate of the visible files and skips anything hidden by
 * the ISO9660 view; it is bounded so a hostile/broken tree cannot spin.
 */

#ifndef OL_UMD_H
#define OL_UMD_H

#include <lua.h>
#include <stddef.h>

void umd_api_register(lua_State *L);

/* Pure helpers, shared by the PSP and host builds and host-tested. */
const char *umd_type_name(unsigned int mask);
void umd_format_size(unsigned long long bytes, char *buf, size_t cap);

#endif
