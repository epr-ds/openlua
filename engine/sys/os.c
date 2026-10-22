/* OpenLua - OneLua os compatibility module.
 *
 * The standard Lua os table is kept intact: os.time, os.date and os.exit are
 * still the stock Lua functions.  This module only adds the OneLua-specific
 * system queries and utility calls:
 *
 *    os.delay(ms)             sceKernelDelayThread(ms * 1000)
 *    os.getdate()             sceRtcGetCurrentClockLocalTime()
 *    os.nick()                sceUtilityGetSystemParamString(NICKNAME)
 *    os.mac()                 sceWlanGetEtherAddr()
 *    os.language()            sceUtilityGetSystemParamInt(LANGUAGE)
 *    os.ram()                 sceKernelTotalFreeMemSize()
 *    os.totalram()            free memory cached at registration
 *    os.cpu([mhz])            scePowerGet/SetCpuClockFrequency()
 *    os.bus()                 scePowerGetBusClockFrequency()
 *    os.infoms0()/infoef0()   statvfs() over ms0:/ and ef0:/
 *    os.cfw()                 "UNK": no reliable public CFW detector
 *    os.versiontxt()          flash0:/etc/version.txt
 *    os.restart()             sceKernelLoadExec(cwd/EBOOT.PBP), PSP only
 *    os.message() etc.        documented, predictable no-op stubs
 *
 * Host builds use fixed mocks for the console-only queries so the Lua surface
 * can be tested without touching the test machine.  os.restart(), os.exit(),
 * os.message() and the PRX helpers are never called by the host tests or the
 * PPSSPP demo.
 */

#include "engine/sys/os.h"
#include "engine/core/olreg.h"

#include <lauxlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/statvfs.h>
#include <time.h>

#if defined(__PSP__)
#include "engine/fs/olfs.h"

#include <pspkernel.h>
#include <psploadexec.h>
#include <psppower.h>
#include <psprtc.h>
#include <pspsysmem.h>
#include <psputility.h>
#include <pspwlan.h>
#else
#include <errno.h>

/* Fixed host mocks.  They model a PSP-like machine without touching the host
 * clock, network, power state or filesystem layout. */
#define HOST_TOTAL_RAM (32ULL * 1024ULL * 1024ULL)
#define HOST_FREE_RAM  (24ULL * 1024ULL * 1024ULL)

static int host_cpu = 333;
static int host_bus = 166;
#endif

/* OneLua caches the total at startup.  Do the same in os_api_register() so
 * subsequent Lua allocations do not make the reported total shrink. */
static lua_Number g_total_ram;

void os_format_date(int year, int month, int day,
                    int hour, int minute, int second,
                    char *out, size_t cap)
{
    char tmp[24];
    unsigned y, mo, d, h, mi, s;
    size_t n;

    if (!out || cap == 0)
        return;

    /* Clamp each field to its display width so this helper is safe with
     * garbage input and has a bounded, fixed-size output. */
    y = (unsigned)(year % 10000);
    mo = (unsigned)(month % 100);
    d = (unsigned)(day % 100);
    h = (unsigned)(hour % 100);
    mi = (unsigned)(minute % 100);
    s = (unsigned)(second % 100);
    snprintf(tmp, sizeof(tmp), "%02u/%02u/%04u @ %02u:%02u:%02u",
             d, mo, y, h, mi, s);
    n = strlen(tmp);
    if (n >= cap)
        n = cap - 1;
    memcpy(out, tmp, n);
    out[n] = '\0';
}

