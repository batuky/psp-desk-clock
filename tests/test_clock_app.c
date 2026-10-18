#include <string.h>

#include "clock_app.h"
#include "clock_face.h"
#include "local_time.h"
#include "minitest.h"

/* A scripted stand-in for the hardware. Each read_time call returns the next
 * scripted reading (the last one repeats). The loop quits after
 * `polls_before_exit` waits. */
typedef struct {
    LocalTime time;
    int readable;
} Reading;

#define ON(y, mo, d, h, m, s) {{(y), (mo), (d), (h), (m), (s)}, 1}
#define AT(h, m, s) ON(2026, 10, 17, h, m, s)
#define UNREADABLE {{0, 0, 0, 0, 0, 0}, 0}

typedef struct {
    const Reading *script;
    int script_len;
    int polls_before_exit;
    unsigned press_polls; /* bit n set: the Layout switch was pressed before poll n (n < 32) */
    int reads, waits, awake_calls, presents, switch_checks;
    Framebuffer last_frame;
} Fake;

static int fake_read_time(void *ctx, LocalTime *now) {
    Fake *f = ctx;
    const Reading *r = &f->script[f->reads < f->script_len ? f->reads : f->script_len - 1];
    f->reads++;
    if (!r->readable) return -1;
    *now = r->time;
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

static int fake_switch_pressed(void *ctx) {
    Fake *f = ctx;
    int poll = f->switch_checks++;
    return poll < 32 && ((f->press_polls >> poll) & 1u);
}

static Fake fake;
static Framebuffer scratch;
static Framebuffer expected;

static ClockLayout layout = CLOCK_LAYOUT_D;

static void run_with_presses(const Reading *script, int len, int polls, unsigned press_polls) {
    ClockPlatform platform = {fake_read_time, fake_present, fake_keep_awake,
                              fake_wait, fake_exit_requested, fake_switch_pressed, &fake};
    memset(&fake, 0, sizeof fake);
    fake.script = script;
    fake.script_len = len;
    fake.polls_before_exit = polls;
    fake.press_polls = press_polls;
    clock_app_run(&platform, &scratch, layout);
}

static void run(const Reading *script, int len, int polls) {
    run_with_presses(script, len, polls, 0);
}

/* The last presented frame is the Clock face for `now` in the current layout
 * (NULL: the dashes shown when the clock can't be read). */
static int last_frame_shows_in(ClockLayout which, const LocalTime *now) {
    clock_face_render(&expected, which, now);
    return memcmp(&fake.last_frame, &expected, sizeof expected) == 0;
}

static int last_frame_shows(const LocalTime *now) { return last_frame_shows_in(layout, now); }

static void first_frame_is_presented_immediately(void) {
    static const Reading script[] = {AT(12, 34, 56)};
    run(script, 1, 1);
    CHECK(fake.presents == 1);
    CHECK(last_frame_shows(&script[0].time));
}

static void same_time_is_not_presented_twice(void) {
    static const Reading script[] = {AT(12, 34, 56)};
    run(script, 1, 5); /* five polls, the clock never moves */
    CHECK(fake.presents == 1);
}

/* The Clock face has no seconds, so a new second changes nothing on screen. */
static void a_new_second_is_not_presented(void) {
    static const Reading script[] = {AT(12, 34, 56), AT(12, 34, 57), AT(12, 34, 0)};
    run(script, 3, 3);
    CHECK(fake.presents == 1);
}

static void the_chosen_layout_is_the_one_presented(void) {
    static const Reading script[] = {AT(12, 34, 56)};
    layout = CLOCK_LAYOUT_B;
    run(script, 1, 1);
    CHECK(last_frame_shows(&script[0].time));
    layout = CLOCK_LAYOUT_D;
    CHECK(!last_frame_shows(&script[0].time)); /* the two layouts really differ */
}

/* Pressing the Layout switch shows the other Layout at once, even though the
 * time has not changed. */
static void a_press_switches_the_layout_and_redraws_at_once(void) {
    static const Reading script[] = {AT(12, 34, 56)};
    layout = CLOCK_LAYOUT_D;
    run_with_presses(script, 1, 2, 1u << 1); /* pressed before the second poll */
    CHECK(fake.presents == 2);
    CHECK(last_frame_shows_in(CLOCK_LAYOUT_B, &script[0].time));
}

static void a_second_press_switches_back(void) {
    static const Reading script[] = {AT(12, 34, 56)};
    layout = CLOCK_LAYOUT_D;
    run_with_presses(script, 1, 3, (1u << 1) | (1u << 2));
    CHECK(fake.presents == 3);
    CHECK(last_frame_shows_in(CLOCK_LAYOUT_D, &script[0].time));
}

static void a_press_while_the_clock_is_unreadable_still_switches_the_dashes(void) {
    static const Reading script[] = {UNREADABLE};
    layout = CLOCK_LAYOUT_D;
    run_with_presses(script, 1, 2, 1u << 1);
    CHECK(fake.presents == 2);
    CHECK(last_frame_shows_in(CLOCK_LAYOUT_B, NULL));
}

static void a_new_minute_is_presented(void) {
    static const Reading script[] = {AT(12, 34, 56), AT(12, 35, 56)};
    run(script, 2, 2);
    CHECK(fake.presents == 2);
    CHECK(last_frame_shows(&script[1].time));
}

/* Out-of-range readings must not be mistaken for a different, valid time:
 * minute 60 of hour 0 is not the same display as 01:00. */
static void different_readings_are_never_mistaken_for_each_other(void) {
    static const Reading script[] = {AT(0, 60, 0), AT(1, 0, 0)};
    run(script, 2, 2);
    CHECK(fake.presents == 2);
    CHECK(last_frame_shows(&script[1].time));
}

/* The Date line shows the date, so a new day must redraw even when the time
 * of day is the same as before. */
static void a_new_day_is_presented_even_if_the_time_of_day_repeats(void) {
    static const Reading script[] = {ON(2026, 10, 17, 12, 0, 0), ON(2026, 10, 18, 12, 0, 0)};
    run(script, 2, 2);
    CHECK(fake.presents == 2);
}

static void the_screen_is_kept_awake_on_every_poll(void) {
    static const Reading script[] = {AT(12, 34, 56)};
    run(script, 1, 4);
    CHECK(fake.awake_calls == 4);
}

static void the_loop_stops_when_exit_is_requested(void) {
    static const Reading script[] = {AT(12, 34, 56)};
    run(script, 1, 3);
    CHECK(fake.waits == 3); /* ran exactly three polls, then quit */
    run(script, 1, 0);      /* asked to quit before the first poll */
    CHECK(fake.presents == 0);
    CHECK(fake.reads == 0);
    CHECK(fake.awake_calls == 0);
    CHECK(fake.switch_checks == 0);
}

static void an_unreadable_clock_shows_dashes_and_the_loop_carries_on(void) {
    static const Reading script[] = {UNREADABLE, UNREADABLE, AT(12, 34, 56)};
    run(script, 3, 1);
    CHECK(fake.presents == 1);
    CHECK(last_frame_shows(NULL)); /* dashes, not 00:00 */
    run(script, 3, 3);             /* two failed reads, then a good one */
    CHECK(fake.presents == 2);     /* dashes once, then the real time */
    CHECK(last_frame_shows(&script[2].time));
}

/* A good reading, then a lost one, then the same reading again: the dashes
 * are shown in between, and the time comes back even though it did not
 * change since it was last shown. */
static void the_time_comes_back_after_dashes_even_if_it_is_unchanged(void) {
    static const Reading script[] = {AT(12, 34, 56), UNREADABLE, UNREADABLE, AT(12, 34, 56)};
    run(script, 4, 4);
    CHECK(fake.presents == 3); /* time, dashes once, time again */
    CHECK(last_frame_shows(&script[3].time));
}

int main(void) {
    RUN(first_frame_is_presented_immediately);
    RUN(same_time_is_not_presented_twice);
    RUN(a_new_second_is_not_presented);
    RUN(the_chosen_layout_is_the_one_presented);
    RUN(a_press_switches_the_layout_and_redraws_at_once);
    RUN(a_second_press_switches_back);
    RUN(a_press_while_the_clock_is_unreadable_still_switches_the_dashes);
    RUN(a_new_minute_is_presented);
    RUN(different_readings_are_never_mistaken_for_each_other);
    RUN(a_new_day_is_presented_even_if_the_time_of_day_repeats);
    RUN(the_screen_is_kept_awake_on_every_poll);
    RUN(the_loop_stops_when_exit_is_requested);
    RUN(an_unreadable_clock_shows_dashes_and_the_loop_carries_on);
    RUN(the_time_comes_back_after_dashes_even_if_it_is_unchanged);
    return mt_report();
}
