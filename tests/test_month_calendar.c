#include <stdlib.h>
#include <string.h>

#include "minitest.h"
#include "month_calendar.h"
#include "ppm.h"

/* The expected layouts come from the calendar, not from the code: each month
 * below is a real month whose first weekday and length are well known. */

static LocalTime on(int year, int month, int day) {
    LocalTime t = {year, month, day, 12, 0, 0};
    return t;
}

static void expect_day(const MonthCells *m, int day, int column, int row) {
    CHECK(m->column[day] == column);
    CHECK(m->row[day] == row);
}

static void october_2026_starts_on_a_thursday_and_needs_five_rows(void) {
    LocalTime now = on(2026, 10, 17);
    MonthCells m;
    CHECK(month_calendar_cells(&now, &m) == 1);
    CHECK(m.days == 31);
    CHECK(m.rows == 5);
    CHECK(m.today == 17);
    expect_day(&m, 1, 3, 0);  /* Thursday */
    expect_day(&m, 5, 0, 1);  /* Monday the 5th starts the second row */
    expect_day(&m, 17, 5, 2); /* Saturday */
    expect_day(&m, 31, 5, 4); /* Saturday */
}

static void february_2024_has_a_leap_day(void) {
    LocalTime now = on(2024, 2, 29);
    MonthCells m;
    CHECK(month_calendar_cells(&now, &m) == 1);
    CHECK(m.days == 29);
    CHECK(m.rows == 5);
    CHECK(m.today == 29);
    expect_day(&m, 1, 3, 0);  /* Thursday */
    expect_day(&m, 29, 3, 4); /* Thursday again */
}

static void february_2026_has_28_days_and_starts_on_a_sunday(void) {
    LocalTime now = on(2026, 2, 1);
    MonthCells m;
    CHECK(month_calendar_cells(&now, &m) == 1);
    CHECK(m.days == 28);
    CHECK(m.rows == 5);
    expect_day(&m, 1, 6, 0);  /* Sunday ends the first row */
    expect_day(&m, 2, 0, 1);
    expect_day(&m, 28, 5, 4); /* Saturday */
}

static void a_month_can_fit_exactly_four_rows(void) {
    LocalTime now = on(2021, 2, 10); /* 1 February 2021 was a Monday */
    MonthCells m;
    CHECK(month_calendar_cells(&now, &m) == 1);
    CHECK(m.days == 28);
    CHECK(m.rows == 4);
    expect_day(&m, 1, 0, 0);
    expect_day(&m, 28, 6, 3);
}

static void some_months_need_six_rows(void) {
    LocalTime august = on(2026, 8, 3); /* 31 days, starts on a Saturday */
    LocalTime november = on(2026, 11, 3); /* 30 days, starts on a Sunday */
    MonthCells m;
    CHECK(month_calendar_cells(&august, &m) == 1);
    CHECK(m.rows == 6);
    expect_day(&m, 1, 5, 0);
    expect_day(&m, 31, 0, 5); /* Monday */
    CHECK(month_calendar_cells(&november, &m) == 1);
    CHECK(m.rows == 6);
    expect_day(&m, 1, 6, 0);
    expect_day(&m, 30, 0, 5); /* Monday */
}

static void days_follow_each_other_one_cell_at_a_time(void) {
    for (int month = 1; month <= 12; month++) {
        LocalTime now = on(2026, month, 1);
        MonthCells m;
        CHECK(month_calendar_cells(&now, &m) == 1);
        for (int day = 1; day < m.days; day++)
            CHECK(m.row[day + 1] * CALENDAR_COLUMNS + m.column[day + 1] ==
                  m.row[day] * CALENDAR_COLUMNS + m.column[day] + 1);
        CHECK(m.row[m.days] == m.rows - 1); /* the last day is in the last row */
    }
}

static void a_missing_or_impossible_date_gives_no_layout(void) {
    MonthCells m, untouched;
    LocalTime bad_month = on(2026, 13, 1), leap_day_that_isnt = on(2026, 2, 29);
    memset(&m, 0x5A, sizeof m);
    memcpy(&untouched, &m, sizeof m);
    CHECK(month_calendar_cells(NULL, &m) == 0);
    CHECK(month_calendar_cells(&bad_month, &m) == 0);
    CHECK(month_calendar_cells(&leap_day_that_isnt, &m) == 0);
    CHECK(memcmp(&m, &untouched, sizeof m) == 0);
}

