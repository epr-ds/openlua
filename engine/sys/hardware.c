/* OpenLua - OneLua hardware information and control module.
 *
 * The safe, user-mode subset of the OneLua hw table is implemented here:
 *
 *   hw.getmodel()    PPSSPP nickname / ef0: heuristic + safe fallback
 *   hw.firmware()    sceKernelDevkitVersion()            (additive helper)
 *   hw.region()      "--": no reliable user-mode region query
 *   hw.volume([n])   /CONFIG/SYSTEM/SOUND main_volume (1..30)
 *   hw.mute([n])     /CONFIG/SYSTEM/SOUND mute (0/1)
 *   hw.headphone()   sceHprmIsHeadphoneExist()
 *   hw.audioout([n]) sceAudioRoutingSet/GetMode() 0=headphones, 1=speakers
 *
 * The motherboard/fuse/NAND identifiers need kernel-only IDStorage or syscon
 * access.  Their OneLua entry points are kept for compatibility but return
 * documented placeholders; the same applies to hw.led(), because
 * sceSysconCtrlLED() is a kernel driver call.  No undocumented kernel import
 * is used anywhere in this file.
 *
 * The host build uses fixed mocks and an in-memory sound registry so the Lua
 * surface can be exercised without a PSP.  Formatting and clamping are done
 * by the pure hw_* helpers below.
 */

#include "engine/sys/hardware.h"
#include "engine/core/olreg.h"

#include <lauxlib.h>
#include <stdio.h>
#include <string.h>

#if defined(__PSP__)
#include <pspkernel.h>
#include <psputility.h>
#include <pspsysmem.h>
#include <pspreg.h>
#include <pspaudiorouting.h>
#include <psphprm.h>
#include <sys/statvfs.h>

#define HW_SOUND_CATEGORY "/CONFIG/SYSTEM/SOUND"

/* PPSSPP 1.20.4 does not expose the CONFIG/SYSTEM/SOUND registry category.
 * Keep stable session-local values so hw.volume()/hw.mute() still return the
 * documented range; write-through is used whenever the category exists. */
static int fallback_volume = 27;
static int fallback_mute = 0;
#else
/* Host mocks model a PSP with the PPSSPP defaults.  They never touch the
 * host's own audio, registry, filesystem or LEDs. */
static int host_volume    = 15;
static int host_mute      = 0;
static int host_audioout  = 1;
static int host_headphone = 0;
static int host_leds[4]   = { 0, 0, 0, 0 };
#endif

/* ---------------------------------------------------------------------- */
/* Pure helpers                                                            */
/* ---------------------------------------------------------------------- */

static void copy_trunc(const char *src, char *out, size_t cap)
{
    size_t n;

    if (!out || cap == 0)
        return;
    if (!src)
        src = "";
    n = strlen(src);
    if (n >= cap)
        n = cap - 1;
    memcpy(out, src, n);
    out[n] = '\0';
}

void hw_format_hex(unsigned int value, char *out, size_t cap)
{
    char tmp[16];

    snprintf(tmp, sizeof(tmp), "0x%08X", value);
    copy_trunc(tmp, out, cap);
}

void hw_format_firmware(unsigned int version, char *out, size_t cap)
{
    char tmp[24];
    unsigned int major = (version >> 24) & 0xFFu;
    unsigned int minor = (version >> 16) & 0xFFu;
    unsigned int revision = (version >> 8) & 0xFFu;

    if (version == 0 || major == 0) {
        copy_trunc("UNK", out, cap);
        return;
    }

    /* 0x06060010 -> 6.60, 0x06060110 -> 6.61, 0x05050010 -> 5.50. */
    snprintf(tmp, sizeof(tmp), "%u.%u%u", major, minor, revision);
    copy_trunc(tmp, out, cap);
}

void hw_format_nand_mb(unsigned int megabytes, char *out, size_t cap)
{
    char tmp[24];

    if (megabytes == 0) {
        copy_trunc("UNK", out, cap);
        return;
    }
    snprintf(tmp, sizeof(tmp), "%uMB", megabytes);
    copy_trunc(tmp, out, cap);
}

const char *hw_model_name(int model)
{
    switch (model) {
    case 0: return "1000";
    case 1: return "2000";
    case 2: return "3000";
    case 3: return "Go";
    case 4: return "Street";
    case 5: return "Vita";
    case 6: return "Emulator";
    default: return "UNK";
    }
}

const char *hw_region_name(int region)
{
    switch (region) {
    case 0:  return "Japan";
    case 1:  return "America";
    case 2:  return "Australia";
    case 3:  return "United Kingdom";
    case 4:  return "Europe";
    case 5:  return "Korea";
    case 6:  return "Hong-Kong";
    case 7:  return "Taiwan";
    case 8:  return "Russia";
    case 9:  return "China";
    case 10: return "Mexico";
    default: return "--";
    }
}

int hw_clamp_volume(int level)
{
    if (level < 1)
        return 1;
    if (level > 30)
        return 30;
    return level;
}

int hw_clamp_mute(int mode)
{
    return mode ? 1 : 0;
}

