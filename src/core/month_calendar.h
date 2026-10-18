/* Month calendar: a grid of the current month's days, weeks starting on
 * Monday, today's day highlighted. See GLOSSARY.md. */
#ifndef MONTH_CALENDAR_H
#define MONTH_CALENDAR_H

#include "framebuffer.h"
#include "local_time.h"

#define MONTH_CALENDAR_COLUMNS 7 /* Monday to Sunday */
#define MONTH_CALENDAR_MAX_ROWS 6
#define MONTH_CALENDAR_MAX_DAYS 31

/* One day's place in the grid, counted from 0. */
typedef struct {
    int column; /* 0 = Monday ... 6 = Sunday */
    int row;    /* 0 = first week row */
} MonthCell;

/* Where the days of one month sit in the grid. `cell[0]` is the 1st of the
 * month and `cell[days - 1]` the last; the entries after that are unused. */
typedef struct {
    int days;  /* 28 to 31 */
    int rows;  /* week rows the month needs, 4 to 6 */
    int today; /* the day of the month to highlight, 1 to `days` */
    MonthCell cell[MONTH_CALENDAR_MAX_DAYS];
} MonthCells;

/* The cell of `day`, counted from 1 as on the calendar; `day` must be 1 to
 * `days`. */
static inline const MonthCell *month_cell_of_day(const MonthCells *m, int day) {
    return &m->cell[day - 1];
}

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