/* ---- drawing ---- */

#define CW 30
#define CH 17
#define GX 250
#define GY 138

static Framebuffer fb, other;

static void clear(Framebuffer *f) {
    for (int i = 0; i < FB_WIDTH * FB_HEIGHT; i++) f->pixels[i] = PIXEL_BLACK;
}

static Pixel px(const Framebuffer *f, int x, int y) { return f->pixels[y * FB_WIDTH + x]; }

/* Grid row 0 is the header; week rows start at 1. */
static int cell_x(int column) { return GX + column * CW; }
static int cell_y(int grid_row) { return GY + grid_row * CH; }

static int lit_in_cell(const Framebuffer *f, int column, int grid_row) {
    int n = 0;
    for (int y = cell_y(grid_row); y < cell_y(grid_row) + CH; y++)
        for (int x = cell_x(column); x < cell_x(column) + CW; x++) n += px(f, x, y) != PIXEL_BLACK;
    return n;
}

static int same_cell(const Framebuffer *a, int a_column, int a_row, const Framebuffer *b,
                     int b_column, int b_row) {
    for (int dy = 0; dy < CH; dy++)
        for (int dx = 0; dx < CW; dx++)
            if (px(a, cell_x(a_column) + dx, cell_y(a_row) + dy) !=
                px(b, cell_x(b_column) + dx, cell_y(b_row) + dy))
                return 0;
    return 1;
}

static void draw(Framebuffer *f, const LocalTime *now) {
    clear(f);
    month_calendar_draw(f, GX, GY, CW, CH, now);
}

static void the_header_shows_the_weekday_initials(void) {
    LocalTime now = on(2026, 10, 17);
    draw(&fb, &now);
    for (int c = 0; c < CALENDAR_COLUMNS; c++) {
        CHECK(lit_in_cell(&fb, c, 0) > 0);
        CHECK(px(&fb, cell_x(c) + 1, cell_y(0) + 1) == PIXEL_BLACK); /* not highlighted */
    }
    /* P S Ç P C C P: equal letters look equal, different letters differ. */
    CHECK(same_cell(&fb, 0, 0, &fb, 3, 0));
    CHECK(same_cell(&fb, 0, 0, &fb, 6, 0));
    CHECK(same_cell(&fb, 4, 0, &fb, 5, 0));
    CHECK(!same_cell(&fb, 0, 0, &fb, 1, 0)); /* P is not S */
    CHECK(!same_cell(&fb, 1, 0, &fb, 4, 0)); /* S is not C */
    CHECK(!same_cell(&fb, 2, 0, &fb, 4, 0)); /* Ç is not C */
}

static void every_day_has_a_cell_and_the_other_cells_stay_empty(void) {
    LocalTime now = on(2026, 10, 17);
    MonthCells m;
    int occupied[CALENDAR_MAX_ROWS][CALENDAR_COLUMNS] = {{0}};
    CHECK(month_calendar_cells(&now, &m) == 1);
    for (int d = 1; d <= m.days; d++) occupied[m.row[d]][m.column[d]] = 1;
    draw(&fb, &now);
    for (int r = 0; r < CALENDAR_MAX_ROWS; r++)
        for (int c = 0; c < CALENDAR_COLUMNS; c++) {
            if (occupied[r][c])
                CHECK(lit_in_cell(&fb, c, r + 1) > 0);
            else
                CHECK(lit_in_cell(&fb, c, r + 1) == 0);
        }
}

static void today_is_a_white_cell_with_black_digits(void) {
    LocalTime now = on(2026, 10, 17);
    MonthCells m;
    CHECK(month_calendar_cells(&now, &m) == 1);
    draw(&fb, &now);
    for (int d = 1; d <= m.days; d++) {
        int x = cell_x(m.column[d]), y = cell_y(m.row[d] + 1);
        Pixel corner = px(&fb, x + 1, y + 1);
        int black = 0;
        for (int dy = 0; dy < CH; dy++)
            for (int dx = 0; dx < CW; dx++) black += px(&fb, x + dx, y + dy) == PIXEL_BLACK;
        if (d == 17) {
            CHECK_HEX(corner, PIXEL_WHITE);
            CHECK(black > 0); /* the digits, cut out of the white */
        } else {
            CHECK_HEX(corner, PIXEL_BLACK);
        }
    }
}