int hw_clamp_audioout(int mode)
{
    return mode ? 1 : 0;
}

int hw_valid_led(int led)
{
    return led >= 0 && led <= 3;
}

/* ---------------------------------------------------------------------- */
/* Sound registry access                                                   */
/* ---------------------------------------------------------------------- */

#if defined(__PSP__)
static int hw_reg_open(struct RegParam *reg, REGHANDLE *h, REGHANDLE *hd,
                       int mode)
{
    memset(reg, 0, sizeof(*reg));
    reg->regtype = 1;
    reg->namelen = (unsigned int)strlen("/system");
    reg->unk2 = 1;
    reg->unk3 = 1;
    strcpy(reg->name, "/system");

    if (sceRegOpenRegistry(reg, mode, h) < 0)
        return -1;
    if (sceRegOpenCategory(*h, HW_SOUND_CATEGORY, mode, hd) < 0) {
        sceRegCloseRegistry(*h);
        return -1;
    }
    return 0;
}

static int hw_read_sound_key(const char *key, unsigned int *out)
{
    struct RegParam reg;
    REGHANDLE h = 0, hd = 0;
    unsigned int value = 0;
    int rc;

    if (!out)
        return -1;
    if (hw_reg_open(&reg, &h, &hd, 1) < 0)
        return -1;
    rc = sceRegGetKeyValueByName(hd, key, &value, sizeof(value));
    sceRegCloseCategory(hd);
    sceRegCloseRegistry(h);
    if (rc < 0)
        return -1;
    *out = value;
    return 0;
}

static int hw_write_sound_key(const char *key, unsigned int value)
{
    struct RegParam reg;
    REGHANDLE h = 0, hd = 0;
    int rc;

    if (hw_reg_open(&reg, &h, &hd, 2) < 0)
        return -1;
    rc = sceRegSetKeyValue(hd, key, &value, sizeof(value));
    if (rc >= 0) {
        sceRegFlushCategory(hd);
        sceRegFlushRegistry(h);
    }
    sceRegCloseCategory(hd);
    sceRegCloseRegistry(h);
    return rc < 0 ? -1 : 0;
}

static int hw_audioout_get(void)
{
    return sceAudioRoutingGetMode();
}

static int hw_volume_get(void)
{
    unsigned int value;

    if (hw_read_sound_key("main_volume", &value) == 0 && value <= 30)
        return (int)value;
    return fallback_volume;
}

static int hw_volume_set(int level)
{
    level = hw_clamp_volume(level);
    fallback_volume = level;
    (void)hw_write_sound_key("main_volume", (unsigned int)level);
    return level;
}

static int hw_mute_get(void)
{
    unsigned int value;

    if (hw_read_sound_key("mute", &value) == 0)
        return value ? 1 : 0;
    return fallback_mute;
}

static int hw_mute_set(int mode)
{
    mode = hw_clamp_mute(mode);
    fallback_mute = mode;
    (void)hw_write_sound_key("mute", (unsigned int)mode);
    return mode;
}

static int hw_audioout_set(int mode)
{
    int rc = sceAudioRoutingSetMode(mode);
    return rc < 0 ? rc : 0;
}

static int hw_headphone_get(void)
{
    return sceHprmIsHeadphoneExist() > 0 ? 1 : 0;
}

static int hw_led_set(int led, int state)
{
    /* sceSysconCtrlLED() lives in the kernel-only sceSyscon_driver library.
     * Deliberately do not import it from a user-mode EBOOT. */
    (void)led;
    (void)state;
    return -1;
}
#else
static int host_sound_key(const char *key)
{
    if (strcmp(key, "main_volume") == 0)
        return 0;
    if (strcmp(key, "mute") == 0)
        return 1;
    return -1;
}

static int hw_read_sound_key(const char *key, unsigned int *out)
{
    int which = host_sound_key(key);

    if (!out || which < 0)
        return -1;
    *out = (unsigned int)(which == 0 ? host_volume : host_mute);
    return 0;
}

static int hw_write_sound_key(const char *key, unsigned int value)
{
    int which = host_sound_key(key);

    if (which == 0)
        host_volume = (int)value;
    else if (which == 1)
        host_mute = value ? 1 : 0;
    else
        return -1;
    return 0;
}

static int hw_audioout_get(void)
{
    return host_audioout;
}

static int hw_volume_get(void)
{
    unsigned int value;

    if (hw_read_sound_key("main_volume", &value) == 0 && value <= 30)
        return (int)value;
    return 15;
}

static int hw_volume_set(int level)
{
    level = hw_clamp_volume(level);
    (void)hw_write_sound_key("main_volume", (unsigned int)level);
    return level;
}

static int hw_mute_get(void)
{
    unsigned int value;

    if (hw_read_sound_key("mute", &value) == 0)
        return value ? 1 : 0;
    return 0;
}

static int hw_mute_set(int mode)
{
    mode = hw_clamp_mute(mode);
    (void)hw_write_sound_key("mute", (unsigned int)mode);
    return mode;
}

