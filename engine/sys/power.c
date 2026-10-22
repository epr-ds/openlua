/* OpenLua - OneLua power management module
 *
 * The documented OneLua calls map to the public PSPSDK power API:
 *
 *   power.plugged()  scePowerIsPowerOnline()
 *   power.suspend()  scePowerRequestSuspend()
 *   power.lock()     scePowerLock(0)
 *   power.unlock()   scePowerUnlock(0)
 *   power.tick()     scePowerTick(PSP_POWER_TICK_ALL)
 *   power.off()      scePowerRequestStandby()
 *
 * PSPSDK has no user-mode software power-off call.  power.off() therefore
 * requests standby, the closest safe public operation; a true shutdown still
 * needs the PSP's hardware power switch.  The suspend/standby requests are
 * deliberately absent from the PPSSPP demo; host builds use no-op stubs.
 */

#include "engine/sys/power.h"
#include "engine/core/olreg.h"

#include <lauxlib.h>

#if defined(__PSP__)
#include <psppower.h>
#else
/* Host mocks.  They only model a powered, unlocked console so the Lua module
 * can be registered and its safe calls exercised without a PSP. */
#define PSP_POWER_TICK_ALL 0

static int scePowerIsPowerOnline(void) { return 1; }
static int scePowerLock(int unknown)   { (void)unknown; return 0; }
static int scePowerUnlock(int unknown) { (void)unknown; return 0; }
static int scePowerTick(int type)      { (void)type; return 0; }
static int scePowerRequestSuspend(void) { return 0; }
static int scePowerRequestStandby(void) { return 0; }
#endif

static int l_plugged(lua_State *L)
{
    lua_pushboolean(L, scePowerIsPowerOnline() > 0);
    return 1;
}

static int l_suspend(lua_State *L)
{
    (void)L;
    (void)scePowerRequestSuspend();
    return 0;
}

static int l_lock(lua_State *L)
{
    lua_pushboolean(L, scePowerLock(0) == 0);
    return 1;
}

static int l_unlock(lua_State *L)
{
    lua_pushboolean(L, scePowerUnlock(0) == 0);
    return 1;
}

static int l_tick(lua_State *L)
{
    (void)L;
    (void)scePowerTick(PSP_POWER_TICK_ALL);
    return 0;
}

static int l_off(lua_State *L)
{
    (void)L;
    (void)scePowerRequestStandby();
    return 0;
}

static const luaL_Reg power_funcs[] = {
    { "plugged", l_plugged },
    { "suspend", l_suspend },
    { "lock",    l_lock    },
    { "unlock",  l_unlock  },
    { "tick",    l_tick    },
    { "off",     l_off     },
    { NULL, NULL }
};

void power_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, power_funcs);
    lua_setglobal(L, "power");
}
