/* Minimal standalone PSP boot test - no engine code, no Lua. */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspgu.h>

PSP_MODULE_INFO("HelloPSP", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

static unsigned int __attribute__((aligned(16))) gu_list[0x40000 / 4];

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

int setup_callbacks(void)
{
    int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0)
        sceKernelStartThread(thid, 0, NULL);
    return thid;
}

void *vram_alloc(int size)
{
    static int head = 0;
    void *p = (void *)(0x04000000 + head);
    head += size;
    return p;
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    setup_callbacks();

    pspDebugScreenInit();
    pspDebugScreenPrintf("hello: before GU init\n");

    void *fb0 = vram_alloc(512 * 272 * 4);
    void *fb1 = vram_alloc(512 * 272 * 4);

    sceGuInit();
    sceGuStart(GU_DIRECT, gu_list);
    sceGuDrawBuffer(GU_PSM_8888, fb0, 512);
    sceGuDispBuffer(480, 272, fb1, 512);
    sceGuOffset(2048 - (480 / 2), 2048 - (272 / 2));
    sceGuViewport(2048, 2048, 480, 272);
    sceGuScissor(0, 0, 480, 272);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
    pspDebugScreenPrintf("hello: GU init ok\n");

    while (1) {
        sceGuStart(GU_DIRECT, gu_list);
        sceGuClearColor(0xFF0000FF); /* blue (ABGR) */
        sceGuClear(GU_COLOR_BUFFER_BIT);
        sceGuFinish();
        sceGuSync(0, 0);
        sceGuSwapBuffers();
    }
    return 0;
}
