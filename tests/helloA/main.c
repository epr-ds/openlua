/* Variant A: debug screen text only, no GU at all. */

#include <pspkernel.h>
#include <pspdebug.h>

PSP_MODULE_INFO("HelloA", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

int exit_callback(int arg1, int arg2, void *common)
{
    (void)arg1; (void)arg2; (void)common;
    sceKernelExitGame();
    return 0;
}

int callback_thread(SceSize args, void *argp)
{
    (void)args; (void)argp;
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    int thid = sceKernelCreateThread("cb", callback_thread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0)
        sceKernelStartThread(thid, 0, NULL);

    pspDebugScreenInit();
    pspDebugScreenPrintf("VARIANT A: main thread reached\n");
    while (1) {
        sceKernelDelayThread(1000);
    }
    return 0;
}
