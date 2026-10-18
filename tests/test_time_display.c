#include <string.h>

#include "minitest.h"
#include "time_display.h"

static Framebuffer fb;

/* The two shapes the Clock face uses (layouts B and D), spelled out here so
 * the tests do not depend on the compositor. */
static const TimeDisplayShape SHAPE_B = {84, 130, 12, 3, 12};
static const TimeDisplayShape SHAPE_D = {80, 120, 11, 3, 8};

static const TimeDisplayShape *shape = &SHAPE_B;
static DigitCell cells[TD_DIGIT_COUNT];

static void use_shape(const TimeDisplayShape *which) {
    shape = which;
    time_display_cells(shape, cells);
}

/* Draws onto a black screen. The seconds vary between renders only to prove
 * they are never shown. */
static void render(int h, int m) {
    LocalTime now = {2026, 10, 17, h, m, (h * 7 + m * 13) % 60};
    fb_clear(&fb, PIXEL_BLACK);
    time_display_draw(&fb, shape, &now);
}

static Pixel at(int x, int y) { return fb.pixels[y * FB_WIDTH + x]; }

static void drawing_leaves_the_rest_of_the_screen_alone(void) {
    LocalTime now = {2026, 10, 17, 12, 34, 56};
    memset(&fb, 0x5A, sizeof fb);
    time_display_draw(&fb, shape, &now);
    CHECK_HEX(at(0, 0), 0x5A5A5A5A);
    CHECK_HEX(at(FB_WIDTH - 1, FB_HEIGHT - 1), 0x5A5A5A5A);
}

/* Independent source of truth: the digits as the familiar seven-segment
 * ASCII art, parsed into segment masks. 6 and 9 keep their tails; 7 has no
 * upper-left bar. Segment bits: a top, b upper right, c lower right,
 * d bottom, e lower left, f upper left, g middle. */
enum { A = 1, B = 2, C = 4, D = 8, E = 16, F = 32, G = 64 };
#define DASH G

static const char *const GLYPHS[10][3] = {
    {" _ ", "| |", "|_|"}, /* 0 */
    {"   ", "  |", "  |"}, /* 1 */
    {" _ ", " _|", "|_ "}, /* 2 */
    {" _ ", " _|", " _|"}, /* 3 */
    {"   ", "|_|", "  |"}, /* 4 */
    {" _ ", "|_ ", " _|"}, /* 5 */
    {" _ ", "|_ ", "|_|"}, /* 6 */
    {" _ ", "  |", "  |"}, /* 7 */
    {" _ ", "|_|", "|_|"}, /* 8 */
    {" _ ", "|_|", " _|"}, /* 9 */
};

static int glyph_mask(int digit) {
    const char *const *g = GLYPHS[digit];
    int mask = 0;
    if (g[0][1] == '_') mask |= A;
    if (g[1][2] == '|') mask |= B;
    if (g[2][2] == '|') mask |= C;
    if (g[2][1] == '_') mask |= D;
    if (g[2][0] == '|') mask |= E;
    if (g[1][0] == '|') mask |= F;
    if (g[1][1] == '_') mask |= G;
    return mask;
}

/* Centre of each segment inside a cell, from the cell's own geometry. */
static void segment_probe(const DigitCell *c, int seg, int *px, int *py) {
    int x = c->x, y = c->y, w = c->w, h = c->h, t = c->t;
    switch (seg) {
    case A: *px = x + w / 2;     *py = y + t / 2;         break;
    case B: *px = x + w - t / 2; *py = y + h / 4;         break;
    case C: *px = x + w - t / 2; *py = y + 3 * h / 4;     break;
    case D: *px = x + w / 2;     *py = y + h - 1 - t / 2; break;
    case E: *px = x + t / 2;     *py = y + 3 * h / 4;     break;
    case F: *px = x + t / 2;     *py = y + h / 4;         break;
    default: *px = x + w / 2;    *py = y + h / 2;         break; /* G */
    }
}

/* Lit segments are exactly white; a segment that is not lit is not drawn,
 * so its centre stays black. */
