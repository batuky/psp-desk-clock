#include <stdlib.h>
#include <string.h>

#include "bitmap_font.h"
#include "clock_face.h"
#include "minitest.h"
#include "month_calendar.h"
#include "pinned_shapes.h"
#include "ppm.h"

static Framebuffer fb;
static Framebuffer other;

static LocalTime on(int year, int month, int day, int hour, int minute) {
    LocalTime t = {year, month, day, hour, minute, 0};
    return t;
}

/* Start every render from garbage so a test can't pass on leftovers. */
static void render(ClockLayout layout, const LocalTime *now) {
    memset(&fb, 0x5A, sizeof fb);
    clock_face_render(&fb, layout, now);
}

static Pixel at(int x, int y) { return fb.pixels[y * FB_WIDTH + x]; }

static int rect_on_screen(const ScreenRect *r) {
    return r->w > 0 && r->h > 0 && r->x >= 0 && r->y >= 0 && r->x + r->w <= FB_WIDTH &&
           r->y + r->h <= FB_HEIGHT;
}

static int rects_overlap(const ScreenRect *a, const ScreenRect *b) {
    return a->x < b->x + b->w && b->x < a->x + a->w && a->y < b->y + b->h && b->y < a->y + a->h;
}

/* The time, every Date line and the calendar: the most pieces a layout has. */
#define MAX_PIECES (1 + CLOCK_FACE_MAX_DATE_LINES + 1)

/* Every piece of a layout as a flat list, so checks can treat them alike. */
static int pieces(const ClockFaceRegions *r, const ScreenRect *out[MAX_PIECES]) {
    int n = 0;
    out[n++] = &r->time;
    for (int i = 0; i < r->date_line_count; i++) out[n++] = &r->date_line[i];
    if (r->has_calendar) out[n++] = &r->calendar;
    return n;
}

static int lit_pixels_in(const Framebuffer *f, const ScreenRect *r) {
    int n = 0;
    for (int y = r->y; y < r->y + r->h; y++)
        for (int x = r->x; x < r->x + r->w; x++)
            if (f->pixels[y * FB_WIDTH + x] != PIXEL_BLACK) n++;
    return n;
}

static int all_black(const Framebuffer *f) {
    for (int i = 0; i < FB_WIDTH * FB_HEIGHT; i++)
        if (f->pixels[i] != PIXEL_BLACK) return 0;
    return 1;
}

/* 1 when both framebuffers hold the same pixels inside `r`. */
static int same_inside(const Framebuffer *a, const Framebuffer *b, const ScreenRect *r) {
    for (int y = r->y; y < r->y + r->h; y++)
        for (int x = r->x; x < r->x + r->w; x++)
            if (a->pixels[y * FB_WIDTH + x] != b->pixels[y * FB_WIDTH + x]) return 0;
    return 1;
}

static void blank(Framebuffer *f, const ScreenRect *r) {
    fb_fill_rect(f, r->x, r->y, r->w, r->h, PIXEL_BLACK);
}

/* Bounding box of the lit pixels inside `r`; false when there are none. */
static int ink_box(const ScreenRect *r, int *left, int *right) {
    int found = 0;
    for (int x = r->x; x < r->x + r->w; x++)
        for (int y = r->y; y < r->y + r->h; y++)
            if (at(x, y) != PIXEL_BLACK) {
                if (!found) *left = x;
                *right = x;
                found = 1;
                break;
            }
    return found;
}

static const ClockLayout LAYOUTS[CLOCK_LAYOUT_COUNT] = {CLOCK_LAYOUT_B, CLOCK_LAYOUT_D};

/* --- Where the pieces go ------------------------------------------------ */

static void layout_b_is_the_time_and_one_centred_date_line(void) {
    ClockFaceRegions r;
    clock_face_regions(CLOCK_LAYOUT_B, &r);
    /* Digits 84x130, top at y=12, centred. */
    CHECK(r.time.y == PINNED_SHAPE_B.top && r.time.h == PINNED_SHAPE_B.digit_h);
    /* Four digits, two 8 px gaps, and the colon's column (8 px gap plus
     * twice the bar thickness). */
    CHECK(r.time.w == 4 * PINNED_SHAPE_B.digit_w + 2 * 8 + 8 + 2 * PINNED_SHAPE_B.thickness);
    CHECK(r.time.x - (FB_WIDTH - r.time.x - r.time.w) <= 1);
    CHECK((FB_WIDTH - r.time.x - r.time.w) - r.time.x <= 1);
    CHECK(r.date_line_count == 1);
    CHECK(r.has_calendar == 0);
    /* The Date line sits under the time, centred on it, in 3x text. */
    CHECK(r.date_line[0].y >= r.time.y + r.time.h);
    CHECK(r.date_line[0].h == FONT_GLYPH_H * 3);
    CHECK(abs((r.date_line[0].x + r.date_line[0].w / 2) - (r.time.x + r.time.w / 2)) <= 1);
}