void os_format_mac(const unsigned char mac[6], char *out, size_t cap)
{
    if (!out || cap == 0 || !mac)
        return;
    snprintf(out, cap, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

const char *os_language_name(int language)
{
    static const char *names[] = {
        "JAPANESE", "ENGLISH", "FRENCH", "SPANISH", "GERMAN", "ITALIAN",
        "DUTCH", "PORTUGUESE", "RUSSIAN", "KOREAN", "CHINESE", "UNKNOWN"
    };

    if (language < 0 || language >= (int)(sizeof(names) / sizeof(names[0])))
        return "UNKNOWN";
    return names[language];
}

static int l_delay(lua_State *L)
{
    int ms = (int)luaL_checkinteger(L, 1);

    if (ms > 0) {
#if defined(__PSP__)
        sceKernelDelayThread((SceUInt)ms * 1000u);
#else
        struct timespec ts;
        ts.tv_sec = ms / 1000;
        ts.tv_nsec = (long)(ms % 1000) * 1000000L;
        while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
            ;
#endif
    }
    return 0;
}

static int l_getdate(lua_State *L)
{
    char buf[32];

#if defined(__PSP__)
    ScePspDateTime dt;
    if (sceRtcGetCurrentClockLocalTime(&dt) < 0)
        os_format_date(0, 0, 0, 0, 0, 0, buf, sizeof(buf));
    else
        os_format_date(dt.year, dt.month, dt.day,
                       dt.hour, dt.minute, dt.second, buf, sizeof(buf));
#else
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    if (!tm)
        os_format_date(0, 0, 0, 0, 0, 0, buf, sizeof(buf));
    else
        os_format_date(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                       tm->tm_hour, tm->tm_min, tm->tm_sec, buf, sizeof(buf));
#endif
    lua_pushstring(L, buf);
    return 1;
}

static int l_nick(lua_State *L)
{
#if defined(__PSP__)
    char buf[128];
    if (sceUtilityGetSystemParamString(PSP_SYSTEMPARAM_ID_STRING_NICKNAME,
                                       buf, sizeof(buf)) != 0)
        buf[0] = '\0';
    lua_pushstring(L, buf);
#else
    lua_pushliteral(L, "OpenLua");
#endif
    return 1;
}

static int l_mac(lua_State *L)
{
    unsigned char mac[6];
    char buf[18];

#if defined(__PSP__)
    unsigned char raw[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    if (sceWlanGetEtherAddr(raw) != 0)
        memset(raw, 0, sizeof(raw));
    memcpy(mac, raw, sizeof(mac));
#else
    static const unsigned char host_mac[6] = {
        0x02, 0x4F, 0x4C, 0x55, 0x41, 0x01
    };
    memcpy(mac, host_mac, sizeof(mac));
#endif
    os_format_mac(mac, buf, sizeof(buf));
    lua_pushstring(L, buf);
    return 1;
}

static int l_language(lua_State *L)
{
#if defined(__PSP__)
    int language = 0;
    if (sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE,
                                    &language) != 0)
        language = -1;
    lua_pushstring(L, os_language_name(language));
#else
    lua_pushliteral(L, "ENGLISH");
#endif
    return 1;
}

static int l_password(lua_State *L)
{
    (void)L;
    /* PSPSDK has no public user-mode API for the parental-control password. */
    lua_pushliteral(L, "");
    return 1;
}

static int l_ram(lua_State *L)
{
#if defined(__PSP__)
    lua_pushnumber(L, (lua_Number)sceKernelTotalFreeMemSize());
#else
    lua_pushnumber(L, (lua_Number)HOST_FREE_RAM);
#endif
    return 1;
}

static int l_totalram(lua_State *L)
{
    lua_pushnumber(L, g_total_ram);
    return 1;
}

static int l_cpu(lua_State *L)
{
    if (lua_gettop(L) == 0) {
#if defined(__PSP__)
        lua_pushinteger(L, scePowerGetCpuClockFrequency());
#else
        lua_pushinteger(L, host_cpu);
#endif
        return 1;
    }

    {
        int mhz = (int)luaL_checkinteger(L, 1);
        if (mhz >= 1 && mhz <= 333) {
#if defined(__PSP__)
            (void)scePowerSetCpuClockFrequency(mhz);
#else
            host_cpu = mhz;
#endif
        }
    }
    return 0;
}

static int l_bus(lua_State *L)
{
#if defined(__PSP__)
    lua_pushinteger(L, scePowerGetBusClockFrequency());
#else
    lua_pushinteger(L, host_bus);
#endif
    return 1;
}

static int push_fs_info(lua_State *L, const char *path)
{
    struct statvfs vfs;
    unsigned long long frsize;
    unsigned long long max;
    unsigned long long freeb;

    if (statvfs(path, &vfs) != 0) {
        lua_pushnil(L);
        return 1;
    }

    frsize = vfs.f_frsize ? (unsigned long long)vfs.f_frsize
                          : (unsigned long long)vfs.f_bsize;
    max = (unsigned long long)vfs.f_blocks * frsize;
    if (max == 0) {
        /* PPSSPP and hosts without the device can return an all-zero
         * statvfs.  Treat that as "not available" rather than reporting a
         * misleading zero-capacity filesystem. */
        lua_pushnil(L);
        return 1;
    }
    freeb = (unsigned long long)vfs.f_bavail * frsize;
    if (freeb > max)
        freeb = max;

    lua_newtable(L);
    lua_pushnumber(L, (lua_Number)max);
    lua_setfield(L, -2, "max");
    lua_pushnumber(L, (lua_Number)freeb);
    lua_setfield(L, -2, "free");
    lua_pushnumber(L, (lua_Number)(max - freeb));
    lua_setfield(L, -2, "used");
    return 1;
}

static int l_infoms0(lua_State *L)
{
#if defined(__PSP__)
    return push_fs_info(L, "ms0:/");
#else
    return push_fs_info(L, ".");
#endif
}

static int l_infoef0(lua_State *L)
{
#if defined(__PSP__)
    return push_fs_info(L, "ef0:/");
#else
    lua_pushnil(L);
    return 1;
#endif
}

static int l_cfw(lua_State *L)
{
#if defined(__PSP__)
    /* No public user-mode API distinguishes OFW from CFW reliably. */
    lua_pushliteral(L, "UNK");
#else
    lua_pushliteral(L, "HOST");
#endif
    return 1;
}

#if defined(__PSP__)
static int read_flash_version(char *out, size_t cap)
{
    FILE *f;
    size_t n;

    if (!out || cap == 0)
        return -1;
    out[0] = '\0';
    f = fopen("flash0:/etc/version.txt", "rb");
    if (!f)
        return -1;
    n = fread(out, 1, cap - 1, f);
    fclose(f);
    out[n] = '\0';
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r' ||
                     out[n - 1] == ' ' || out[n - 1] == '\t'))
        out[--n] = '\0';
    return 0;
}
#endif