static void expect_cell(int cell, int lit_mask) {
    const DigitCell *c = &cells[cell];
    for (int seg = A; seg <= G; seg <<= 1) {
        int px, py;
        segment_probe(c, seg, &px, &py);
        if (lit_mask & seg)
            CHECK_HEX(at(px, py), 0xFFFFFFFF);
        else
            CHECK_HEX(at(px, py), 0xFF000000);
    }
}

static void expect_digits(int cell, int value) {
    expect_cell(cell, glyph_mask(value / 10));
    expect_cell(cell + 1, glyph_mask(value % 10));
}

static void hours_show_their_digits(void) {
    for (int h = 0; h < 24; h++) {
        render(h, 0);
        expect_digits(0, h);
    }
}

static void minutes_show_their_digits(void) {
    for (int m = 0; m < 60; m++) {
        render(0, m);
        expect_digits(2, m);
    }
}

static void out_of_range_components_show_a_dash(void) {
    LocalTime now = {2026, 10, 17, 24, 34, 56};
    /* Each bad component becomes a dash; a good one next to it still renders. */
    fb_clear(&fb, PIXEL_BLACK);
    time_display_draw(&fb, shape, &now);
    expect_cell(0, DASH);
    expect_cell(1, DASH);
    expect_digits(2, 34);

    now.hour = -1;
    now.minute = 60;
    fb_clear(&fb, PIXEL_BLACK);
    time_display_draw(&fb, shape, &now);
    for (int cell = 0; cell < TD_DIGIT_COUNT; cell++) expect_cell(cell, DASH);

    now.hour = 12;
    now.minute = 100;
    fb_clear(&fb, PIXEL_BLACK);
    time_display_draw(&fb, shape, &now);
    expect_digits(0, 12);
    expect_cell(2, DASH);
    expect_cell(3, DASH);
}

static void an_unreadable_clock_shows_a_dash_in_every_cell(void) {
    fb_clear(&fb, PIXEL_BLACK);
    time_display_draw(&fb, shape, NULL);
    for (int cell = 0; cell < TD_DIGIT_COUNT; cell++) expect_cell(cell, DASH);
}

/* The seconds are not displayed: changing them changes nothing. */
static void seconds_are_not_shown(void) {
    static Framebuffer first;
    render(12, 34);
    first = fb;
    for (int sec = 0; sec < 60; sec++) {
        LocalTime now = {2026, 10, 17, 12, 34, sec};
        fb_clear(&fb, PIXEL_BLACK);
        time_display_draw(&fb, shape, &now);
        CHECK(memcmp(&fb, &first, sizeof fb) == 0);
    }
}

static void cells_follow_the_shape_and_share_one_row(void) {
    for (int i = 0; i < TD_DIGIT_COUNT; i++) {
        CHECK(cells[i].w == shape->digit_w && cells[i].h == shape->digit_h);
        CHECK(cells[i].t == shape->thickness && cells[i].g == shape->corner_gap);
        CHECK(cells[i].y == shape->top);
    }
}

/* The digits are centred across the screen, in reading order without
 * overlap, and stay on the screen. */
static void digits_are_centred_in_order_and_on_screen(void) {
    int left = cells[0].x, right = cells[TD_DIGIT_COUNT - 1].x + cells[TD_DIGIT_COUNT - 1].w;
    CHECK(left >= 0 && right <= FB_WIDTH);
    CHECK(left - (FB_WIDTH - right) <= 1 && (FB_WIDTH - right) - left <= 1);
    CHECK(shape->top >= 0 && shape->top + shape->digit_h <= FB_HEIGHT);
    for (int i = 1; i < TD_DIGIT_COUNT; i++) CHECK(cells[i].x >= cells[i - 1].x + cells[i - 1].w);
}

static void colon_separates_hours_from_minutes(void) {
    const DigitCell *c = cells;
    int x = (c[1].x + c[1].w + c[2].x) / 2;
    render(12, 34);
    CHECK_HEX(at(x, c[0].y + c[0].h / 3), 0xFFFFFFFF);
    CHECK_HEX(at(x, c[0].y + 2 * c[0].h / 3), 0xFFFFFFFF);
    CHECK_HEX(at(x, c[0].y + c[0].h / 2), 0xFF000000); /* gap between dots */
}

