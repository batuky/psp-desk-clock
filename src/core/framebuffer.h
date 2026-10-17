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

#endif
