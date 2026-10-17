/* Pixel buffer the core renders the Clock face into (ADR 0001).
 * Pixels use the PSP's native 8888 layout, 0xAABBGGRR as a little-endian
 * word, so the PSP layer can copy rows straight into VRAM. */
#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

#define FB_WIDTH 480
#define FB_HEIGHT 272

typedef uint32_t Pixel;

#define PIXEL_BLACK ((Pixel)0xFF000000u)
#define PIXEL_WHITE ((Pixel)0xFFFFFFFFu)

typedef struct {
    Pixel pixels[FB_WIDTH * FB_HEIGHT];
} Framebuffer;

/* Fills the rectangle with its top-left corner at (x, y), clipped at the
 * screen edges. */
static inline void fb_fill_rect(Framebuffer *fb, int x, int y, int w, int h, Pixel color) {
    for (int row = y; row < y + h; row++) {
        if (row < 0 || row >= FB_HEIGHT) continue;
        for (int col = x; col < x + w; col++)
            if (col >= 0 && col < FB_WIDTH) fb->pixels[row * FB_WIDTH + col] = color;
    }
}

#endif
