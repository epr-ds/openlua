/* OpenLua - OneLua-compatible on-screen keyboard (OSK).
 *
 * The supported subset is `osk.init(desc, text [, type [, textmax
 * [, lines [, mode]]]])`, backed by the PSP `sceUtilityOsk` dialog.
 *
 * The call blocks until the user confirms or cancels.  Confirming returns the
 * edited string (which may be empty); cancelling returns nil.  The OSK runs
 * through the normal GE/display path and restores the 2D GE state and the
 * previous clear colour before returning, so the game frame loop can resume
 * immediately.
 */

#ifndef OL_OSK_H
#define OL_OSK_H

#include <lua.h>

#define OSK_TEXTMAX_DEFAULT 128
#define OSK_TEXTMAX_MIN     1
#define OSK_TEXTMAX_MAX     512
#define OSK_LINES_MIN       1
#define OSK_LINES_MAX       8

/* Resolved request, kept for host tests and diagnostics.  On the PSP it is
 * filled before the dialog runs and updated with the result. */
typedef struct {
    int type;        /* 1 = normal, 2 = numeric */
    int textmax;     /* clamped character limit */
    int lines;       /* clamped line count */
    int mode;        /* -1 or 0..10 (OneLua language code) */
    int language;    /* mapped SceUtilityOskInputLanguage value */
    int inputtype;   /* mapped SceUtilityOskInputType bit mask */
    int opened;      /* 1 once the PSP dialog was started */
    int cancelled;   /* 1 when the user cancelled */
    char result[OSK_TEXTMAX_MAX * 4 + 1];   /* UTF-8 result (host echo) */
} OL_OskStats;

void osk_api_register(lua_State *L);

/* Diagnostics for the host tests.  Not part of the Lua API. */
const OL_OskStats *osk_last_stats(void);

#endif
