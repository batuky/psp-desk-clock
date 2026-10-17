#include "clock_app.h"

#include <stddef.h>

#include "time_display.h"

/* What is on the screen right now. */
typedef enum { NOTHING_SHOWN, UNREADABLE_SHOWN, TIME_SHOWN } Shown;

void clock_app_run(const ClockPlatform *platform, Framebuffer *fb) {
    Shown shown = NOTHING_SHOWN;
    LocalTime shown_time = {0};

    while (!platform->exit_requested(platform->ctx)) {
        LocalTime now;
        int readable = platform->read_time(platform->ctx, &now) >= 0;
        Shown wanted = readable ? TIME_SHOWN : UNREADABLE_SHOWN;

        /* Redraw only when what is displayed changes. An unreadable clock
         * shows dashes instead of a wrong time. */
        if (wanted != shown || (readable && !local_time_equal(&now, &shown_time))) {
            time_display_render(fb, readable ? &now : NULL);
            platform->present(platform->ctx, fb);
            shown = wanted;
            if (readable) shown_time = now;
        }
        platform->keep_awake(platform->ctx);
        platform->wait(platform->ctx);
    }
}
