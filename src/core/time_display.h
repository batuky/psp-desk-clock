/* Time display: large white seven-segment hours and minutes on a black
 * background. Segments that are not lit are not drawn. See GLOSSARY.md. */
#ifndef TIME_DISPLAY_H
#define TIME_DISPLAY_H

#include "framebuffer.h"
#include "local_time.h"

/* Digit cells in reading order: hour tens, hour units, minute tens,
 * minute units. */
#define TD_DIGIT_COUNT 4

typedef struct {
    int x, y;  /* top-left corner in the Framebuffer */
    int w, h;  /* cell size in pixels */
    int t;     /* segment thickness in pixels */
    int g;     /* pixels each segment is pulled back from the corners it
                  shares with its neighbours */
} DigitCell;

/* How big the Time display is and where it sits vertically. It is centred
 * across the screen. */
typedef struct {
    int digit_w, digit_h; /* one digit cell, in pixels */
    int thickness;        /* segment thickness */
    int corner_gap;       /* see DigitCell.g */
    int top;              /* y of the top of the digits */
} TimeDisplayShape;

/* Fills in where each digit cell sits for `shape`. */
void time_display_cells(const TimeDisplayShape *shape, DigitCell cells[TD_DIGIT_COUNT]);

/* Draws the Time display for `now` over whatever is in the Framebuffer; it
 * does not clear it. A component outside its valid range (hour 0-23, minute
 * 0-59) is drawn as a dash instead of digits, and so is everything when `now`
 * is NULL (the clock could not be read), so the display never shows a wrong
 * time. The shape must keep the digits on the screen. */
void time_display_draw(Framebuffer *fb, const TimeDisplayShape *shape, const LocalTime *now);

#endif
