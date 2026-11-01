/* OpenLua - OneLua-compatible on-screen keyboard (OSK).
 *
 * `osk.init(desc, text [, type [, textmax [, lines [, mode]]]])` opens the
 * PSP system keyboard and blocks until the user confirms or cancels.
 *
 * Defaults and clamping:
 *   type     1 (normal)       2 selects the numeric keypad; anything else -> 1
 *   textmax  128              clamped to 1..512 characters
 *   lines    1                clamped to 1..8 lines
 *   mode     -1 (firmware)    OneLua language code, clamped to -1..10
 *
 * The OneLua language list is mapped by meaning to
 * SceUtilityOskInputLanguage.  The PSP enum has no Chinese entries, so modes
 * 9/10 (Traditional/Simplified Chinese) fall back to the firmware default,
 * which is documented in docs/API.md.
 *
 * Confirming returns the edited UTF-8 string, including "" for empty input.
 * Cancelling (or an OSK start/status failure) returns nil.
 */

#include "engine/sys/osk.h"
#include "engine/core/olreg.h"

#include <lauxlib.h>
#include <string.h>

#if defined(__PSP__)
#include <pspdisplay.h>
#include <pspgu.h>
#include <psptypes.h>
#include <psputility.h>
#include <psputility_osk.h>

#include "engine/gfx/gfx.h"
#include "engine/gfx/gfx3d.h"
#endif

#define OSK_DESC_MAX 128

static OL_OskStats g_last;

const OL_OskStats *osk_last_stats(void)
{
    return &g_last;
}

/* ---------------------------------------------------------------------- */
/* UTF-8 <-> UTF-16 helpers                                                */
/* ---------------------------------------------------------------------- */

static unsigned int read_utf8(const unsigned char *s, size_t len, size_t *used)
{
    unsigned char c = s[0];

    if (c < 0x80) {
        *used = 1;
        return c;
    }

    int n;
    unsigned int cp;
    if ((c & 0xE0) == 0xC0) {
        n = 2;
        cp = c & 0x1F;
    } else if ((c & 0xF0) == 0xE0) {
        n = 3;
        cp = c & 0x0F;
    } else if ((c & 0xF8) == 0xF0) {
        n = 4;
        cp = c & 0x07;
    } else {
        *used = 1;
        return 0xFFFD;
    }

    if ((size_t)n > len) {
        *used = 1;
        return 0xFFFD;
    }

    for (int i = 1; i < n; i++) {
        unsigned char cc = s[i];
        if ((cc & 0xC0) != 0x80) {
            *used = 1;
            return 0xFFFD;
        }
        cp = (cp << 6) | (cc & 0x3F);
    }

    if ((n == 2 && cp < 0x80) ||
        (n == 3 && cp < 0x800) ||
        (n == 4 && cp < 0x10000) ||
        (cp >= 0xD800 && cp <= 0xDFFF) ||
        cp > 0x10FFFF) {
        *used = 1;
        return 0xFFFD;
    }

    *used = (size_t)n;
    return cp;
}

/* Convert at most `cap` UTF-16 code units (excluding the terminator).
 * Returns the number of units written. */
static int utf8_to_utf16(const char *src, size_t len,
                         unsigned short *dst, int cap)
{
    int n = 0;
    size_t i = 0;

    while (i < len && n < cap) {
        size_t used = 0;
        unsigned int cp = read_utf8((const unsigned char *)src + i, len - i,
                                    &used);
        i += used;

        if (cp > 0xFFFF) {
            if (n + 2 > cap)
                break;
            cp -= 0x10000;
            dst[n++] = (unsigned short)(0xD800 | (cp >> 10));
            dst[n++] = (unsigned short)(0xDC00 | (cp & 0x3FF));
        } else {
            dst[n++] = (unsigned short)cp;
        }
    }

    dst[n] = 0;
    return n;
}

