/* OpenLua - OneLua-compatible FTP file server.
 *
 * ftp.init() starts a small anonymous, read-only FTP server rooted at the
 * current working directory.  ftp.term() stops it.  Writes, renames and
 * deletes are rejected; passive transfers only.
 */

#ifndef OL_FTP_H
#define OL_FTP_H

#include <lua.h>

int ftp_start(const char *root);
void ftp_stop(void);
int ftp_state(void);

void ftp_api_register(lua_State *L);

#endif
