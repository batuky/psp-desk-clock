#include <string.h>

#include "clock_app.h"
#include "clock_face.h"
#include "local_time.h"
#include "minitest.h"
#include "settings.h"

/* A scripted stand-in for the hardware. Each read_time call returns the next
 * scripted reading (the last one repeats). The loop quits after
 * `polls_before_exit` waits. */
typedef struct {
    LocalTime time;
    int readable;
} Reading;

#define READING_ON(y, mo, d, h, m, s) {{(y), (mo), (d), (h), (m), (s)}, 1}
#define AT(h, m, s) READING_ON(2026, 10, 17, h, m, s)
#define UNREADABLE {{0, 0, 0, 0, 0, 0}, 0}

typedef struct {
    const Reading *script;
    int script_len;
    int polls_before_exit;
    unsigned press_polls; /* bit n set: the Layout switch was pressed before poll n (n < 32) */
    int reads, waits, awake_calls, presents, switch_checks;
    Framebuffer last_frame;
    /* The settings file: `file` holds `file_len` bytes, or is absent when
     * file_len < 0. A write replaces it, unless writes_fail. */
    char file[SETTINGS_MAX_BYTES];
    int file_len;
    int writes_fail;
    int settings_reads, settings_writes;
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

static int fake_read_settings(void *ctx, char *buf, size_t cap) {
    Fake *f = ctx;
    size_t len = (size_t)f->file_len;
    f->settings_reads++;
    if (f->file_len < 0) return -1;
    if (len > cap) len = cap;
    memcpy(buf, f->file, len);
    return (int)len;
}

static int fake_write_settings(void *ctx, const char *buf, size_t len) {
    Fake *f = ctx;
    f->settings_writes++;
    if (f->writes_fail) return -1;
    memcpy(f->file, buf, len);
    f->file_len = (int)len;
    return 0;
}

static Fake fake;
static Framebuffer scratch;
static Framebuffer expected;

/* What the app finds in the settings file when it starts. */
static ClockLayout layout = CLOCK_LAYOUT_D; /* the Layout saved last time */
static const char *raw_file = NULL;         /* if set, the file's exact text instead */
static int file_absent = 0;
static int writes_fail = 0;

static void restore_defaults(void) {
    layout = CLOCK_LAYOUT_D;
    raw_file = NULL;
    file_absent = 0;
    writes_fail = 0;
}

static void run_with_presses(const Reading *script, int len, int polls, unsigned press_polls) {
    ClockPlatform platform = {fake_read_time, fake_present, fake_keep_awake,
                              fake_wait, fake_exit_requested, fake_switch_pressed,
                              fake_read_settings, fake_write_settings, &fake};
    memset(&fake, 0, sizeof fake);
    if (file_absent) {
        fake.file_len = -1;
    } else if (raw_file) {
        fake.file_len = (int)strlen(raw_file);
        CHECK(fake.file_len <= (int)sizeof fake.file); /* the fake file holds no more */
        if (fake.file_len > (int)sizeof fake.file) fake.file_len = (int)sizeof fake.file;
        memcpy(fake.file, raw_file, (size_t)fake.file_len);
    } else {
        fake.file_len = (int)settings_format_layout(layout, fake.file);
    }
    fake.writes_fail = writes_fail;
    fake.script = script;
    fake.script_len = len;
    fake.polls_before_exit = polls;
    fake.press_polls = press_polls;
    clock_app_run(&platform, &scratch);
    /* These apply to one run only, so a test that stops early can't leak them. */
    raw_file = NULL;
    file_absent = 0;
    writes_fail = 0;
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

/* What the settings file says now, as a Layout. */
static ClockLayout saved_layout(void) {
    return settings_parse_layout(fake.file, (size_t)fake.file_len);
}

static void the_saved_layout_is_shown_at_start(void) {
    static const Reading script[] = {AT(12, 34, 56)};
    layout = CLOCK_LAYOUT_B;
    run(script, 1, 1);
    CHECK(last_frame_shows_in(CLOCK_LAYOUT_B, &script[0].time));
    restore_defaults();
}

static void a_missing_or_unreadable_file_starts_in_the_default_layout(void) {
    static const Reading script[] = {AT(12, 34, 56)};
    file_absent = 1;
    run(script, 1, 1);
    CHECK(last_frame_shows_in(CLOCK_LAYOUT_DEFAULT, &script[0].time));
    CHECK(fake.settings_reads == 1);
    CHECK(fake.settings_writes == 0); /* nothing to save yet */
    restore_defaults();
}

static void a_corrupt_file_starts_in_the_default_layout(void) {
    static const Reading script[] = {AT(12, 34, 56)};
    raw_file = "layout=Z\n";
    run(script, 1, 1);
    CHECK(last_frame_shows_in(CLOCK_LAYOUT_DEFAULT, &script[0].time));
    raw_file = "garbage\001";
    run(script, 1, 1);
    CHECK(last_frame_shows_in(CLOCK_LAYOUT_DEFAULT, &script[0].time));
    restore_defaults();
}

static void the_file_is_read_once_at_start_not_on_every_poll(void) {
    static const Reading script[] = {AT(12, 34, 56)};
    run(script, 1, 5);
    CHECK(fake.settings_reads == 1);
}

static void a_press_saves_the_new_layout(void) {
    static const Reading script[] = {AT(12, 34, 56)};
    layout = CLOCK_LAYOUT_D;
    run_with_presses(script, 1, 3, 1u << 1);
    CHECK(fake.settings_writes == 1);
    CHECK(saved_layout() == CLOCK_LAYOUT_B);
    run_with_presses(script, 1, 3, (1u << 1) | (1u << 2));
    CHECK(fake.settings_writes == 2);
    CHECK(saved_layout() == CLOCK_LAYOUT_D);
    restore_defaults();
}

static void the_saved_layout_is_what_the_next_start_shows(void) {
    static const Reading script[] = {AT(12, 34, 56)};
    layout = CLOCK_LAYOUT_D;
    run_with_presses(script, 1, 2, 1u << 1);
    layout = saved_layout(); /* the file this run left behind */
    run(script, 1, 1);
    CHECK(last_frame_shows_in(CLOCK_LAYOUT_B, &script[0].time));
    restore_defaults();
}

static void nothing_is_written_when_nothing_was_pressed(void) {
    static const Reading script[] = {AT(12, 34, 56), AT(12, 35, 56)};
    run(script, 2, 5);
    CHECK(fake.settings_writes == 0);
}

/* The settings are a convenience: when the Memory Stick refuses the write,
 * the clock still switches and keeps running. */
static void a_failed_write_is_ignored(void) {
    static const Reading script[] = {AT(12, 34, 56)};
    layout = CLOCK_LAYOUT_D;
    writes_fail = 1;
    run_with_presses(script, 1, 4, (1u << 1) | (1u << 2));
    CHECK(fake.settings_writes == 2); /* tried on each press, did not give up */
    CHECK(fake.waits == 4);           /* the loop ran to the end */
    CHECK(fake.presents == 3);
    CHECK(last_frame_shows_in(CLOCK_LAYOUT_D, &script[0].time));
    restore_defaults();
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
    static const Reading script[] = {READING_ON(2026, 10, 17, 12, 0, 0),
                                     READING_ON(2026, 10, 18, 12, 0, 0)};
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
    RUN(the_saved_layout_is_shown_at_start);
    RUN(a_missing_or_unreadable_file_starts_in_the_default_layout);
    RUN(a_corrupt_file_starts_in_the_default_layout);
    RUN(the_file_is_read_once_at_start_not_on_every_poll);
    RUN(a_press_saves_the_new_layout);
    RUN(the_saved_layout_is_what_the_next_start_shows);
    RUN(nothing_is_written_when_nothing_was_pressed);
    RUN(a_failed_write_is_ignored);
    RUN(a_new_minute_is_presented);
    RUN(different_readings_are_never_mistaken_for_each_other);
    RUN(a_new_day_is_presented_even_if_the_time_of_day_repeats);
    RUN(the_screen_is_kept_awake_on_every_poll);
    RUN(the_loop_stops_when_exit_is_requested);
    RUN(an_unreadable_clock_shows_dashes_and_the_loop_carries_on);
    RUN(the_time_comes_back_after_dashes_even_if_it_is_unchanged);
    return mt_report();
}
