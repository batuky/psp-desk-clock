/* PSP layer: the only code that touches the hardware. Everything it draws
 * comes from the pure core in src/core (ADR 0001); it stays in user mode
 * (ADR 0002). */
#include <pspdisplay.h>
#include <pspge.h>
#include <pspkernel.h>
#include <psppower.h>
#include <psprtc.h>
#include <string.h>

#include "time_display.h"

PSP_MODULE_INFO("psp-desk-clock", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);

#define VRAM_STRIDE 512      /* pixels per VRAM row, fixed by the hardware */
#define POLL_USEC (100 * 1000)

/* 33 MHz is plenty for a clock and keeps the draw from USB power low.
 * Constraints: cpu <= pll, bus * 2 <= pll. */
#define CLOCK_PLL 33
#define CLOCK_CPU 33
#define CLOCK_BUS 16

static Framebuffer frame;

static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1;
    (void)arg2;
    (void)common;
    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    int id = sceKernelCreateCallback("exit callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(id);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_exit_callback(void) {
    int thread = sceKernelCreateThread("exit thread", callback_thread, 0x11, 0xFA0, 0, NULL);
    if (thread >= 0) sceKernelStartThread(thread, 0, NULL);
}

static void present(void *vram) {
    for (int y = 0; y < FB_HEIGHT; y++)
        memcpy((Pixel *)vram + y * VRAM_STRIDE, &frame.pixels[y * FB_WIDTH],
               FB_WIDTH * sizeof(Pixel));
}

int main(void) {
    void *vram = (void *)(0x40000000u | (unsigned int)sceGeEdramGetAddr());
    int last_second = -1;

    setup_exit_callback();
    scePowerSetClockFrequency(CLOCK_PLL, CLOCK_CPU, CLOCK_BUS);
    sceDisplaySetMode(0, FB_WIDTH, FB_HEIGHT);
    sceDisplaySetFrameBuf(vram, VRAM_STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888,
                          PSP_DISPLAY_SETBUF_NEXTFRAME);

    for (;;) {
        ScePspDateTime now;
        /* Draw only when the second changes. If the RTC can't be read, show
         * dashes instead of a wrong time. */
        if (sceRtcGetCurrentClockLocalTime(&now) < 0) now.second = 0xFFFF;
        if ((int)now.second != last_second) {
            last_second = now.second;
            if (now.second == 0xFFFF)
                time_display_render(&frame, -1, -1, -1);
            else
                time_display_render(&frame, now.hour, now.minute, now.second);
            present(vram);
        }
        scePowerTick(PSP_POWER_TICK_ALL);
        sceKernelDelayThread(POLL_USEC);
    }
    return 0;
}