static void a_value_that_is_not_a_layout_gets_layout_d(void) {
    ClockFaceRegions d, bad;
    clock_face_regions(CLOCK_LAYOUT_D, &d);
    clock_face_regions(CLOCK_LAYOUT_COUNT, &bad);
    CHECK(bad.time.y == d.time.y && bad.time.w == d.time.w);
    CHECK(bad.date_line_count == d.date_line_count && bad.has_calendar == d.has_calendar);
    clock_face_regions((ClockLayout)-1, &bad);
    CHECK(bad.date_line_count == d.date_line_count && bad.has_calendar == d.has_calendar);
}

static void the_next_layout_cycles_through_all_of_them(void) {
    CHECK(clock_face_next_layout(CLOCK_LAYOUT_B) == CLOCK_LAYOUT_D);
    CHECK(clock_face_next_layout(CLOCK_LAYOUT_D) == CLOCK_LAYOUT_B);
    /* A value that is not a Layout counts as layout D, so its next is B. */
    CHECK(clock_face_next_layout(CLOCK_LAYOUT_COUNT) == CLOCK_LAYOUT_B);
    CHECK(clock_face_next_layout((ClockLayout)-1) == CLOCK_LAYOUT_B);
}

static void layout_d_is_the_time_the_date_lower_left_and_the_calendar_lower_right(void) {
    ClockFaceRegions r;
    clock_face_regions(CLOCK_LAYOUT_D, &r);
    /* Digits 80x120, top at y=8, centred. */
    CHECK(r.time.y == PINNED_SHAPE_D.top && r.time.h == PINNED_SHAPE_D.digit_h);
    /* Four digits, two 8 px gaps, and the colon's column (8 px gap plus
     * twice the bar thickness). */
    CHECK(r.time.w == 4 * PINNED_SHAPE_D.digit_w + 2 * 8 + 8 + 2 * PINNED_SHAPE_D.thickness);
    CHECK(abs((r.time.x + r.time.w / 2) - FB_WIDTH / 2) <= 1);
    CHECK(r.date_line_count == 2);
    CHECK(r.has_calendar == 1);
    /* Date on two lines, one above the other, in the lower left, 3x text. */
    CHECK(r.date_line[0].h == FONT_GLYPH_H * 3 && r.date_line[1].h == FONT_GLYPH_H * 3);
    CHECK(r.date_line[0].x == r.date_line[1].x);
    CHECK(r.date_line[0].y + r.date_line[0].h <= r.date_line[1].y);
    CHECK(r.date_line[0].y >= r.time.y + r.time.h);
    CHECK(r.date_line[1].x + r.date_line[1].w <= FB_WIDTH / 2);
    /* Calendar in the lower right: 30x17 cells, seven columns, a header and
     * six week rows. */
    CHECK(r.calendar.w == 7 * 30);
    CHECK(r.calendar.h == 7 * 17);
    CHECK(r.calendar.x >= FB_WIDTH / 2);
    CHECK(r.calendar.y >= r.time.y + r.time.h);
}

static void every_piece_is_on_screen(void) {
    for (int l = 0; l < CLOCK_LAYOUT_COUNT; l++) {
        ClockFaceRegions r;
        const ScreenRect *p[MAX_PIECES];
        int n;
        clock_face_regions(LAYOUTS[l], &r);
        n = pieces(&r, p);
        for (int i = 0; i < n; i++) CHECK(rect_on_screen(p[i]));
    }
}

static void no_two_pieces_overlap(void) {
    for (int l = 0; l < CLOCK_LAYOUT_COUNT; l++) {
        ClockFaceRegions r;
        const ScreenRect *p[MAX_PIECES];
        int n;
        clock_face_regions(LAYOUTS[l], &r);
        n = pieces(&r, p);
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++) CHECK(!rects_overlap(p[i], p[j]));
    }
}

/* --- What gets drawn ---------------------------------------------------- */

