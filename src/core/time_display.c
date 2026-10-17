#include "time_display.h"

#define GHOST ((Pixel)0xFF2A2A2Au)
#define SEGMENT_GAP 2 /* empty pixels between neighbouring segments */

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

/* Hours and minutes are big, seconds are half size and share the bottom
 * edge. Everything sits in the left part of the screen, leaving the right
 * side for the Month calendar. */
#define BIG_W 48
#define BIG_H 88
#define BIG_T 10
#define SMALL_W 24
#define SMALL_H 44
#define SMALL_T 5
#define TOP 32
#define LEFT 8
#define DIGIT_GAP 6
#define COLON_W 16
#define SECONDS_GAP 12
#define SMALL_GAP 6
#define COLON_DOT 10

const DigitCell *time_display_cells(void) {
    static DigitCell cells[TD_DIGIT_COUNT];
    static int ready;
    if (!ready) {
        int x = LEFT;
        int small_y = TOP + BIG_H - SMALL_H;
        cells[0] = (DigitCell){x, TOP, BIG_W, BIG_H, BIG_T};
        x += BIG_W + DIGIT_GAP;
        cells[1] = (DigitCell){x, TOP, BIG_W, BIG_H, BIG_T};
        x += BIG_W + DIGIT_GAP + COLON_W;
        cells[2] = (DigitCell){x, TOP, BIG_W, BIG_H, BIG_T};
        x += BIG_W + DIGIT_GAP;
        cells[3] = (DigitCell){x, TOP, BIG_W, BIG_H, BIG_T};
        x += BIG_W + SECONDS_GAP;
        cells[4] = (DigitCell){x, small_y, SMALL_W, SMALL_H, SMALL_T};
        x += SMALL_W + SMALL_GAP;
        cells[5] = (DigitCell){x, small_y, SMALL_W, SMALL_H, SMALL_T};
        ready = 1;
    }
    return cells;
}

static void fill_rect(Framebuffer *fb, int x, int y, int w, int h, Pixel color) {
    for (int row = y; row < y + h; row++)
        for (int col = x; col < x + w; col++)
            fb->pixels[row * FB_WIDTH + col] = color;
}

static void draw_segment(Framebuffer *fb, const DigitCell *c, int seg, Pixel color) {
    int x = c->x, y = c->y, w = c->w, h = c->h, t = c->t, g = SEGMENT_GAP;
    int mid = y + (h - t) / 2; /* top edge of the middle bar */
    switch (seg) {
    case SEG_A: fill_rect(fb, x + g, y, w - 2 * g, t, color); break;
    case SEG_G: fill_rect(fb, x + g, mid, w - 2 * g, t, color); break;
    case SEG_D: fill_rect(fb, x + g, y + h - t, w - 2 * g, t, color); break;
    case SEG_F: fill_rect(fb, x, y + t + g, t, mid - (y + t + g) - g, color); break;
    case SEG_B: fill_rect(fb, x + w - t, y + t + g, t, mid - (y + t + g) - g, color); break;
    case SEG_E: fill_rect(fb, x, mid + t + g, t, (y + h - t - g) - (mid + t + g), color); break;
    case SEG_C: fill_rect(fb, x + w - t, mid + t + g, t, (y + h - t - g) - (mid + t + g), color); break;
    }
}

static void draw_cell(Framebuffer *fb, const DigitCell *c, int lit_mask) {
    for (int seg = SEG_A; seg <= SEG_G; seg <<= 1)
        draw_segment(fb, c, seg, GHOST);
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
    int left = cells[1].x + cells[1].w;
    int x = left + (DIGIT_GAP + COLON_W - COLON_DOT) / 2;
    fill_rect(fb, x, TOP + BIG_H / 3 - COLON_DOT / 2, COLON_DOT, COLON_DOT, PIXEL_WHITE);
    fill_rect(fb, x, TOP + 2 * BIG_H / 3 - COLON_DOT / 2, COLON_DOT, COLON_DOT, PIXEL_WHITE);
}

void time_display_render(Framebuffer *fb, int hour, int minute, int second) {
    for (int i = 0; i < FB_WIDTH * FB_HEIGHT; i++) fb->pixels[i] = PIXEL_BLACK;
    draw_pair(fb, 0, hour, 23);
    draw_pair(fb, 2, minute, 59);
    draw_pair(fb, 4, second, 59);
    draw_colon(fb);
}
