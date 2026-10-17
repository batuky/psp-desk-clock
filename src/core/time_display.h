/* Time display: large white seven-segment hours and minutes with a smaller
 * seconds readout, on a black background. Unlit segments are drawn as
 * Ghost segments. See GLOSSARY.md. */
#ifndef TIME_DISPLAY_H
#define TIME_DISPLAY_H

#include "framebuffer.h"

/* Digit cells in reading order: hour tens, hour units, minute tens,
 * minute units, second tens, second units. */
#define TD_DIGIT_COUNT 6

typedef struct {
    int x, y;  /* top-left corner in the Framebuffer */
    int w, h;  /* cell size in pixels */
    int t;     /* segment thickness in pixels */
} DigitCell;

/* Where each digit cell sits. Fixed for the lifetime of the program. */
const DigitCell *time_display_cells(void);

/* Clears the Framebuffer and draws the Time display. A component outside
 * its valid range (hour 0-23, minute 0-59, second 0-59) is drawn as a dash
 * instead of digits, so the display never shows a wrong time. */
void time_display_render(Framebuffer *fb, int hour, int minute, int second);

#endif