/* The regions are real: for every day of a month with six week rows and one
 * with five, covering every weekday, plus an unreadable clock, nothing is
 * drawn outside the pieces' rectangles. */
static void drawing_stays_inside_the_pieces(void) {
    for (int l = 0; l < CLOCK_LAYOUT_COUNT; l++) {
        ClockFaceRegions r;
        const ScreenRect *p[MAX_PIECES];
        int n;
        clock_face_regions(LAYOUTS[l], &r);
        n = pieces(&r, p);
        for (int month = 8; month <= 10; month += 2) /* August (6 rows) and October (5) */
            for (int day = 1; day <= 31; day++) {
                LocalTime now = on(2026, month, day, 8 + day % 2 * 12, 8 + day);
                render(LAYOUTS[l], &now);
                for (int i = 0; i < n; i++) blank(&fb, p[i]);
                CHECK(all_black(&fb));
            }
        render(LAYOUTS[l], NULL);
        for (int i = 0; i < n; i++) blank(&fb, p[i]);
        CHECK(all_black(&fb));
    }
}

static void every_piece_draws_something(void) {
    LocalTime now = on(2026, 10, 17, 12, 34);
    for (int l = 0; l < CLOCK_LAYOUT_COUNT; l++) {
        ClockFaceRegions r;
        const ScreenRect *p[MAX_PIECES];
        int n;
        clock_face_regions(LAYOUTS[l], &r);
        n = pieces(&r, p);
        render(LAYOUTS[l], &now);
        for (int i = 0; i < n; i++) CHECK(lit_pixels_in(&fb, p[i]) > 0);
    }
}

static void the_time_shows_hours_and_minutes_without_seconds(void) {
    LocalTime a = on(2026, 10, 17, 12, 34), b = on(2026, 10, 17, 12, 34);
    b.second = 59;
    for (int l = 0; l < CLOCK_LAYOUT_COUNT; l++) {
        render(LAYOUTS[l], &a);
        other = fb;
        render(LAYOUTS[l], &b);
        CHECK(memcmp(&fb, &other, sizeof fb) == 0);
    }
}

/* The Date line text in a region is exactly what the font draws for it. */
static void expect_text_at(const ScreenRect *r, const char *text) {
    fb_clear(&other, PIXEL_BLACK);
    bitmap_font_draw_text(&other, r->x, r->y, 3, text, PIXEL_WHITE);
    CHECK(same_inside(&fb, &other, r));
}

static void layout_d_shows_the_date_and_the_weekday_on_two_lines(void) {
    ClockFaceRegions r;
    LocalTime now = on(2026, 10, 17, 12, 34);
    clock_face_regions(CLOCK_LAYOUT_D, &r);
    render(CLOCK_LAYOUT_D, &now);
    expect_text_at(&r.date_line[0], "17.10.2026");
    expect_text_at(&r.date_line[1], "Cumartesi");

    now = on(2026, 8, 31, 12, 34); /* a Monday, the longest weekday */
    render(CLOCK_LAYOUT_D, &now);
    expect_text_at(&r.date_line[0], "31.08.2026");
    expect_text_at(&r.date_line[1], "Pazartesi");
}

static void layout_b_shows_the_whole_date_line_centred_on_the_screen(void) {
    ClockFaceRegions r;
    LocalTime now = on(2026, 10, 17, 12, 34);
    int left, right;
    clock_face_regions(CLOCK_LAYOUT_B, &r);
    render(CLOCK_LAYOUT_B, &now);
    CHECK(ink_box(&r.date_line[0], &left, &right));
    /* "17.10.2026 Cumartesi" is 20 characters of 18 pixels, less the 3 pixels
     * of space after the last: 357 wide. */
    CHECK(right - left + 1 >= 357 - 6 && right - left + 1 <= 357);
    CHECK(abs((left + right) / 2 - FB_WIDTH / 2) <= 3);

    /* A shorter line stays centred. */
    now = on(2026, 2, 3, 12, 34); /* "03.02.2026 Salı": 15 characters */
    render(CLOCK_LAYOUT_B, &now);
    CHECK(ink_box(&r.date_line[0], &left, &right));
    CHECK(abs((left + right) / 2 - FB_WIDTH / 2) <= 3);
}

