#include "month_calendar.h"

#include <stdio.h>
#include <string.h>

#include "bitmap_font.h"
#include "calendar.h"

/* Weekday initials, Monday first: Pazartesi, Salı, Çarşamba, Perşembe, Cuma,
 * Cumartesi, Pazar. Ç is U+00C7, spelled as its UTF-8 bytes. */
static const char *const INITIALS[MONTH_CALENDAR_COLUMNS] = {"P", "S", "\xC3\x87", "P",
                                                            "C", "C", "P"};

/* Facts about the font, taken from bitmap_font.h so they cannot drift: a
 * two-digit number at scale 1 is two glyphs with one space between them, and
 * letters sit on all but the last row (that one is only the cedilla). */
#define WIDEST_NUMBER_PIXELS (2 * FONT_ADVANCE - 1)
#define CAP_HEIGHT_PIXELS (FONT_GLYPH_H - 1)
/* Clear space left to the next number, in font pixels, so that "12" next to
 * "13" does not read as "1213". */
#define NUMBER_GAP_PIXELS 3

int month_calendar_cells(const LocalTime *now, MonthCells *out) {
    int first_column, index;

    if (!now || !calendar_date_exists(now->year, now->month, now->day)) return 0;

    memset(out, 0, sizeof *out);
    out->days = calendar_days_in_month(now->year, now->month);
    out->today = now->day;
    /* calendar_weekday counts from Sunday; the grid starts on Monday. */
    first_column = (calendar_weekday(now->year, now->month, 1) + 6) % 7;
    out->rows = (first_column + out->days + MONTH_CALENDAR_COLUMNS - 1) / MONTH_CALENDAR_COLUMNS;
    for (int i = 0; i < out->days; i++) {
        index = first_column + i;
        out->cell[i].column = index % MONTH_CALENDAR_COLUMNS;
        out->cell[i].row = index / MONTH_CALENDAR_COLUMNS;
    }
    return 1;
}

/* The largest whole scale at which a two-digit number fits the cell with a
 * clear gap to its neighbours. The height is measured by FONT_GLYPH_H, the
 * full 8 rows including the cedilla row, although draw_centred centres by
 * CAP_HEIGHT_PIXELS: a cell that holds the cedilla also holds the letters. */
static int text_scale(int cell_w, int cell_h) {
    int by_width = cell_w / (WIDEST_NUMBER_PIXELS + NUMBER_GAP_PIXELS);
    int by_height = cell_h / FONT_GLYPH_H;
    int scale = by_width < by_height ? by_width : by_height;
    return scale < 1 ? 1 : scale;
}

/* Draws `text` (`glyphs` characters long) centred in the cell at (x, y). */
static void draw_centred(Framebuffer *fb, int x, int y, int cell_w, int cell_h, int scale,
                         const char *text, int glyphs, Pixel color) {
    int ink_width = glyphs * FONT_ADVANCE * scale - scale;
    bitmap_font_draw_text(fb, x + (cell_w - ink_width) / 2,
                          y + (cell_h - CAP_HEIGHT_PIXELS * scale) / 2, scale, text, color);
}

void month_calendar_draw(Framebuffer *fb, int x, int y, int cell_w, int cell_h,
                         const LocalTime *now) {
    MonthCells cells;
    int scale = text_scale(cell_w, cell_h);

    for (int column = 0; column < MONTH_CALENDAR_COLUMNS; column++)
        draw_centred(fb, x + column * cell_w, y, cell_w, cell_h, scale, INITIALS[column], 1,
                     PIXEL_WHITE);
    if (!month_calendar_cells(now, &cells)) return;

    for (int i = 0; i < cells.days; i++) {
        char number[3];
        int day = i + 1;
        const MonthCell *at = &cells.cell[i];
        int cell_x = x + at->column * cell_w;
        int cell_y = y + (at->row + 1) * cell_h; /* +1: the header row */
        Pixel color = PIXEL_WHITE;

        if (day == cells.today) {
            fb_fill_rect(fb, cell_x + 1, cell_y + 1, cell_w - 2, cell_h - 2, PIXEL_WHITE);
            color = PIXEL_BLACK;
        }
        /* A day is at most 31; the modulo tells psp-gcc the number fits `number`. */
        snprintf(number, sizeof number, "%u", (unsigned)day % 100u);
        draw_centred(fb, cell_x, cell_y, cell_w, cell_h, scale, number, (int)strlen(number), color);
    }
}
