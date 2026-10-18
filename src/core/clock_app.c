#include "clock_app.h"

#include <stddef.h>

#include "clock_face.h"
#include "settings.h"

/* What is on the screen right now. */
typedef enum { NOTHING_SHOWN, UNREADABLE_SHOWN, TIME_SHOWN } Shown;

static ClockLayout load_layout(const ClockPlatform *platform) {
    char text[SETTINGS_MAX_BYTES];
    int len = platform->read_settings(platform->ctx, text, sizeof text);
    return len < 0 ? CLOCK_LAYOUT_DEFAULT : settings_parse_layout(text, (size_t)len);
}

static void save_layout(const ClockPlatform *platform, ClockLayout layout) {
    char text[SETTINGS_MAX_BYTES];
    size_t len = settings_format_layout(layout, text);
    /* The settings are a convenience: if the write fails, the clock still works. */
    (void)platform->write_settings(platform->ctx, text, len);
}

void clock_app_run(const ClockPlatform *platform, Framebuffer *fb) {
    ClockLayout layout = load_layout(platform);
    Shown shown = NOTHING_SHOWN;
    LocalTime shown_time = {0};

    while (!platform->exit_requested(platform->ctx)) {
        LocalTime now;
        int switched = platform->switch_pressed(platform->ctx);
        int readable = platform->read_time(platform->ctx, &now) >= 0;
        Shown wanted = readable ? TIME_SHOWN : UNREADABLE_SHOWN;

        if (switched) {
            layout = clock_face_next_layout(layout);
            save_layout(platform, layout);
        }

        /* Redraw only when what is displayed changes (the Clock face has no
         * seconds), and at once when the Layout switches. An unreadable
         * clock shows dashes instead of a wrong time. */
        if (switched || wanted != shown ||
            (readable && !local_time_same_minute(&now, &shown_time))) {
            clock_face_render(fb, layout, readable ? &now : NULL);
            platform->present(platform->ctx, fb);
            shown = wanted;
            if (readable) shown_time = now;
        }
        platform->keep_awake(platform->ctx);
        platform->wait(platform->ctx);
    }
}