static void layout_d_draws_the_month_calendar_in_its_region(void) {
    ClockFaceRegions r;
    LocalTime now = on(2026, 10, 17, 12, 34);
    clock_face_regions(CLOCK_LAYOUT_D, &r);
    render(CLOCK_LAYOUT_D, &now);
    fb_clear(&other, PIXEL_BLACK);
    month_calendar_draw(&other, r.calendar.x, r.calendar.y, 30, 17, &now);
    CHECK(same_inside(&fb, &other, &r.calendar));
}

static void layout_b_has_no_calendar(void) {
    ClockFaceRegions r;
    LocalTime now = on(2026, 10, 17, 12, 34);
    clock_face_regions(CLOCK_LAYOUT_B, &r);
    render(CLOCK_LAYOUT_B, &now);
    /* Everything below the Date line is empty. */
    {
        int top = r.date_line[0].y + r.date_line[0].h;
        ScreenRect below = {0, top, FB_WIDTH, FB_HEIGHT - top};
        CHECK(lit_pixels_in(&fb, &below) == 0);
    }
}

/* An unreadable clock shows dashes, never a wrong time or date. */
static void an_unreadable_clock_shows_dashes_and_only_the_calendar_header(void) {
    ClockFaceRegions r;
    clock_face_regions(CLOCK_LAYOUT_D, &r);
    render(CLOCK_LAYOUT_D, NULL);
    expect_text_at(&r.date_line[0], "--.--.----");
    CHECK(lit_pixels_in(&fb, &r.date_line[1]) == 0);
    {
        ScreenRect days = {r.calendar.x, r.calendar.y + 17, r.calendar.w, r.calendar.h - 17};
        CHECK(lit_pixels_in(&fb, &days) == 0);
    }

    clock_face_regions(CLOCK_LAYOUT_B, &r);
    render(CLOCK_LAYOUT_B, NULL);
    {
        int left, right;
        CHECK(ink_box(&r.date_line[0], &left, &right));
        CHECK(abs((left + right) / 2 - FB_WIDTH / 2) <= 3); /* dashes, centred */
    }
}

static void the_background_is_black(void) {
    LocalTime now = on(2026, 10, 17, 12, 34);
    for (int l = 0; l < CLOCK_LAYOUT_COUNT; l++) {
        render(LAYOUTS[l], &now);
        CHECK_HEX(at(0, 0), PIXEL_BLACK);
        CHECK_HEX(at(FB_WIDTH - 1, FB_HEIGHT - 1), PIXEL_BLACK);
    }
}

/* Golden images of 17.10.2026 12:34, one per layout. Written once and
 * approved by eye; regenerate with `make update-golden` and review the
 * images before committing. */
static void layout_b_matches_the_golden_image(void) {
    const char *path = "tests/golden/clock_face_b.ppm";
    LocalTime now = on(2026, 10, 17, 12, 34);
    render(CLOCK_LAYOUT_B, &now);
    if (getenv("UPDATE_GOLDEN")) CHECK(ppm_write(path, &fb));
    CHECK(ppm_matches(path, &fb));
}

static void layout_d_matches_the_golden_image(void) {
    const char *path = "tests/golden/clock_face_d.ppm";
    LocalTime now = on(2026, 10, 17, 12, 34);
    render(CLOCK_LAYOUT_D, &now);
    if (getenv("UPDATE_GOLDEN")) CHECK(ppm_write(path, &fb));
    CHECK(ppm_matches(path, &fb));
}

int main(void) {
    RUN(layout_b_is_the_time_and_one_centred_date_line);
    RUN(layout_d_is_the_time_the_date_lower_left_and_the_calendar_lower_right);
    RUN(a_value_that_is_not_a_layout_gets_layout_d);
    RUN(the_next_layout_cycles_through_all_of_them);
    RUN(every_piece_is_on_screen);
    RUN(no_two_pieces_overlap);
    RUN(drawing_stays_inside_the_pieces);
    RUN(every_piece_draws_something);
    RUN(the_time_shows_hours_and_minutes_without_seconds);
    RUN(layout_d_shows_the_date_and_the_weekday_on_two_lines);
    RUN(layout_b_shows_the_whole_date_line_centred_on_the_screen);
    RUN(layout_d_draws_the_month_calendar_in_its_region);
    RUN(layout_b_has_no_calendar);
    RUN(an_unreadable_clock_shows_dashes_and_only_the_calendar_header);
    RUN(the_background_is_black);
    RUN(layout_b_matches_the_golden_image);
    RUN(layout_d_matches_the_golden_image);
    return mt_report();
}