static int hw_audioout_set(int mode)
{
    host_audioout = mode ? 1 : 0;
    return 0;
}

static int hw_headphone_get(void)
{
    return host_headphone;
}

static int hw_led_set(int led, int state)
{
    if (led < 0 || led > 3)
        return -1;
    host_leds[led] = state ? 1 : 0;
    return 0;
}
#endif

/* ---------------------------------------------------------------------- */
/* Lua bindings                                                            */
/* ---------------------------------------------------------------------- */

static int l_unknown_string(lua_State *L)
{
    lua_pushliteral(L, "UNK");
    return 1;
}

static int l_pommel(lua_State *L)
{
    lua_pushnumber(L, 0);
    return 1;
}

static int l_pandora(lua_State *L)
{
    lua_pushboolean(L, 0);
    return 1;
}

static int l_region(lua_State *L)
{
#if defined(__PSP__)
    /* IDStorage holds the region, but reading it requires kernel mode. */
    lua_pushliteral(L, "--");
#else
    lua_pushstring(L, hw_region_name(1));
#endif
    return 1;
}

static int l_getmodel(lua_State *L)
{
#if defined(__PSP__)
    char nickname[64];
    struct statvfs vfs;

    /* PPSSPP's default nickname is a stable, harmless emulator marker.  A
     * real PSP with the same nickname merely reports "Emulator"; it cannot
     * affect any other operation. */
    if (sceUtilityGetSystemParamString(PSP_SYSTEMPARAM_ID_STRING_NICKNAME,
                                       nickname, sizeof(nickname)) >= 0) {
        nickname[sizeof(nickname) - 1] = '\0';
        if (strcmp(nickname, "PPSSPP") == 0) {
            lua_pushliteral(L, "Emulator");
            return 1;
        }
    }

    /* Only the PSP Go has the internal ef0: storage.  This identifies a Go
     * without touching kernel APIs, but cannot distinguish 1000/2000/3000. */
    if (statvfs("ef0:/", &vfs) == 0) {
        lua_pushliteral(L, "Go");
        return 1;
    }
    lua_pushliteral(L, "UNK");
#else
    lua_pushliteral(L, "Emulator");
#endif
    return 1;
}

static int l_firmware(lua_State *L)
{
    unsigned int version;
    char buf[24];

#if defined(__PSP__)
    version = (unsigned int)sceKernelDevkitVersion();
#else
    version = 0x06060010u;
#endif
    hw_format_firmware(version, buf, sizeof(buf));
    lua_pushstring(L, buf);
    return 1;
}

static int l_volume(lua_State *L)
{
    if (lua_gettop(L) >= 1) {
        int level = hw_clamp_volume((int)luaL_checkinteger(L, 1));
        lua_pushnumber(L, (lua_Number)hw_volume_set(level));
        return 1;
    }

    lua_pushnumber(L, (lua_Number)hw_volume_get());
    return 1;
}

static int l_mute(lua_State *L)
{
    if (lua_gettop(L) >= 1) {
        hw_mute_set((int)luaL_checkinteger(L, 1));
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, hw_mute_get());
    return 1;
}

static int l_headphone(lua_State *L)
{
    lua_pushnumber(L, hw_headphone_get() ? 1 : 0);
    return 1;
}

static int l_audioout(lua_State *L)
{
    if (lua_gettop(L) >= 1) {
        int mode = hw_clamp_audioout((int)luaL_checkinteger(L, 1));
        int rc = hw_audioout_set(mode);
        lua_pushnumber(L, rc < 0 ? rc : 0);
        return 1;
    }

    {
        int mode = hw_audioout_get();
        lua_pushnumber(L, mode < 0 ? -1 : (mode ? 1 : 0));
    }
    return 1;
}

static int l_led(lua_State *L)
{
    int led = (int)luaL_checkinteger(L, 1);
    int state = luaL_checkinteger(L, 2) ? 1 : 0;

    if (!hw_valid_led(led))
        lua_pushnumber(L, -1);
    else
        lua_pushnumber(L, hw_led_set(led, state));
    return 1;
}

static const luaL_Reg hw_funcs[] = {
    { "baryon",     l_unknown_string },
    { "tachyon",    l_unknown_string },
    { "pommel",     l_pommel         },
    { "fuseid",     l_unknown_string },
    { "fusecfg",    l_unknown_string },
    { "kirk",       l_unknown_string },
    { "spock",      l_unknown_string },
    { "nandseed",   l_unknown_string },
    { "ofwinitial", l_unknown_string },
    { "board",      l_unknown_string },
    { "gen",        l_unknown_string },
    { "region",     l_region         },
    { "getmodel",   l_getmodel       },
    { "firmware",   l_firmware       },
    { "pandora",    l_pandora        },
    { "nand",       l_unknown_string },
    { "volume",     l_volume         },
    { "mute",       l_mute           },
    { "headphone",  l_headphone      },
    { "audioout",   l_audioout       },
    { "led",        l_led            },
    { NULL, NULL }
};

void hardware_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, hw_funcs);
    lua_setglobal(L, "hw");
}
