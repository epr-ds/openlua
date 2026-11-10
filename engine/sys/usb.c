/* OpenLua - OneLua-compatible USB mass-storage module.
 *
 * See usb.h.  The memory-stick mode uses only the standard sceUsb and
 * USBStor drivers.  The optional UMD/flash modes use pspUsbDevice, which needs
 * Sony's `_usbdevice.prx` loaded before this EBOOT starts; importing that
 * library statically makes the whole EBOOT fail with 8002013C on a normal CFW
 * boot where the module is not preloaded.  This portable build therefore keeps
 * only the standard memory-stick path and reports UMD/flash as unavailable.
 *
 * Host builds have no USB controller, so every mode is inactive and every
 * query returns false.
 */

#include "engine/sys/usb.h"
#include "engine/core/olreg.h"

#include <lauxlib.h>

#if defined(__PSP__)

#include <psptypes.h>
#include <pspusb.h>
#include <pspusbstor.h>   /* PSP_USBSTOR_DRIVERNAME only; no USBStorBoot calls */

#define USB_PID            0x282

static int g_active;

static void usb_stop_hw(void)
{
    if (g_active) {
        sceUsbDeactivate(USB_PID);
        sceUsbStop(PSP_USBSTOR_DRIVERNAME, 0, NULL);
        sceUsbStop(PSP_USBBUS_DRIVERNAME, 0, NULL);
        g_active = 0;
    }
}

/* device < 0 selects the default memory-stick view; the UMD/flash devices
 * are not available in this portable build. */
static int usb_start_hw(int device, int readonly)
{
    (void)readonly;

    usb_stop_hw();
    if (device >= 0)
        return 0;

    if (sceUsbStart(PSP_USBBUS_DRIVERNAME, 0, NULL) < 0)
        goto fail;
    if (sceUsbStart(PSP_USBSTOR_DRIVERNAME, 0, NULL) < 0) {
        sceUsbStop(PSP_USBBUS_DRIVERNAME, 0, NULL);
        goto fail;
    }

    if (sceUsbActivate(USB_PID) < 0) {
        sceUsbStop(PSP_USBSTOR_DRIVERNAME, 0, NULL);
        sceUsbStop(PSP_USBBUS_DRIVERNAME, 0, NULL);
        goto fail;
    }

    g_active = 1;
    return 1;

fail:
    return 0;
}

static int usb_connected_hw(void)
{
    int state = sceUsbGetState();
    return state >= 0 && (state & PSP_USB_CONNECTION_ESTABLISHED) != 0;
}

#else /* host: no USB controller */

static void usb_stop_hw(void) { }
static int usb_start_hw(int device, int readonly)
{
    (void)device;
    (void)readonly;
    return 0;
}
static int usb_connected_hw(void) { return 0; }

#endif

static int l_mstick(lua_State *L)
{
    (void)L;
    usb_start_hw(-1, 0);
    return 0;
}

static int l_umd(lua_State *L)
{
    (void)L;
    /* UMD mode needs _usbdevice.prx; not available in the portable build. */
    return 0;
}

static int l_flash(lua_State *L, int device)
{
    (void)L;
    (void)device;
    /* Flash modes need _usbdevice.prx; not available in the portable build. */
    return 0;
}

static int l_flash0(lua_State *L)
{
    return l_flash(L, 0);
}

static int l_flash1(lua_State *L)
{
    return l_flash(L, 1);
}

static int l_flash2(lua_State *L)
{
    return l_flash(L, 2);
}

static int l_flash3(lua_State *L)
{
    return l_flash(L, 3);
}

static int l_stop(lua_State *L)
{
    (void)L;
    usb_stop_hw();
    return 0;
}

static int l_isconnected(lua_State *L)
{
    lua_pushboolean(L, usb_connected_hw());
    return 1;
}

static int l_isactive(lua_State *L)
{
#if defined(__PSP__)
    lua_pushboolean(L, g_active != 0);
#else
    lua_pushboolean(L, 0);
#endif
    return 1;
}

static const luaL_Reg usb_funcs[] = {
    { "mstick",     l_mstick     },
    { "umd",        l_umd        },
    { "flash0",     l_flash0     },
    { "flash1",     l_flash1     },
    { "flash2",     l_flash2     },
    { "flash3",     l_flash3     },
    { "stop",       l_stop       },
    { "isconnected", l_isconnected },
    { "isactive",   l_isactive   },
    { NULL, NULL }
};

void usb_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, usb_funcs);
    lua_setglobal(L, "usb");
}
