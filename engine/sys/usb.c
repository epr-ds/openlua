/* OpenLua - OneLua-compatible USB mass-storage module.
 *
 * See usb.h.  The memory-stick mode uses only the standard sceUsb and
 * USBStor drivers.  UMD/flash modes additionally select a device through
 * pspUsbDeviceSetDevice(), which needs Sony's optional _usbdevice.prx; a
 * failure there is reported through isactive() == false rather than raising.
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
#include <pspusbdevice.h>
#include <pspusbstor.h>

#define USB_PID            0x282
#define USB_STOR_CAPACITY  0x800000

static int g_active;
static int g_device_set;

static void usb_stop_hw(void)
{
    if (g_active) {
        sceUsbDeactivate(USB_PID);
        sceUsbStop(PSP_USBSTOR_DRIVERNAME, 0, NULL);
        sceUsbStop(PSP_USBBUS_DRIVERNAME, 0, NULL);
        g_active = 0;
    }
    if (g_device_set) {
        pspUsbDeviceFinishDevice();
        g_device_set = 0;
    }
}

/* device < 0 selects the default memory-stick view. */
static int usb_start_hw(int device, int readonly)
{
    usb_stop_hw();

    if (device >= 0) {
        if (pspUsbDeviceSetDevice((unsigned int)device, readonly, 0) < 0)
            return 0;
        g_device_set = 1;
    }

    if (sceUsbStart(PSP_USBBUS_DRIVERNAME, 0, NULL) < 0)
        goto fail;
    if (sceUsbStart(PSP_USBSTOR_DRIVERNAME, 0, NULL) < 0) {
        sceUsbStop(PSP_USBBUS_DRIVERNAME, 0, NULL);
        goto fail;
    }

    /* Advisory capacity hint used by the boot-capable storage driver. */
    sceUsbstorBootSetCapacity(USB_STOR_CAPACITY);

    if (sceUsbActivate(USB_PID) < 0) {
        sceUsbStop(PSP_USBSTOR_DRIVERNAME, 0, NULL);
        sceUsbStop(PSP_USBBUS_DRIVERNAME, 0, NULL);
        goto fail;
    }

    g_active = 1;
    return 1;

fail:
    if (g_device_set) {
        pspUsbDeviceFinishDevice();
        g_device_set = 0;
    }
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
#if defined(__PSP__)
    usb_start_hw(PSP_USBDEVICE_UMD9660, 0);
#endif
    return 0;
}

static int l_flash(lua_State *L, int device)
{
    (void)L;
#if defined(__PSP__)
    usb_start_hw(device, 1);   /* read-only: never allow flash writes */
#else
    (void)device;
#endif
    return 0;
}

static int l_flash0(lua_State *L)
{
#if defined(__PSP__)
    return l_flash(L, PSP_USBDEVICE_FLASH0);
#else
    return l_flash(L, 0);
#endif
}

static int l_flash1(lua_State *L)
{
#if defined(__PSP__)
    return l_flash(L, PSP_USBDEVICE_FLASH1);
#else
    return l_flash(L, 1);
#endif
}

static int l_flash2(lua_State *L)
{
#if defined(__PSP__)
    return l_flash(L, PSP_USBDEVICE_FLASH2);
#else
    return l_flash(L, 2);
#endif
}

static int l_flash3(lua_State *L)
{
#if defined(__PSP__)
    return l_flash(L, PSP_USBDEVICE_FLASH3);
#else
    return l_flash(L, 3);
#endif
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
