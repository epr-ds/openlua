/*
 * OpenLua boot probe - diagnose 8002013C on a real PSP.
 *
 * This is a deliberately tiny user-mode EBOOT.  It imports only the core
 * display/input/io libraries, so it should boot on any CFW that can run
 * homebrew at all.  It then asks the firmware to load the optional modules
 * that OpenLua uses and reports the result on screen and in a text file.
 *
 * Nothing is written to flash.  Modules are unloaded again when possible.
 */

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspiofilemgr.h>
#include <psputility.h>
#include <psputility_modules.h>
#include <psputility_netmodules.h>
#include <psputility_usbmodules.h>
#include <psputility_avmodules.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

PSP_MODULE_INFO("OpenLua Boot Probe", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(1024);

#define LOG_CAP 8192

static char g_log[LOG_CAP];
static int g_len;

static void plog(const char *fmt, ...)
{
    char line[256];
    va_list ap;
    int n;

    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);

    pspDebugScreenPrintf("%s", line);

    n = (int)strlen(line);
    if (g_len + n < LOG_CAP - 1) {
        memcpy(g_log + g_len, line, (size_t)n);
        g_len += n;
        g_log[g_len] = '\0';
    }
}

static void save_log(void)
{
    static const char *paths[] = {
        "openlua_probe.txt",
        "ms0:/openlua_probe.txt",
        "ef0:/openlua_probe.txt",
        NULL
    };
    int i;

    for (i = 0; paths[i]; i++) {
        SceUID fd = sceIoOpen(paths[i], PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
        if (fd >= 0) {
            sceIoWrite(fd, g_log, g_len);
            sceIoClose(fd);
        }
    }
}

static int result_ok(int rc)
{
    /* Some firmware reports "already loaded" for a module that is in use.
     * That still proves the module exists. */
    return rc >= 0 || (unsigned int)rc == 0x80111102u;
}

static void report(const char *name, int rc)
{
    plog("%-22s %08X %s\n", name, (unsigned int)rc,
         result_ok(rc) ? "OK" : "MISSING/ERROR");
    save_log();
}

static void test_net(void)
{
    static const struct { const char *name; int id; } mods[] = {
        { "net common", PSP_NET_MODULE_COMMON },
        { "net inet",   PSP_NET_MODULE_INET   },
        { "net adhoc",  PSP_NET_MODULE_ADHOC  },
    };
    unsigned int i;

    for (i = 0; i < sizeof(mods) / sizeof(mods[0]); i++) {
        int rc = sceUtilityLoadNetModule(mods[i].id);
        report(mods[i].name, rc);
    }
    for (i = sizeof(mods) / sizeof(mods[0]); i-- > 0;) {
        int rc = sceUtilityLoadNetModule(mods[i].id);
        (void)rc;
    }
}

static void test_av(void)
{
    int rc;

    rc = sceUtilityLoadAvModule(PSP_AV_MODULE_AVCODEC);
    report("av avcodec", rc);
    rc = sceUtilityLoadAvModule(PSP_AV_MODULE_MPEGBASE);
    report("av mpegbase", rc);

    rc = sceUtilityUnloadAvModule(PSP_AV_MODULE_MPEGBASE);
    (void)rc;
    rc = sceUtilityUnloadAvModule(PSP_AV_MODULE_AVCODEC);
    (void)rc;
}

static void test_usb(void)
{
    int rc;

    rc = sceUtilityLoadUsbModule(PSP_USB_MODULE_ACC);
    report("usb acc", rc);
    rc = sceUtilityLoadUsbModule(PSP_USB_MODULE_CAM);
    report("usb cam", rc);

    rc = sceUtilityUnloadUsbModule(PSP_USB_MODULE_CAM);
    (void)rc;
    rc = sceUtilityUnloadUsbModule(PSP_USB_MODULE_ACC);
    (void)rc;
}

static void test_irda(void)
{
    int rc = sceUtilityLoadModule(PSP_MODULE_IRDA);
    report("irda", rc);
    rc = sceUtilityUnloadModule(PSP_MODULE_IRDA);
    (void)rc;
}

static void wait_for_exit(void)
{
    SceCtrlData pad;
    unsigned int i;
    int done = 0;

    plog("\nPress any button to exit (auto in 30s).\n");
    save_log();

    for (i = 0; i < 600 && !done; i++) {
        memset(&pad, 0, sizeof(pad));
        if (sceCtrlReadBufferPositive(&pad, 1) > 0 && pad.Buttons != 0)
            done = 1;
        sceKernelDelayThread(50 * 1000);
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    pspDebugScreenInit();
    pspDebugScreenClear();
    pspDebugScreenSetXY(0, 0);

    plog("OpenLua boot probe\n");
    plog("firmware %08X\n", (unsigned int)sceKernelDevkitVersion());
    plog("module load results:\n");
    save_log();

    test_net();
    test_av();
    test_usb();
    test_irda();

    plog("\nIf this screen is visible, basic homebrew\n");
    plog("execution works on this console.\n");
    plog("Report openlua_probe.txt back to OpenLua.\n");
    save_log();

    wait_for_exit();
    sceKernelExitGame();
    return 0;
}
