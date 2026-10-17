#include "clock_app.h"

#include <stddef.h>

#include "clock_face.h"

/* What is on the screen right now. */
typedef enum { NOTHING_SHOWN, UNREADABLE_SHOWN, TIME_SHOWN } Shown;

void clock_app_run(const ClockPlatform *platform, Framebuffer *fb, ClockLayout layout) {
    Shown shown = NOTHING_SHOWN;
    LocalTime shown_time = {0};

    while (!platform->exit_requested(platform->ctx)) {
        LocalTime now;
        int readable = platform->read_time(platform->ctx, &now) >= 0;
        Shown wanted = readable ? TIME_SHOWN : UNREADABLE_SHOWN;

        /* Redraw only when what is displayed changes (the Clock face has no
         * seconds). An unreadable clock shows dashes instead of a wrong
         * time. */
        if (wanted != shown || (readable && !local_time_same_minute(&now, &shown_time))) {
            clock_face_render(fb, layout, readable ? &now : NULL);
            platform->present(platform->ctx, fb);
            shown = wanted;
            if (readable) shown_time = now;
        }
        platform->keep_awake(platform->ctx);
        platform->wait(platform->ctx);
    }
}
