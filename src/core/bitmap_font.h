/* A small hand-drawn bitmap font for the Date line and the Month calendar.
 * Glyphs are 5x8 pixels (letters sit on rows 0-6; row 7 holds the cedilla
 * of Ç and ş). It covers only what the clock shows: the digits, '.', '-',
 * space, and the letters of the Turkish weekday names (including Ç, ş and ı).
 * Text is UTF-8. */
#ifndef BITMAP_FONT_H
#define BITMAP_FONT_H

#include "framebuffer.h"

#define FONT_GLYPH_W 5
#define FONT_GLYPH_H 8
#define FONT_ADVANCE 6 /* glyph width plus one pixel of space, at scale 1 */

/* Draws `utf8` with its top-left corner at (x, y). Every font pixel becomes
 * a scale x scale block of `color`. A character the font does not have is
 * drawn as an empty box, so a missing glyph is visible. Anything outside the
 * Framebuffer is clipped. Returns the x position after the last character
 * (each character, however many bytes it takes, advances FONT_ADVANCE *
 * scale). */
int bitmap_font_draw_text(Framebuffer *fb, int x, int y, int scale, const char *utf8,
                          Pixel color);

/* 1 when every character of `utf8` has a real glyph, else 0. */
int bitmap_font_can_draw(const char *utf8);

#endif
