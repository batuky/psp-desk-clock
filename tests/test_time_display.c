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

/* Independent source of truth: the standard seven-segment alphabet.
 * Bit order a..g = top, upper right, lower right, bottom, lower left,
 * upper left, middle. 6 and 9 keep their tails; 7 has no upper-left bar. */
enum { A = 1, B = 2, C = 4, D = 8, E = 16, F = 32, G = 64 };
static const int DIGIT_SEGMENTS[10] = {
    A | B | C | D | E | F,      /* 0 */
    B | C,                      /* 1 */
    A | B | D | E | G,          /* 2 */
    A | B | C | D | G,          /* 3 */
    B | C | F | G,              /* 4 */
    A | C | D | F | G,          /* 5 */
    A | C | D | E | F | G,      /* 6 */
    A | B | C,                  /* 7 */
    A | B | C | D | E | F | G,  /* 8 */
    A | B | C | D | F | G,      /* 9 */
};
#define DASH G

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

/* Lit segments are exactly white; Ghost segments are a faint grey: not
 * black, not white, equal channels, dim. */
static int is_ghost(Pixel p) {
    uint32_t r = p & 0xFF, g = (p >> 8) & 0xFF, b = (p >> 16) & 0xFF;
    return (p >> 24) == 0xFF && r == g && g == b && r > 0 && r <= 0x40;
}

static void expect_cell(int cell, int lit_mask) {
    const DigitCell *c = &time_display_cells()[cell];
    for (int seg = A; seg <= G; seg <<= 1) {
        int px, py;
        segment_probe(c, seg, &px, &py);
        if (lit_mask & seg)
            CHECK_HEX(at(px, py), 0xFFFFFFFF);
        else
            CHECK(is_ghost(at(px, py)));
    }
}

static void expect_digits(int cell, int value) {
    expect_cell(cell, DIGIT_SEGMENTS[value / 10]);
    expect_cell(cell + 1, DIGIT_SEGMENTS[value % 10]);
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

static void time_display_leaves_the_right_side_free(void) {
    const DigitCell *c = time_display_cells();
    for (int i = 0; i < TD_DIGIT_COUNT; i++) {
        CHECK(c[i].x >= 0 && c[i].y >= 0);
        CHECK(c[i].x + c[i].w <= 300); /* Month calendar needs ~170 px */
        CHECK(c[i].y + c[i].h <= FB_HEIGHT);
    }
}

static void colon_separates_hours_from_minutes(void) {
    const DigitCell *c = time_display_cells();
    int x = (c[1].x + c[1].w + c[2].x) / 2;
    render(12, 34, 56);
    CHECK_HEX(at(x, c[0].y + c[0].h / 3), 0xFFFFFFFF);
    CHECK_HEX(at(x, c[0].y + 2 * c[0].h / 3), 0xFFFFFFFF);
    CHECK_HEX(at(x, c[0].y + c[0].h / 2), 0xFF000000); /* gap between dots */
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
    RUN(time_display_leaves_the_right_side_free);
    RUN(colon_separates_hours_from_minutes);
    RUN(time_display_matches_the_golden_image);
    return mt_report();
}
