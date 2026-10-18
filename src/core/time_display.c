#include "time_display.h"

enum { SEG_A = 1, SEG_B = 2, SEG_C = 4, SEG_D = 8, SEG_E = 16, SEG_F = 32, SEG_G = 64 };

/* Lit segments per digit 0-9 (a..g = top, upper right, lower right, bottom,
 * lower left, upper left, middle). */
static const unsigned char DIGIT_SEGMENTS[10] = {
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
    SEG_B | SEG_C,
    SEG_A | SEG_B | SEG_D | SEG_E | SEG_G,
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_G,
    SEG_B | SEG_C | SEG_F | SEG_G,
    SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,
    SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,
    SEG_A | SEG_B | SEG_C,
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,
};

/* Space between neighbouring digits, and the width of the colon's column
 * beyond that (twice the segment thickness, so the dots scale with the
 * digits). */
#define DIGIT_GAP 8

static int colon_width(const TimeDisplayShape *shape) { return 2 * shape->thickness; }

void time_display_cells(const TimeDisplayShape *shape, DigitCell cells[TD_DIGIT_COUNT]) {
    int pair = 2 * shape->digit_w + DIGIT_GAP;
    int colon_gap = DIGIT_GAP + colon_width(shape);
    int left = (FB_WIDTH - (2 * pair + colon_gap)) / 2;
    int x[TD_DIGIT_COUNT];

    x[0] = left;
    x[1] = x[0] + shape->digit_w + DIGIT_GAP;
    x[2] = x[1] + shape->digit_w + colon_gap;
    x[3] = x[2] + shape->digit_w + DIGIT_GAP;
    for (int i = 0; i < TD_DIGIT_COUNT; i++) {
        DigitCell c = {x[i], shape->top, shape->digit_w, shape->digit_h, shape->thickness,
                       shape->corner_gap};
        cells[i] = c;
    }
}

static void put(Framebuffer *fb, int x, int y, Pixel color) {
    fb->pixels[y * FB_WIDTH + x] = color;
}

/* How far a six-cornered bar is cut back from its tip at row (or column) i
 * of its thickness t: 0 along the middle, growing towards the edges so the
 * ends come to a 45 degree point. */
static int taper(int i, int t) {
    int d = 2 * i + 1 - t;
    return (d < 0 ? -d : d) / 2;
}

/* Horizontal bar whose tips are the pixels x0 and x0 + len - 1, centred on
 * row `centre`. */
static void hex_bar_horizontal(Framebuffer *fb, int x0, int centre, int len, int t, Pixel color) {
    for (int i = 0; i < t; i++) {
        int d = taper(i, t);
        for (int x = x0 + d; x < x0 + len - d; x++) put(fb, x, centre - t / 2 + i, color);
    }
}

/* Vertical bar whose tips are the pixels y0 and y0 + len - 1, centred on
 * column `centre`. */
static void hex_bar_vertical(Framebuffer *fb, int centre, int y0, int len, int t, Pixel color) {
    for (int j = 0; j < t; j++) {
        int d = taper(j, t);
        for (int y = y0 + d; y < y0 + len - d; y++) put(fb, centre - t / 2 + j, y, color);
    }
}

/* Each segment runs between two corner points of the digit and is pulled
 * back by g from both, so neighbours never touch. */
static void draw_segment(Framebuffer *fb, const DigitCell *c, int seg, Pixel color) {
    int t = c->t, g = c->g;
    int left = c->x + t / 2, right = c->x + c->w - t + t / 2;
    int top = c->y + t / 2, bottom = c->y + c->h - t + t / 2, middle = c->y + c->h / 2;
    int across = right - left - 2 * g + 1;
    int upper = middle - top - 2 * g + 1, lower = bottom - middle - 2 * g + 1; /* vertical bars */
    switch (seg) {
    case SEG_A: hex_bar_horizontal(fb, left + g, top, across, t, color); break;
    case SEG_G: hex_bar_horizontal(fb, left + g, middle, across, t, color); break;
    case SEG_D: hex_bar_horizontal(fb, left + g, bottom, across, t, color); break;
    case SEG_F: hex_bar_vertical(fb, left, top + g, upper, t, color); break;
    case SEG_B: hex_bar_vertical(fb, right, top + g, upper, t, color); break;
    case SEG_E: hex_bar_vertical(fb, left, middle + g, lower, t, color); break;
    case SEG_C: hex_bar_vertical(fb, right, middle + g, lower, t, color); break;
    }
}

static void draw_cell(Framebuffer *fb, const DigitCell *c, int lit_mask) {
    /* Segments that are not lit are not drawn. */
    for (int seg = SEG_A; seg <= SEG_G; seg <<= 1)
        if (lit_mask & seg) draw_segment(fb, c, seg, PIXEL_WHITE);
}

/* Stands for a value that is not there; draw_pair shows it as dashes. */
#define MISSING_VALUE (-1)

/* Draws a two-digit value, or a dash in both cells when it is out of range. */
static void draw_pair(Framebuffer *fb, const DigitCell *cells, int value, int max) {
    int valid = value >= 0 && value <= max;
    draw_cell(fb, &cells[0], valid ? DIGIT_SEGMENTS[value / 10] : SEG_G);
    draw_cell(fb, &cells[1], valid ? DIGIT_SEGMENTS[value % 10] : SEG_G);
}

static void draw_colon(Framebuffer *fb, const TimeDisplayShape *shape, const DigitCell *cells) {
    int dot = shape->thickness;
    int gap_left = cells[1].x + cells[1].w;
    int x = gap_left + (DIGIT_GAP + colon_width(shape) - dot) / 2;
    int h = shape->digit_h;
    fb_fill_rect(fb, x, shape->top + h / 3 - dot / 2, dot, dot, PIXEL_WHITE);
    fb_fill_rect(fb, x, shape->top + 2 * h / 3 - dot / 2, dot, dot, PIXEL_WHITE);
}

void time_display_draw(Framebuffer *fb, const TimeDisplayShape *shape, const LocalTime *now) {
    DigitCell cells[TD_DIGIT_COUNT];
    int hour = now ? now->hour : MISSING_VALUE;
    int minute = now ? now->minute : MISSING_VALUE;

    time_display_cells(shape, cells);
    draw_pair(fb, &cells[0], hour, 23);
    draw_pair(fb, &cells[2], minute, 59);
    draw_colon(fb, shape, cells);
}
