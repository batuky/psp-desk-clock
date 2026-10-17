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

/* The Time display spans the top of the screen, centred. Hours and minutes
 * are big; seconds are smaller and share the bottom edge. The lower half of
 * the screen is left for the Date line and the Month calendar. */
#define BIG_W 70
#define BIG_H 110
#define BIG_T 10 /* segment thickness */
#define BIG_G 3  /* gap at shared corners */
#define SMALL_W 44
#define SMALL_H 72
#define SMALL_T 6
#define SMALL_G 2
#define TOP 8
#define DIGIT_GAP 8
#define COLON_W 24
#define COLON_DOT 12
#define SECONDS_GAP 14
#define SMALL_GAP 8

#define CLOCK_W (4 * BIG_W + 2 * DIGIT_GAP + (DIGIT_GAP + COLON_W) + SECONDS_GAP + \
                 2 * SMALL_W + SMALL_GAP)
#define LEFT ((FB_WIDTH - CLOCK_W) / 2)

/* Left edge of each digit cell, in reading order. */
#define X_HOUR_TENS LEFT
#define X_HOUR_UNITS (X_HOUR_TENS + BIG_W + DIGIT_GAP)
#define X_MINUTE_TENS (X_HOUR_UNITS + BIG_W + DIGIT_GAP + COLON_W)
#define X_MINUTE_UNITS (X_MINUTE_TENS + BIG_W + DIGIT_GAP)
#define X_SECOND_TENS (X_MINUTE_UNITS + BIG_W + SECONDS_GAP)
#define X_SECOND_UNITS (X_SECOND_TENS + SMALL_W + SMALL_GAP)
#define SMALL_TOP (TOP + BIG_H - SMALL_H)

#define BIG_CELL(x) {(x), TOP, BIG_W, BIG_H, BIG_T, BIG_G}
#define SMALL_CELL(x) {(x), SMALL_TOP, SMALL_W, SMALL_H, SMALL_T, SMALL_G}

const DigitCell *time_display_cells(void) {
    static const DigitCell cells[TD_DIGIT_COUNT] = {
        BIG_CELL(X_HOUR_TENS),     BIG_CELL(X_HOUR_UNITS),
        BIG_CELL(X_MINUTE_TENS),   BIG_CELL(X_MINUTE_UNITS),
        SMALL_CELL(X_SECOND_TENS), SMALL_CELL(X_SECOND_UNITS),
    };
    return cells;
}

static void put(Framebuffer *fb, int x, int y, Pixel color) {
    fb->pixels[y * FB_WIDTH + x] = color;
}

static void fill_rect(Framebuffer *fb, int x, int y, int w, int h, Pixel color) {
    for (int row = y; row < y + h; row++)
        for (int col = x; col < x + w; col++) put(fb, col, row, color);
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
    switch (seg) {
    case SEG_A: hex_bar_horizontal(fb, left + g, top, across, t, color); break;
    case SEG_G: hex_bar_horizontal(fb, left + g, middle, across, t, color); break;
    case SEG_D: hex_bar_horizontal(fb, left + g, bottom, across, t, color); break;
    case SEG_F: hex_bar_vertical(fb, left, top + g, middle - top - 2 * g + 1, t, color); break;
    case SEG_B: hex_bar_vertical(fb, right, top + g, middle - top - 2 * g + 1, t, color); break;
    case SEG_E: hex_bar_vertical(fb, left, middle + g, bottom - middle - 2 * g + 1, t, color); break;
    case SEG_C: hex_bar_vertical(fb, right, middle + g, bottom - middle - 2 * g + 1, t, color); break;
    }
}

static void draw_cell(Framebuffer *fb, const DigitCell *c, int lit_mask) {
    /* Segments that are not lit are not drawn. */
    for (int seg = SEG_A; seg <= SEG_G; seg <<= 1)
        if (lit_mask & seg) draw_segment(fb, c, seg, PIXEL_WHITE);
}

/* Draws a two-digit value, or a dash in both cells when it is out of range. */
static void draw_pair(Framebuffer *fb, int first_cell, int value, int max) {
    const DigitCell *cells = time_display_cells();
    int valid = value >= 0 && value <= max;
    draw_cell(fb, &cells[first_cell], valid ? DIGIT_SEGMENTS[value / 10] : SEG_G);
    draw_cell(fb, &cells[first_cell + 1], valid ? DIGIT_SEGMENTS[value % 10] : SEG_G);
}

static void draw_colon(Framebuffer *fb) {
    const DigitCell *cells = time_display_cells();
    int gap_left = cells[1].x + cells[1].w;
    int x = gap_left + (DIGIT_GAP + COLON_W - COLON_DOT) / 2;
    fill_rect(fb, x, TOP + BIG_H / 3 - COLON_DOT / 2, COLON_DOT, COLON_DOT, PIXEL_WHITE);
    fill_rect(fb, x, TOP + 2 * BIG_H / 3 - COLON_DOT / 2, COLON_DOT, COLON_DOT, PIXEL_WHITE);
}

void time_display_render(Framebuffer *fb, const LocalTime *now) {
    for (int i = 0; i < FB_WIDTH * FB_HEIGHT; i++) fb->pixels[i] = PIXEL_BLACK;
    draw_pair(fb, 0, now ? now->hour : -1, 23);
    draw_pair(fb, 2, now ? now->minute : -1, 59);
    draw_pair(fb, 4, now ? now->second : -1, 59);
    draw_colon(fb);
}
