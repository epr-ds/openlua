/* Minimal PSP homebrew — verifies the EBOOT boot path (no Lua, no engine).
 * If you see the "Mini Test" text, the bootstrap / stack / heap setup works
 * and any black-screen in the full OpenLua build is in engine init (GE init,
 * Lua heap, thread setup). Press START to quit. */

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>

PSP_MODULE_INFO("MinTest", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(4096);

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    pspDebugScreenInit();
    pspDebugScreenSetTextColor(0xFFFFFFFF);
    pspDebugScreenSetXY(2, 2);
    pspDebugScreenPrintf("OpenLua Mini Test\n");
    pspDebugScreenPrintf("=================\n");
    pspDebugScreenPrintf("If you see this, the\nEBOOT bootstrap works!\n");
    pspDebugScreenPrintf("Any black screen in the\nfull OpenLua build is in\nengine init (GE init / heap /\nLua state), not the kernel.\n");
    pspDebugScreenPrintf("Press START to quit.\n");

    while (1) {
        sceDisplayWaitVblankStart();
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);
        if (pad.Buttons & PSP_CTRL_START) break;
    }

    return 0;
}