static void a_number_looks_the_same_wherever_it_sits(void) {
    /* The 5th is a Monday in October 2026 and a Thursday in November 2026. */
    LocalTime october = on(2026, 10, 17), november = on(2026, 11, 17);
    MonthCells a, b;
    draw(&fb, &october);
    draw(&other, &november);
    CHECK(month_calendar_cells(&october, &a) == 1);
    CHECK(month_calendar_cells(&november, &b) == 1);
    CHECK(a.column[5] != b.column[5]); /* really in different places */
    CHECK(same_cell(&fb, a.column[5], a.row[5] + 1, &other, b.column[5], b.row[5] + 1));
}

static void nothing_is_drawn_outside_the_grid(void) {
    static const LocalTime six_rows = {2026, 8, 3, 12, 0, 0};
    LocalTime now = on(2026, 10, 17);
    const LocalTime *cases[] = {&now, &six_rows, NULL};
    for (int i = 0; i < 3; i++) {
        MonthCells m;
        int rows = cases[i] && month_calendar_cells(cases[i], &m) ? m.rows : 0;
        int outside = 0;
        draw(&fb, cases[i]);
        for (int y = 0; y < FB_HEIGHT; y++)
            for (int x = 0; x < FB_WIDTH; x++) {
                int inside = x >= GX && x < GX + CALENDAR_COLUMNS * CW && y >= GY &&
                             y < GY + (1 + rows) * CH;
                if (!inside && px(&fb, x, y) != PIXEL_BLACK) outside++;
            }
        CHECK(outside == 0);
    }
}

static void a_missing_or_impossible_date_draws_only_the_header(void) {
    LocalTime bad = on(2026, 2, 29);
    const LocalTime *cases[] = {NULL, &bad};
    for (int i = 0; i < 2; i++) {
        draw(&fb, cases[i]);
        for (int c = 0; c < CALENDAR_COLUMNS; c++) CHECK(lit_in_cell(&fb, c, 0) > 0);
        for (int r = 1; r <= CALENDAR_MAX_ROWS; r++)
            for (int c = 0; c < CALENDAR_COLUMNS; c++) CHECK(lit_in_cell(&fb, c, r) == 0);
    }
}

/* Neighbouring numbers must read as separate numbers. Measured from the
 * drawing: take a day whose two digits are both full-width glyphs (the 20th:
 * '2' and '0'), find the blank gap between those two digits, and compare it
 * with the blank gap between the 20th and the 21st next to it. The gap
 * between numbers has to be at least three times the gap between digits, or
 * "20 21" reads as "2021". */
#define ORIGIN 8
#define NUMBER_TO_DIGIT_GAP_RATIO 3

static int column_has_ink(int x, int y0, int y1) {
    for (int y = y0; y < y1; y++)
        if (px(&fb, x, y) != PIXEL_BLACK) return 1;
    return 0;
}

/* Blank columns between the two digits of the 20th, and between the 20th
 * and the 21st. Both are written to the arguments. */
static void measure_gaps(int cw, int ch, int *between_digits, int *between_numbers) {
    LocalTime now = on(2026, 10, 1); /* today is the 1st: no highlight in the rows below */
    MonthCells m;
    int y0, y1, x0, x1, x, ink_end_of_20 = -1, ink_start_of_21 = -1;

    CHECK(month_calendar_cells(&now, &m) == 1);
    clear(&fb);
    month_calendar_draw(&fb, ORIGIN, ORIGIN, cw, ch, &now);
    y0 = ORIGIN + (m.row[20] + 1) * ch;
    y1 = y0 + ch;
    x0 = ORIGIN + m.column[20] * cw; /* the 21st is in the next column */
    x1 = x0 + cw;

    /* Gap inside the 20th: skip the first digit's ink, count the blanks. */
    x = x0;
    while (x < x1 && !column_has_ink(x, y0, y1)) x++;
    while (x < x1 && column_has_ink(x, y0, y1)) x++;
    *between_digits = 0;
    while (x < x1 && !column_has_ink(x, y0, y1)) { (*between_digits)++; x++; }

    for (x = x0; x < x1; x++)
        if (column_has_ink(x, y0, y1)) ink_end_of_20 = x;
    for (x = x1; x < x1 + cw; x++)
        if (column_has_ink(x, y0, y1)) { ink_start_of_21 = x; break; }
    *between_numbers = ink_start_of_21 - ink_end_of_20 - 1;
}

