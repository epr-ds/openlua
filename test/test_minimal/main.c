/* Minimal PSP homebrew — verifies the EBOOT boot path (no Lua, no engine).
 * If the "Mini Test" text appears, module setup / entry point works and any
 * black-screen in the full OpenLua build is in engine init (heap, threads,
 * or GE setup). Press START to quit. */

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspgu.h>

PSP_MODULE_INFO("MinTest", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(10240);

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    pspDebugScreenInit();
    pspDebugScreenClear();
    pspDebugScreenSetXY(2, 4);
    pspDebugScreenPrintf("OpenLua Mini Test\n");
    pspDebugScreenPrintf("=================\n");
    pspDebugScreenPrintf("If you see this, the\n");
    pspDebugScreenPrintf("EBOOT bootstrap works.\n");
    pspDebugScreenPrintf("Press START to quit.\n");

    while (1) {
        sceDisplayWaitVblankStart();
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);
        if (pad.Buttons & PSP_CTRL_START)
            break;
    }
    return 0;
}
