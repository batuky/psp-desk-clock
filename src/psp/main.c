/* PSP layer: the only code that touches the hardware. It fills in the
 * ClockPlatform that the pure core's main loop runs against (ADR 0001) and
 * stays in user mode (ADR 0002). */
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspkernel.h>
#include <psppower.h>
#include <psprtc.h>
#include <string.h>

#include "button_edge.h"
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
static ButtonEdge switch_edge;

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

/* L or R is the Layout switch; a held button counts once, and L and R held
 * together count as one button.
 *
 * The pad is sampled every vblank (about 17 ms) but polled every POLL_USEC, so
 * a short tap could fall between two polls. Reading the samples of the whole
 * poll interval and merging them catches it. */
#define PAD_SAMPLES (POLL_USEC / 16000 + 1)

static int psp_switch_pressed(void *ctx) {
    SceCtrlData pad[PAD_SAMPLES];
    unsigned int buttons = 0;
    int count = sceCtrlPeekBufferPositive(pad, PAD_SAMPLES);
    int down;
    (void)ctx;
    if (count > PAD_SAMPLES) count = PAD_SAMPLES;
    for (int i = 0; i < count; i++) buttons |= pad[i].Buttons;
    down = (buttons & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) != 0;
    return button_edge_pressed(&switch_edge, down);
}

int main(void) {
    Pixel *vram = (Pixel *)(0x40000000u | (unsigned int)sceGeEdramGetAddr());
    ClockPlatform platform = {psp_read_time, psp_present, psp_keep_awake,
                              psp_wait, psp_exit_requested, psp_switch_pressed, vram};

    setup_exit_callback();
    /* Buttons only, sampled every vblank, for the Layout switch. */
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    /* A trigger already held at launch is not a press: note it, drop the answer. */
    (void)psp_switch_pressed(NULL);
    scePowerSetClockFrequency(CLOCK_PLL, CLOCK_CPU, CLOCK_BUS);
    sceDisplaySetMode(0, FB_WIDTH, FB_HEIGHT);
    sceDisplaySetFrameBuf(vram, VRAM_STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888,
                          PSP_DISPLAY_SETBUF_NEXTFRAME);

    clock_app_run(&platform, &frame, CLOCK_LAYOUT_D);
    return 0;
}