static int l_versiontxt(lua_State *L)
{
#if defined(__PSP__)
    char buf[256];
    if (read_flash_version(buf, sizeof(buf)) != 0)
        buf[0] = '\0';
    lua_pushstring(L, buf);
#else
    lua_pushliteral(L, "HOST");
#endif
    return 1;
}

static int l_message(lua_State *L)
{
    (void)luaL_checkstring(L, 1);
    if (lua_gettop(L) >= 2) {
        (void)luaL_checkinteger(L, 2);
        lua_pushinteger(L, -1);
        return 1;
    }
    return 0;
}

static int l_messagebox(lua_State *L)
{
    (void)luaL_checkstring(L, 1);
    (void)luaL_checkstring(L, 2);
    lua_pushinteger(L, 0);
    return 1;
}

static int l_initprx(lua_State *L)
{
    (void)luaL_checkstring(L, 1);
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2))
        (void)luaL_checkstring(L, 2);
    lua_pushinteger(L, 0);
    return 1;
}

static int l_stopprx(lua_State *L)
{
    (void)luaL_checkinteger(L, 1);
    lua_pushboolean(L, 0);
    return 1;
}

static int l_requiere(lua_State *L)
{
    (void)luaL_checkstring(L, 1);
    lua_pushinteger(L, 0);
    return 1;
}

static int l_restart(lua_State *L)
{
    (void)L;
#if defined(__PSP__)
    {
        const char *cwd = olfs_cwd_current();
        char path[512];
        size_t n = cwd ? strlen(cwd) : 0;
        int sep = n > 0 && cwd[n - 1] != '/';

        if (n + (size_t)sep + 10 < sizeof(path)) {
            if (n > 0)
                memcpy(path, cwd, n);
            if (sep)
                path[n++] = '/';
            memcpy(path + n, "EBOOT.PBP", 10);
            (void)sceKernelLoadExec(path, NULL);
        }
    }
#endif
    return 0;
}

static const luaL_Reg os_funcs[] = {
    { "delay",      l_delay      },
    { "getdate",    l_getdate    },
    { "nick",       l_nick       },
    { "mac",        l_mac        },
    { "language",   l_language   },
    { "password",   l_password   },
    { "ram",        l_ram        },
    { "totalram",   l_totalram   },
    { "cpu",        l_cpu        },
    { "bus",        l_bus        },
    { "infoms0",    l_infoms0    },
    { "infoef0",    l_infoef0    },
    { "cfw",        l_cfw        },
    { "versiontxt", l_versiontxt },
    { "message",    l_message    },
    { "messagebox", l_messagebox },
    { "restart",    l_restart    },
    { "initprx",    l_initprx    },
    { "stopprx",    l_stopprx    },
    { "requiere",   l_requiere   },
    { NULL,         NULL         }
};

void os_api_register(lua_State *L)
{
    if (g_total_ram == 0) {
#if defined(__PSP__)
        g_total_ram = (lua_Number)sceKernelTotalFreeMemSize();
#else
        g_total_ram = (lua_Number)HOST_TOTAL_RAM;
#endif
    }

    lua_getglobal(L, "os");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
    }
    ol_setfuncs(L, os_funcs);
    lua_setglobal(L, "os");
}
