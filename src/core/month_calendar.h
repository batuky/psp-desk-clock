/* Month calendar: a grid of the current month's days, weeks starting on
 * Monday, today's day highlighted. See GLOSSARY.md. */
#ifndef MONTH_CALENDAR_H
#define MONTH_CALENDAR_H

#include "framebuffer.h"
#include "local_time.h"

#define CALENDAR_COLUMNS 7 /* Monday to Sunday */
#define CALENDAR_MAX_ROWS 6

/* Where the days of one month sit in the grid. Arrays are indexed by the
 * day of the month, 1 to `days`; index 0 is unused. */
typedef struct {
    int days;  /* 28 to 31 */
    int rows;  /* week rows the month needs, 4 to 6 */
    int today; /* the day of the month to highlight */
    unsigned char column[32]; /* 0 = Monday ... 6 = Sunday */
    unsigned char row[32];    /* 0 = first week row */
} MonthCells;

/* Lays out the month of `now`. Returns 1 on success, or 0 (leaving `out`
 * untouched) when `now` is NULL or holds a date that does not exist. */
int month_calendar_cells(const LocalTime *now, MonthCells *out);

/* Draws the Month calendar with its top-left corner at (x, y): a header row
 * of weekday initials (P S Ç P C C P) and then one row per week, every cell
 * cell_w x cell_h pixels. Digits are white on black; today is a white cell
 * with black digits. When `now` is NULL or not a real date only the header is
 * drawn. The text is scaled to the largest whole size that leaves a clear gap
 * between neighbouring numbers: a cell needs 14 x 8 pixels for the smallest
 * text, 28 x 16 for twice that, 42 x 24 for three times. */
void month_calendar_draw(Framebuffer *fb, int x, int y, int cell_w, int cell_h,
                         const LocalTime *now);

#endif