static void numbers_leave_a_clear_gap_between_neighbouring_cells(void) {
    /* The smallest cell each text size supports, one pixel short of the next
     * size, and one comfortably inside each band. */
    static const int SIZES[][2] = {{14, 8}, {27, 16}, {28, 16}, {41, 24}, {42, 24}, {50, 30}};
    for (int i = 0; i < (int)(sizeof SIZES / sizeof SIZES[0]); i++) {
        int between_digits, between_numbers;
        measure_gaps(SIZES[i][0], SIZES[i][1], &between_digits, &between_numbers);
        CHECK(between_digits > 0);
        CHECK(between_numbers >= NUMBER_TO_DIGIT_GAP_RATIO * between_digits);
    }
}

/* A number sits in the middle of its cell: the blank space left of its ink
 * and right of its ink differ by at most one pixel. Checked on the 20th, whose
 * two digits are both full-width glyphs, so the ink fills its whole box. */
static void numbers_are_centred_in_their_cells(void) {
    static const int SIZES[][2] = {{14, 8}, {28, 16}, {30, 17}, {42, 24}, {50, 30}};
    LocalTime now = on(2026, 10, 1);
    MonthCells m;
    CHECK(month_calendar_cells(&now, &m) == 1);
    for (int i = 0; i < (int)(sizeof SIZES / sizeof SIZES[0]); i++) {
        int cw = SIZES[i][0], ch = SIZES[i][1];
        int y0 = ORIGIN + (m.row[20] + 1) * ch, x0 = ORIGIN + m.column[20] * cw;
        int first = -1, last = -1;
        clear(&fb);
        month_calendar_draw(&fb, ORIGIN, ORIGIN, cw, ch, &now);
        for (int x = x0; x < x0 + cw; x++)
            if (column_has_ink(x, y0, y0 + ch)) {
                if (first < 0) first = x;
                last = x;
            }
        CHECK(first >= 0);
        CHECK(abs((first - x0) - (x0 + cw - 1 - last)) <= 1);
    }
}

/* Golden images: October 2026 in the small size (30x17 cells, lower right of
 * the screen) and in a large size (42x28 cells). Written once and approved by
 * eye; regenerate with `make update-golden` and review the images before
 * committing. */
static void the_calendar_matches_the_golden_images(void) {
    const char *small_path = "tests/golden/month_calendar.ppm";
    const char *large_path = "tests/golden/month_calendar_large.ppm";
    LocalTime now = on(2026, 10, 17);
    clear(&fb);
    clear(&other);
    month_calendar_draw(&fb, GX, GY, CW, CH, &now);
    month_calendar_draw(&other, 8, 8, 42, 28, &now);
    if (getenv("UPDATE_GOLDEN")) {
        CHECK(ppm_write(small_path, &fb));
        CHECK(ppm_write(large_path, &other));
    }
    CHECK(ppm_matches(small_path, &fb));
    CHECK(ppm_matches(large_path, &other));
}

int main(void) {
    RUN(october_2026_starts_on_a_thursday_and_needs_five_rows);
    RUN(february_2024_has_a_leap_day);
    RUN(february_2026_has_28_days_and_starts_on_a_sunday);
    RUN(a_month_can_fit_exactly_four_rows);
    RUN(some_months_need_six_rows);
    RUN(days_follow_each_other_one_cell_at_a_time);
    RUN(a_missing_or_impossible_date_gives_no_layout);
    RUN(the_header_shows_the_weekday_initials);
    RUN(every_day_has_a_cell_and_the_other_cells_stay_empty);
    RUN(today_is_a_white_cell_with_black_digits);
    RUN(a_number_looks_the_same_wherever_it_sits);
    RUN(nothing_is_drawn_outside_the_grid);
    RUN(a_missing_or_impossible_date_draws_only_the_header);
    RUN(numbers_leave_a_clear_gap_between_neighbouring_cells);
    RUN(numbers_are_centred_in_their_cells);
    RUN(the_calendar_matches_the_golden_images);
    return mt_report();
}