static int utf16_to_utf8(const unsigned short *src, int units,
                         char *dst, size_t cap)
{
    size_t n = 0;
    int i = 0;

    while (i < units && src[i]) {
        unsigned int cp = src[i++];
        if (cp >= 0xD800 && cp <= 0xDBFF && i < units &&
            src[i] >= 0xDC00 && src[i] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (src[i++] - 0xDC00);
        } else if (cp >= 0xD800 && cp <= 0xDFFF) {
            cp = 0xFFFD;
        }

        unsigned char out[4];
        int bytes;
        if (cp < 0x80) {
            out[0] = (unsigned char)cp;
            bytes = 1;
        } else if (cp < 0x800) {
            out[0] = (unsigned char)(0xC0 | (cp >> 6));
            out[1] = (unsigned char)(0x80 | (cp & 0x3F));
            bytes = 2;
        } else if (cp < 0x10000) {
            out[0] = (unsigned char)(0xE0 | (cp >> 12));
            out[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
            out[2] = (unsigned char)(0x80 | (cp & 0x3F));
            bytes = 3;
        } else {
            out[0] = (unsigned char)(0xF0 | (cp >> 18));
            out[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
            out[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
            out[3] = (unsigned char)(0x80 | (cp & 0x3F));
            bytes = 4;
        }

        if (n + (size_t)bytes >= cap)
            break;
        memcpy(dst + n, out, (size_t)bytes);
        n += (size_t)bytes;
    }

    dst[n] = 0;
    return (int)n;
}

/* ---------------------------------------------------------------------- */
/* Argument handling                                                       */
/* ---------------------------------------------------------------------- */

static int clamp_int(int value, int lo, int hi)
{
    if (value < lo)
        return lo;
    if (value > hi)
        return hi;
    return value;
}

/* OneLua language code -> SceUtilityOskInputLanguage.  The PSPSDK enum has
 * no Chinese values, so 9/10 use the firmware default. */
static int map_language(int mode)
{
    switch (mode) {
    case 0:  return 1;   /* Japanese */
    case 1:  return 2;   /* English */
    case 2:  return 4;   /* Spanish (PSP 3 is French, not in OneLua's list) */
    case 3:  return 5;   /* German */
    case 4:  return 6;   /* Italian */
    case 5:  return 7;   /* Dutch */
    case 6:  return 8;   /* Portuguese */
    case 7:  return 9;   /* Russian */
    case 8:  return 10;  /* Korean */
    default: return 0;   /* firmware default / unsupported Chinese */
    }
}

static int opt_int(lua_State *L, int idx, int def, const char *name)
{
    if (lua_isnoneornil(L, idx))
        return def;
    if (lua_type(L, idx) != LUA_TNUMBER)
        luaL_error(L, "osk: %s must be a number", name);
    return (int)lua_tonumber(L, idx);
}

static void resolve_request(lua_State *L, int *type, int *textmax,
                            int *lines, int *mode)
{
    /* desc and text are required actual strings (numbers are not coerced). */
    if (lua_type(L, 1) != LUA_TSTRING)
        luaL_error(L, "osk: desc must be a string");
    if (lua_type(L, 2) != LUA_TSTRING)
        luaL_error(L, "osk: text must be a string");

    int t = opt_int(L, 3, 1, "type");
    int tm = opt_int(L, 4, OSK_TEXTMAX_DEFAULT, "textmax");
    int ln = opt_int(L, 5, 1, "lines");
    int md = opt_int(L, 6, -1, "mode");

    *type = (t == 2) ? 2 : 1;
    *textmax = clamp_int(tm, OSK_TEXTMAX_MIN, OSK_TEXTMAX_MAX);
    *lines = clamp_int(ln, OSK_LINES_MIN, OSK_LINES_MAX);

    if (md < -1 || md > 10)
        md = -1;
    *mode = md;
}

/* ---------------------------------------------------------------------- */
/* PSP dialog                                                              */
/* ---------------------------------------------------------------------- */

#if defined(__PSP__)

static void restore_display(unsigned int saved_clear)
{
    gfx_set_clear_color(saved_clear);

    if (!gfx_gu_started())
        return;

    /* Leave the GE in the normal 2D state and clear both buffers so no OSK
     * pixels survive into the next game frame. */
    gfx3d_finish();
    gfx_clear_scene();
    sceGuSwapBuffers();
    sceDisplayWaitVblankStart();
    gfx_clear_scene();
}

static int run_dialog(const char *desc, size_t desc_len,
                      const char *text, size_t text_len,
                      int type, int textmax, int lines, int mode)
{
    unsigned short descBuf[OSK_DESC_MAX + 1];
    unsigned short inBuf[OSK_TEXTMAX_MAX + 1];
    unsigned short outBuf[OSK_TEXTMAX_MAX + 1];

    memset(descBuf, 0, sizeof(descBuf));
    memset(inBuf, 0, sizeof(inBuf));
    memset(outBuf, 0, sizeof(outBuf));

    utf8_to_utf16(desc, desc_len, descBuf, OSK_DESC_MAX);
    utf8_to_utf16(text, text_len, inBuf, textmax);

    SceUtilityOskData data;
    memset(&data, 0, sizeof(data));
    data.language = map_language(mode);
    data.inputtype = (type == 2)
        ? PSP_UTILITY_OSK_INPUTTYPE_LATIN_DIGIT
        : PSP_UTILITY_OSK_INPUTTYPE_ALL;
    data.lines = lines;
    data.unk_24 = 1;
    data.desc = descBuf;
    data.intext = inBuf;
    data.outtextlength = textmax + 1;
    data.outtextlimit = textmax;
    data.outtext = outBuf;

    SceUtilityOskParams params;
    memset(&params, 0, sizeof(params));
    params.base.size = sizeof(params);

    int language = 0;
    int button_swap = 0;
    if (sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE,
                                    &language) != 0)
        language = 0;
    if (sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_BUTTON_SWAP,
                                    &button_swap) != 0)
        button_swap = 0;
    params.base.language = language;
    params.base.buttonSwap = button_swap;
    params.base.graphicsThread = 17;
    params.base.accessThread = 19;
    params.base.fontThread = 18;
    params.base.soundThread = 16;
    params.datacount = 1;
    params.data = &data;

    unsigned int saved_clear = gfx_clear_color();
    gfx_set_clear_color(0xFF000000u);
    gfx3d_finish();
    if (gfx_gu_started())
        gfx_clear_scene();

    int ret = sceUtilityOskInitStart(&params);
    if (ret < 0) {
        restore_display(saved_clear);
        return 0;    /* nil: the dialog could not be started */
    }

    g_last.opened = 1;

    int done = 0;
    int failed = 0;
    while (!done) {
        if (gfx_gu_started())
            gfx_clear_scene();

        int status = sceUtilityOskGetStatus();
        if (status < 0) {
            failed = 1;
            break;
        }

        switch (status) {
        case PSP_UTILITY_DIALOG_VISIBLE:
            sceUtilityOskUpdate(1);
            break;
        case PSP_UTILITY_DIALOG_QUIT:
            sceUtilityOskShutdownStart();
            break;
        case PSP_UTILITY_DIALOG_NONE:
            done = 1;
            break;
        default:
            /* INIT / FINISHED: keep presenting frames. */
            break;
        }

        sceDisplayWaitVblankStart();
        if (gfx_gu_started())
            sceGuSwapBuffers();
    }

    restore_display(saved_clear);

    if (failed) {
        g_last.cancelled = 1;
        return 0;    /* nil: the dialog failed */
    }

    if (data.result == PSP_UTILITY_OSK_RESULT_CANCELLED) {
        g_last.cancelled = 1;
        return 0;    /* nil */
    }

    if (data.result != PSP_UTILITY_OSK_RESULT_UNCHANGED &&
        data.result != PSP_UTILITY_OSK_RESULT_CHANGED) {
        return 0;    /* nil */
    }

    int units = 0;
    while (units < textmax && outBuf[units])
        units++;
    utf16_to_utf8(outBuf, units, g_last.result, sizeof(g_last.result));
    return 1;        /* empty string is a valid result */
}

#else   /* host: validate and echo the resolved text */

static int run_dialog(const char *desc, size_t desc_len,
                      const char *text, size_t text_len,
                      int type, int textmax, int lines, int mode)
{
    (void)desc;
    (void)desc_len;
    (void)mode;

    unsigned short inBuf[OSK_TEXTMAX_MAX + 1];
    memset(inBuf, 0, sizeof(inBuf));
    utf8_to_utf16(text, text_len, inBuf, textmax);

    int units = 0;
    while (units < textmax && inBuf[units])
        units++;
    utf16_to_utf8(inBuf, units, g_last.result, sizeof(g_last.result));

    (void)type;
    (void)lines;
    return 1;
}

#endif

/* ---------------------------------------------------------------------- */
/* Lua bindings                                                            */
/* ---------------------------------------------------------------------- */

static int l_init(lua_State *L)
{
    int type, textmax, lines, mode;
    resolve_request(L, &type, &textmax, &lines, &mode);

    size_t desc_len = 0;
    size_t text_len = 0;
    const char *desc = lua_tolstring(L, 1, &desc_len);
    const char *text = lua_tolstring(L, 2, &text_len);

    memset(&g_last, 0, sizeof(g_last));
    g_last.type = type;
    g_last.textmax = textmax;
    g_last.lines = lines;
    g_last.mode = mode;
    g_last.language = map_language(mode);
    g_last.inputtype = (type == 2) ? 1 : 0;

    int rc = run_dialog(desc, desc_len, text, text_len,
                        type, textmax, lines, mode);
    if (!rc) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushstring(L, g_last.result);
    return 1;
}

#if !defined(__PSP__)
static int l_last(lua_State *L)
{
    lua_newtable(L);
    lua_pushinteger(L, g_last.type);
    lua_setfield(L, -2, "type");
    lua_pushinteger(L, g_last.textmax);
    lua_setfield(L, -2, "textmax");
    lua_pushinteger(L, g_last.lines);
    lua_setfield(L, -2, "lines");
    lua_pushinteger(L, g_last.mode);
    lua_setfield(L, -2, "mode");
    lua_pushinteger(L, g_last.language);
    lua_setfield(L, -2, "language");
    lua_pushinteger(L, g_last.inputtype);
    lua_setfield(L, -2, "inputtype");
    lua_pushinteger(L, g_last.opened);
    lua_setfield(L, -2, "opened");
    lua_pushinteger(L, g_last.cancelled);
    lua_setfield(L, -2, "cancelled");
    lua_pushstring(L, g_last.result);
    lua_setfield(L, -2, "result");
    return 1;
}
#endif

static const luaL_Reg osk_funcs[] = {
    { "init", l_init },
#if !defined(__PSP__)
    { "_last", l_last },   /* host test hook only */
#endif
    { NULL, NULL }
};

void osk_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, osk_funcs);
    lua_setglobal(L, "osk");
}
