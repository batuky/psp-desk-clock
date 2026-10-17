#include <string.h>

#include "clock_app.h"
#include "minitest.h"
#include "time_display.h"

/* A scripted stand-in for the hardware. Each read_time call returns the next
 * scripted reading (the last one repeats). The loop quits after
 * `polls_before_exit` waits. */
typedef struct {
    int hour, minute, second;
    int readable;
} Reading;

typedef struct {
    const Reading *script;
    int script_len;
    int polls_before_exit;
    int reads, waits, awake_calls, presents;
    Framebuffer last_frame;
} Fake;

static int fake_read_time(void *ctx, int *h, int *m, int *s) {
    Fake *f = ctx;
    const Reading *r = &f->script[f->reads < f->script_len ? f->reads : f->script_len - 1];
    f->reads++;
    if (!r->readable) return -1;
    *h = r->hour;
    *m = r->minute;
    *s = r->second;
    return 0;
}
static void fake_present(void *ctx, const Framebuffer *fb) {
    Fake *f = ctx;
    f->presents++;
    f->last_frame = *fb;
}
static void fake_keep_awake(void *ctx) { ((Fake *)ctx)->awake_calls++; }
static void fake_wait(void *ctx) { ((Fake *)ctx)->waits++; }
static int fake_exit_requested(void *ctx) {
    Fake *f = ctx;
    return f->waits >= f->polls_before_exit;
}

static Fake fake;
static Framebuffer scratch;
static Framebuffer expected;

static void run(const Reading *script, int len, int polls) {
    ClockPlatform platform = {fake_read_time, fake_present, fake_keep_awake,
                              fake_wait, fake_exit_requested, &fake};
    memset(&fake, 0, sizeof fake);
    fake.script = script;
    fake.script_len = len;
    fake.polls_before_exit = polls;
    clock_app_run(&platform, &scratch);
}

static int last_frame_shows(int h, int m, int s) {
    time_display_render(&expected, h, m, s);
    return memcmp(&fake.last_frame, &expected, sizeof expected) == 0;
}

static void first_frame_is_presented_immediately(void) {
    static const Reading script[] = {{12, 34, 56, 1}};
    run(script, 1, 1);
    CHECK(fake.presents == 1);
    CHECK(last_frame_shows(12, 34, 56));
}

static void same_time_is_not_presented_twice(void) {
    static const Reading script[] = {{12, 34, 56, 1}};
    run(script, 1, 5); /* five polls, the clock never moves */
    CHECK(fake.presents == 1);
}

static void a_new_second_is_presented(void) {
    static const Reading script[] = {{12, 34, 56, 1}, {12, 34, 56, 1}, {12, 34, 57, 1}};
    run(script, 3, 3);
    CHECK(fake.presents == 2);
    CHECK(last_frame_shows(12, 34, 57));
}

static void a_new_minute_is_presented_even_if_the_second_repeats(void) {
    static const Reading script[] = {{12, 34, 56, 1}, {12, 35, 56, 1}};
    run(script, 2, 2);
    CHECK(fake.presents == 2);
    CHECK(last_frame_shows(12, 35, 56));
}

static void the_screen_is_kept_awake_on_every_poll(void) {
    static const Reading script[] = {{12, 34, 56, 1}};
    run(script, 1, 4);
    CHECK(fake.awake_calls == 4);
}

static void the_loop_stops_when_exit_is_requested(void) {
    static const Reading script[] = {{12, 34, 56, 1}};
    run(script, 1, 3);
    CHECK(fake.waits == 3); /* ran exactly three polls, then quit */
    run(script, 1, 0);      /* asked to quit before the first poll */
    CHECK(fake.presents == 0);
    CHECK(fake.reads == 0);
    CHECK(fake.awake_calls == 0);
}

static void an_unreadable_clock_shows_dashes_and_the_loop_carries_on(void) {
    static const Reading script[] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {12, 34, 56, 1}};
    run(script, 3, 1);
    CHECK(fake.presents == 1);
    CHECK(last_frame_shows(-1, -1, -1)); /* dashes, not 00:00:00 */
    run(script, 3, 3);                   /* two failed reads, then a good one */
    CHECK(fake.presents == 2);           /* dashes once, then the real time */
    CHECK(last_frame_shows(12, 34, 56));
}

int main(void) {
    RUN(first_frame_is_presented_immediately);
    RUN(same_time_is_not_presented_twice);
    RUN(a_new_second_is_presented);
    RUN(a_new_minute_is_presented_even_if_the_second_repeats);
    RUN(the_screen_is_kept_awake_on_every_poll);
    RUN(the_loop_stops_when_exit_is_requested);
    RUN(an_unreadable_clock_shows_dashes_and_the_loop_carries_on);
    return mt_report();
}
