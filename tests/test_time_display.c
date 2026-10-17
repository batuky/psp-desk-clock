#include <stdlib.h>
#include <string.h>

#include "minitest.h"
#include "ppm.h"
#include "time_display.h"

static Framebuffer fb;

/* Start every render from garbage so a test can't pass on leftovers. */
static void render(int h, int m, int s) {
    memset(&fb, 0x5A, sizeof fb);
    time_display_render(&fb, h, m, s);
}

static Pixel at(int x, int y) { return fb.pixels[y * FB_WIDTH + x]; }

static void background_is_black(void) {
    render(0, 0, 0);
    CHECK_HEX(at(0, 0), 0xFF000000);
    CHECK_HEX(at(FB_WIDTH - 1, FB_HEIGHT - 1), 0xFF000000);
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
    const DigitCell *c = &time_display_cells()[cell];
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
        render(h, 0, 0);
        expect_digits(0, h);
    }
}

static void minutes_show_their_digits(void) {
    for (int m = 0; m < 60; m++) {
        render(0, m, 0);
        expect_digits(2, m);
    }
}

static void seconds_show_their_digits(void) {
    for (int s = 0; s < 60; s++) {
        render(0, 0, s);
        expect_digits(4, s);
    }
}

static void out_of_range_components_show_a_dash(void) {
    /* Each bad component becomes a dash; good ones next to it still render. */
    render(24, 34, 56);
    expect_cell(0, DASH);
    expect_cell(1, DASH);
    expect_digits(2, 34);
    expect_digits(4, 56);

    render(-1, 60, 7);
    expect_cell(0, DASH);
    expect_cell(1, DASH);
    expect_cell(2, DASH);
    expect_cell(3, DASH);
    expect_digits(4, 7);

    render(12, 34, 100);
    expect_digits(0, 12);
    expect_digits(2, 34);
    expect_cell(4, DASH);
    expect_cell(5, DASH);
}

static void seconds_are_smaller_and_share_the_bottom_edge(void) {
    const DigitCell *c = time_display_cells();
    CHECK(c[4].w < c[0].w && c[4].h < c[0].h);
    CHECK(c[5].w == c[4].w && c[5].h == c[4].h);
    CHECK(c[4].y + c[4].h == c[0].y + c[0].h);
}

/* The Time display spans the top of the screen, centred, and leaves the
 * lower half free for the Date line and the Month calendar. */
static void time_display_spans_the_top_and_leaves_the_bottom_free(void) {
    const DigitCell *c = time_display_cells();
    int left = c[0].x, right = c[TD_DIGIT_COUNT - 1].x + c[TD_DIGIT_COUNT - 1].w;
    CHECK(left >= 8);
    CHECK(FB_WIDTH - right >= 8);
    CHECK(left - (FB_WIDTH - right) <= 1 && (FB_WIDTH - right) - left <= 1); /* centred */
    CHECK(right - left >= FB_WIDTH * 9 / 10 - 40);                           /* wide */
    for (int i = 0; i < TD_DIGIT_COUNT; i++) {
        CHECK(c[i].y >= 0);
        CHECK(c[i].y + c[i].h <= FB_HEIGHT / 2 - 8); /* top half only */
    }
    CHECK(c[0].w >= 64 && c[0].h >= 100);            /* big enough to read across a desk */
}

static void colon_separates_hours_from_minutes(void) {
    const DigitCell *c = time_display_cells();
    int x = (c[1].x + c[1].w + c[2].x) / 2;
    render(12, 34, 56);
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
    const DigitCell *c = &time_display_cells()[3];
    int top_first, top_last, mid_first, mid_last;
    render(0, 8, 0);
    row_extent(c, c->y, &top_first, &top_last);                /* edge row */
    row_extent(c, c->y + c->t / 2, &mid_first, &mid_last);     /* centre row */
    CHECK(top_first >= 0);
    CHECK(mid_first < top_first);
    CHECK(mid_last > top_last);
}

static void vertical_segments_taper_to_a_point(void) {
    const DigitCell *c = &time_display_cells()[3];
    int edge_x = c->x + c->w - 1;                       /* outer edge column */
    int centre_x = c->x + c->w - c->t + c->t / 2;       /* bar's centre column */
    int edge_first, edge_last, centre_first, centre_last;
    render(0, 8, 0);
    /* Only the lower half of the upper-right bar: clear of the top bar, so
     * the scan sees this one segment's end. */
    column_extent(edge_x, c->y + c->h / 4, c->y + c->h / 2, &edge_first, &edge_last);
    column_extent(centre_x, c->y + c->h / 4, c->y + c->h / 2, &centre_first, &centre_last);
    CHECK(edge_last >= 0);
    CHECK(centre_last > edge_last);
}

static void neighbouring_segments_leave_the_corner_open(void) {
    const DigitCell *c = &time_display_cells()[3];
    int left = c->x + c->t / 2, right = c->x + c->w - c->t + c->t / 2;
    int top = c->y + c->t / 2, bottom = c->y + c->h - c->t + c->t / 2;
    render(0, 8, 0);
    CHECK_HEX(at(left, top), 0xFF000000);
    CHECK_HEX(at(right, top), 0xFF000000);
    CHECK_HEX(at(left, bottom), 0xFF000000);
    CHECK_HEX(at(right, bottom), 0xFF000000);
}

/* Golden image of 12:34:56. Written once and approved by eye; regenerate
 * with `make update-golden` and review the image before committing. */
static void time_display_matches_the_golden_image(void) {
    const char *path = "tests/golden/time_12_34_56.ppm";
    render(12, 34, 56);
    if (getenv("UPDATE_GOLDEN")) CHECK(ppm_write(path, &fb));
    CHECK(ppm_matches(path, &fb));
}

int main(void) {
    RUN(background_is_black);
    RUN(hours_show_their_digits);
    RUN(minutes_show_their_digits);
    RUN(seconds_show_their_digits);
    RUN(out_of_range_components_show_a_dash);
    RUN(seconds_are_smaller_and_share_the_bottom_edge);
    RUN(time_display_spans_the_top_and_leaves_the_bottom_free);
    RUN(colon_separates_hours_from_minutes);
    RUN(horizontal_segments_taper_to_a_point);
    RUN(vertical_segments_taper_to_a_point);
    RUN(neighbouring_segments_leave_the_corner_open);
    RUN(time_display_matches_the_golden_image);
    return mt_report();
}