/* Segments are six-cornered, like an LED wall clock: each bar is widest at
 * its middle and tapers to a point at both ends, and neighbouring bars
 * leave the corner where they meet open. Checked on an '8', which lights
 * every segment. */
static int is_lit(int x, int y) { return at(x, y) == PIXEL_WHITE; }

/* First and last lit x in row y within the cell, or -1 when none. */
static void row_extent(const DigitCell *c, int y, int *first, int *last) {
    *first = *last = -1;
    for (int x = c->x; x < c->x + c->w; x++)
        if (is_lit(x, y)) {
            if (*first < 0) *first = x;
            *last = x;
        }
}

/* First and last lit y in column x within y0..y1 (exclusive), or -1. */
static void column_extent(int x, int y0, int y1, int *first, int *last) {
    *first = *last = -1;
    for (int y = y0; y < y1; y++)
        if (is_lit(x, y)) {
            if (*first < 0) *first = y;
            *last = y;
        }
}

static void horizontal_segments_taper_to_a_point(void) {
    const DigitCell *c = &cells[3];
    int top_first, top_last, mid_first, mid_last;
    render(0, 8);
    row_extent(c, c->y, &top_first, &top_last);                /* edge row */
    row_extent(c, c->y + c->t / 2, &mid_first, &mid_last);     /* centre row */
    CHECK(top_first >= 0);
    CHECK(mid_first < top_first);
    CHECK(mid_last > top_last);
}

static void vertical_segments_taper_to_a_point(void) {
    const DigitCell *c = &cells[3];
    int edge_x = c->x + c->w - 1;                       /* outer edge column */
    int centre_x = c->x + c->w - c->t + c->t / 2;       /* bar's centre column */
    int edge_first, edge_last, centre_first, centre_last;
    render(0, 8);
    /* Only the lower half of the upper-right bar: clear of the top bar, so
     * the scan sees this one segment's end. */
    column_extent(edge_x, c->y + c->h / 4, c->y + c->h / 2, &edge_first, &edge_last);
    column_extent(centre_x, c->y + c->h / 4, c->y + c->h / 2, &centre_first, &centre_last);
    CHECK(edge_last >= 0);
    CHECK(centre_last > edge_last);
}

static void neighbouring_segments_leave_the_corner_open(void) {
    const DigitCell *c = &cells[3];
    int left = c->x + c->t / 2, right = c->x + c->w - c->t + c->t / 2;
    int top = c->y + c->t / 2, bottom = c->y + c->h - c->t + c->t / 2;
    render(0, 8);
    CHECK_HEX(at(left, top), 0xFF000000);
    CHECK_HEX(at(right, top), 0xFF000000);
    CHECK_HEX(at(left, bottom), 0xFF000000);
    CHECK_HEX(at(right, bottom), 0xFF000000);
}

/* Every check above holds for both shapes of the Clock face. */
static void run_all(const TimeDisplayShape *which) {
    use_shape(which);
    RUN(drawing_leaves_the_rest_of_the_screen_alone);
    RUN(hours_show_their_digits);
    RUN(minutes_show_their_digits);
    RUN(out_of_range_components_show_a_dash);
    RUN(an_unreadable_clock_shows_a_dash_in_every_cell);
    RUN(seconds_are_not_shown);
    RUN(cells_follow_the_shape_and_share_one_row);
    RUN(digits_are_centred_in_order_and_on_screen);
    RUN(colon_separates_hours_from_minutes);
    RUN(horizontal_segments_taper_to_a_point);
    RUN(vertical_segments_taper_to_a_point);
    RUN(neighbouring_segments_leave_the_corner_open);
}

int main(void) {
    puts("-- shape of layout B");
    run_all(&SHAPE_B);
    puts("-- shape of layout D");
    run_all(&SHAPE_D);
    return mt_report();
}
