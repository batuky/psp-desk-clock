#include "clock_app.h"

#include "time_display.h"

#define NOT_SHOWN (-2)
#define UNREADABLE (-1)

void clock_app_run(const ClockPlatform *platform, Framebuffer *fb) {
    int shown = NOT_SHOWN;
    while (!platform->exit_requested(platform->ctx)) {
        int hour, minute, second, now;
        if (platform->read_time(platform->ctx, &hour, &minute, &second) < 0) {
            /* Show dashes instead of a wrong time. */
            hour = minute = second = -1;
            now = UNREADABLE;
        } else {
            now = hour * 3600 + minute * 60 + second;
        }
        /* Redraw only when what is displayed changes. */
        if (now != shown) {
            time_display_render(fb, hour, minute, second);
            platform->present(platform->ctx, fb);
            shown = now;
        }
        platform->keep_awake(platform->ctx);
        platform->wait(platform->ctx);
    }
}
