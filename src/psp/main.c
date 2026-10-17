/* PSP layer: the only code that touches the hardware. It fills in the
 * ClockPlatform that the pure core's main loop runs against (ADR 0001) and
 * stays in user mode (ADR 0002). */
#include <pspdisplay.h>
#include <pspge.h>
#include <pspkernel.h>
#include <psppower.h>
#include <psprtc.h>
#include <string.h>

#include "clock_app.h"

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

/* HOME: the exit callback ends the program itself, so the loop never has to
 * ask. */
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

static int psp_read_time(void *ctx, LocalTime *now) {
    ScePspDateTime rtc;
    (void)ctx;
    if (sceRtcGetCurrentClockLocalTime(&rtc) < 0) return -1;
    now->year = rtc.year;
    now->month = rtc.month;
    now->day = rtc.day;
    now->hour = rtc.hour;
    now->minute = rtc.minute;
    now->second = rtc.second;
    return 0;
}

static void psp_present(void *ctx, const Framebuffer *fb) {
    Pixel *vram = ctx;
    for (int y = 0; y < FB_HEIGHT; y++)
        memcpy(vram + y * VRAM_STRIDE, &fb->pixels[y * FB_WIDTH], FB_WIDTH * sizeof(Pixel));
}

static void psp_keep_awake(void *ctx) {
    (void)ctx;
    scePowerTick(PSP_POWER_TICK_ALL);
}

static void psp_wait(void *ctx) {
    (void)ctx;
    sceKernelDelayThread(POLL_USEC);
}

static int psp_exit_requested(void *ctx) {
    (void)ctx;
    return 0;
}

int main(void) {
    Pixel *vram = (Pixel *)(0x40000000u | (unsigned int)sceGeEdramGetAddr());
    ClockPlatform platform = {psp_read_time, psp_present, psp_keep_awake,
                              psp_wait, psp_exit_requested, vram};

    setup_exit_callback();
    scePowerSetClockFrequency(CLOCK_PLL, CLOCK_CPU, CLOCK_BUS);
    sceDisplaySetMode(0, FB_WIDTH, FB_HEIGHT);
    sceDisplaySetFrameBuf(vram, VRAM_STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888,
                          PSP_DISPLAY_SETBUF_NEXTFRAME);

    clock_app_run(&platform, &frame);
    return 0;
}
