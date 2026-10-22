/* OpenLua - OneLua battery telemetry module
 *
 * The read-only calls are backed by the documented PSPSDK scePower APIs:
 *
 *   batt.exists()       scePowerIsBatteryExist()
 *   batt.charging()     scePowerIsBatteryCharging()
 *   batt.lifepercent()  scePowerGetBatteryLifePercent()
 *   batt.lifetimemin()  scePowerGetBatteryLifeTime()
 *   batt.temp()         scePowerGetBatteryTemp()
 *   batt.volt()         scePowerGetBatteryVolt()
 *   batt.remaincap()    scePowerGetBatteryRemainCapacity()
 *   batt.fullcap()      scePowerGetBatteryFullCapacity()
 *   batt.low()          scePowerIsLowBattery()
 *
 * PSPSDK has no safe user-mode API for the battery serial number, battery
 * mode, mode changes, or EEPROM/NAND backup.  Those entry points are present
 * for script compatibility but are deliberately inert: batt.serial() and
 * batt.mode() return "-", and every mutating call returns 0 without touching
 * hardware.  This is preferable to guessing at syscon commands that could
 * brick a battery or the console.
 *
 * The host build uses fixed mock values so the Lua module can be tested with
 * the normal host harness.  Formatting is done by batt_format_* helpers,
 * which are covered separately by test/host/test_batt.c.
 */

#include "engine/sys/battery.h"
#include "engine/core/olreg.h"

#include <lauxlib.h>
#include <stdio.h>

#if defined(__PSP__)
#include <psppower.h>
#else
/* Host mocks.  These mirror a healthy battery so test_batt.lua can assert
 * the public Lua surface without a PSP. */
static int scePowerIsBatteryExist(void)        { return 1; }
static int scePowerIsBatteryCharging(void)     { return 0; }
static int scePowerGetBatteryLifePercent(void) { return 76; }
static int scePowerGetBatteryLifeTime(void)    { return 95; }
static int scePowerGetBatteryTemp(void)        { return 31; }
static int scePowerGetBatteryVolt(void)        { return 4020; }
static int scePowerGetBatteryRemainCapacity(void) { return 850; }
static int scePowerGetBatteryFullCapacity(void)   { return 1200; }
static int scePowerIsLowBattery(void)          { return 0; }
#endif

void batt_format_lifetime(int minutes, char *out, size_t cap)
{
    if (!out || cap == 0)
        return;
    if (minutes < 0) {
        snprintf(out, cap, "-");
        return;
    }

    int hours = minutes / 60;
    int mins  = minutes % 60;
    snprintf(out, cap, "%d:%02d", hours, mins);
}

void batt_format_volt(int millivolts, char *out, size_t cap)
{
    if (!out || cap == 0)
        return;
    if (millivolts <= 0) {
        snprintf(out, cap, "-");
        return;
    }

    /* scePowerGetBatteryVolt() reports millivolts.  Format as volts without
     * depending on floating-point printf support in the PSP runtime. */
    int whole = millivolts / 1000;
    int frac  = (millivolts % 1000) / 10;
    snprintf(out, cap, "%d.%02d", whole, frac);
}

void batt_format_capacity(int milliamps, char *out, size_t cap)
{
    if (!out || cap == 0)
        return;
    if (milliamps <= 0) {
        snprintf(out, cap, "-");
        return;
    }

    snprintf(out, cap, "%d", milliamps);
}

static int l_exists(lua_State *L)
{
    lua_pushboolean(L, scePowerIsBatteryExist() > 0);
    return 1;
}

static int l_charging(lua_State *L)
{
    lua_pushboolean(L, scePowerIsBatteryCharging() > 0);
    return 1;
}

static int l_lifepercent(lua_State *L)
{
    lua_pushinteger(L, (lua_Integer)scePowerGetBatteryLifePercent());
    return 1;
}

static int l_lifetimemin(lua_State *L)
{
    lua_pushinteger(L, (lua_Integer)scePowerGetBatteryLifeTime());
    return 1;
}

static int l_lifetime(lua_State *L)
{
    char buf[32];
    batt_format_lifetime(scePowerGetBatteryLifeTime(), buf, sizeof(buf));
    lua_pushstring(L, buf);
    return 1;
}

static int l_temp(lua_State *L)
{
    lua_pushinteger(L, (lua_Integer)scePowerGetBatteryTemp());
    return 1;
}

static int l_volt(lua_State *L)
{
    char buf[32];
    batt_format_volt(scePowerGetBatteryVolt(), buf, sizeof(buf));
    lua_pushstring(L, buf);
    return 1;
}

static int l_remaincap(lua_State *L)
{
    char buf[32];
    batt_format_capacity(scePowerGetBatteryRemainCapacity(), buf, sizeof(buf));
    lua_pushstring(L, buf);
    return 1;
}

static int l_fullcap(lua_State *L)
{
    char buf[32];
    batt_format_capacity(scePowerGetBatteryFullCapacity(), buf, sizeof(buf));
    lua_pushstring(L, buf);
    return 1;
}

static int l_low(lua_State *L)
{
    lua_pushboolean(L, scePowerIsLowBattery() > 0);
    return 1;
}

/* No safe user-mode API is available for these values. */
static int l_serial(lua_State *L)
{
    lua_pushliteral(L, "-");
    return 1;
}

static int l_mode(lua_State *L)
{
    lua_pushliteral(L, "-");
    return 1;
}

/* Mode changes and EEPROM access are intentionally not implemented.  They
 * still accept the OneLua signatures and report failure (0) so scripts can
 * detect the lack of support without any risk to the battery. */
static int l_unsupported(lua_State *L)
{
    (void)L;
    lua_pushinteger(L, 0);
    return 1;
}

static int l_backupbat(lua_State *L)
{
    (void)luaL_checkstring(L, 1);   /* validate signature, never open/read it */
    lua_pushinteger(L, 0);
    return 1;
}

static int l_restorebat(lua_State *L)
{
    (void)luaL_checkstring(L, 1);   /* validate signature, never open/write it */
    lua_pushinteger(L, 0);
    return 1;
}

static const luaL_Reg batt_funcs[] = {
    { "exists",      l_exists      },
    { "charging",    l_charging    },
    { "lifepercent", l_lifepercent },
    { "lifetimemin", l_lifetimemin },
    { "lifetime",    l_lifetime    },
    { "temp",        l_temp        },
    { "volt",        l_volt        },
    { "remaincap",   l_remaincap   },
    { "fullcap",     l_fullcap     },
    { "low",         l_low         },
    { "serial",      l_serial      },
    { "mode",        l_mode        },
    { "tonormal",    l_unsupported },
    { "topandora",   l_unsupported },
    { "toautoboot",  l_unsupported },
    { "backupbat",   l_backupbat   },
    { "restorebat",  l_restorebat  },
    { NULL, NULL }
};

void battery_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, batt_funcs);
    lua_setglobal(L, "batt");
}
