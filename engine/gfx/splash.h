/* OpenLua - OneLua-compatible splash screens.
 *
 * The supported subset is `splash.show`, `splash.old` and
 * `splash.oldschool`. Images are 480x272-friendly PNGs (512px GE limit),
 * shown centred on black, either one at a time or as a short slideshow.
 * Missing assets are skipped; if nothing can be loaded an original
 * geometric fallback is shown instead.
 */

#ifndef OL_SPLASH_H
#define OL_SPLASH_H

#include <lua.h>

#define SPLASH_MAX_IMAGES 32
#define SPLASH_PATH_MAX 512

typedef struct {
    int requested;   /* entries supplied by the caller */
    int loaded;      /* images decoded and displayed */
    int missing;     /* entries that could not be loaded */
    int fallback;    /* 1 when the built-in screen was used */
} OL_SplashStats;

void splash_api_register(lua_State *L);

/* Diagnostics for the host tests.  Not part of the Lua API. */
const OL_SplashStats *splash_last_stats(void);

#endif
