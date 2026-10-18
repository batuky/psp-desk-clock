/* The clock's main loop, free of any hardware: everything it needs from the
 * outside world comes through ClockPlatform. The PSP layer fills it in with
 * real hardware calls; tests fill it in with a fake. */
#ifndef CLOCK_APP_H
#define CLOCK_APP_H

#include "clock_face.h"
#include "framebuffer.h"
#include "local_time.h"

typedef struct {
    /* Reads Local time. Returns 0 on success, negative when the time can't
     * be read (and `now` is then left alone). */
    int (*read_time)(void *ctx, LocalTime *now);
    /* Shows a finished frame on the screen. */
    void (*present)(void *ctx, const Framebuffer *fb);
    /* Stops the PSP from dimming the screen or going to sleep. */
    void (*keep_awake)(void *ctx);
    /* Blocks for one poll interval. */
    void (*wait)(void *ctx);
    /* Returns non-zero once the user asked to quit (HOME). */
    int (*exit_requested)(void *ctx);
    /* Returns non-zero when the user pressed the Layout switch (L or R) since
     * the last call: once per press, not while the button is held. */
    int (*switch_pressed)(void *ctx);
    void *ctx;
} ClockPlatform;

/* Runs until exit_requested says so, showing the Clock face starting in
 * `layout`; each press of the Layout switch moves on to the next one. `fb` is
 * scratch space the loop renders into before each present. */
void clock_app_run(const ClockPlatform *platform, Framebuffer *fb, ClockLayout layout);

#endif
