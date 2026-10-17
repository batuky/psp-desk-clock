#include "clock_face.h"

#include <string.h>

#include "bitmap_font.h"
#include "date_line.h"
#include "month_calendar.h"
#include "time_display.h"

#define DATE_SCALE 3
#define DATE_TEXT_H (FONT_GLYPH_H * DATE_SCALE)
#define DATE_LINE_GAP 8 /* between the two lines of layout D's Date line */
#define EDGE_MARGIN 16

/* The longest text each Date line region has to hold, in characters:
 * "31.08.2026 Pazartesi", "31.08.2026", "Pazartesi". */
#define WHOLE_DATE_CHARS 20
#define DATE_CHARS 10
#define WEEKDAY_CHARS 9

/* Layout D's Month calendar: 7 columns, and a header plus the most week rows
 * a month can need. */
#define CAL_CELL_W 30
#define CAL_CELL_H 17
#define CAL_W (CALENDAR_COLUMNS * CAL_CELL_W)
#define CAL_H ((1 + CALENDAR_MAX_ROWS) * CAL_CELL_H)

static const TimeDisplayShape SHAPE_B = {84, 130, 12, 3, 12};
static const TimeDisplayShape SHAPE_D = {80, 120, 11, 3, 8};

static const TimeDisplayShape *shape_of(ClockLayout layout) {
    return layout == CLOCK_LAYOUT_B ? &SHAPE_B : &SHAPE_D;
}

static int text_width(int chars) { return chars * FONT_ADVANCE * DATE_SCALE - DATE_SCALE; }

static ScreenRect time_region(const TimeDisplayShape *shape) {
    DigitCell cells[TD_DIGIT_COUNT];
    ScreenRect r;
    time_display_cells(shape, cells);
    r.x = cells[0].x;
    r.y = shape->top;
    r.w = cells[TD_DIGIT_COUNT - 1].x + shape->digit_w - cells[0].x;
    r.h = shape->digit_h;
    return r;
}

void clock_face_regions(ClockLayout layout, ClockFaceRegions *out) {
    const TimeDisplayShape *shape = shape_of(layout);
    int below = shape->top + shape->digit_h; /* top of the free area under the time */
    int free_h = FB_HEIGHT - below;

    memset(out, 0, sizeof *out);
    out->time = time_region(shape);

    if (layout == CLOCK_LAYOUT_B) {
        ScreenRect line = {(FB_WIDTH - text_width(WHOLE_DATE_CHARS)) / 2,
                           below + (free_h - DATE_TEXT_H) / 2, text_width(WHOLE_DATE_CHARS),
                           DATE_TEXT_H};
        out->date_line_count = 1;
        out->date_line[0] = line;
        return;
    }

    /* Layout D: the date at the lower left, the calendar at the lower right,
     * both centred in the free area. */
    {
        int block_h = 2 * DATE_TEXT_H + DATE_LINE_GAP;
        int block_y = below + (free_h - block_h) / 2;
        ScreenRect date = {EDGE_MARGIN, block_y, text_width(DATE_CHARS), DATE_TEXT_H};
        ScreenRect weekday = {EDGE_MARGIN, block_y + DATE_TEXT_H + DATE_LINE_GAP,
                              text_width(WEEKDAY_CHARS), DATE_TEXT_H};
        ScreenRect calendar = {FB_WIDTH - EDGE_MARGIN - CAL_W, below + (free_h - CAL_H) / 2,
                               CAL_W, CAL_H};
        out->date_line_count = 2;
        out->date_line[0] = date;
        out->date_line[1] = weekday;
        out->has_calendar = 1;
        out->calendar = calendar;
    }
}

/* Draws `text` inside `region`, centred or against its left edge. */
static void draw_date_text(Framebuffer *fb, const ScreenRect *region, int centred,
                           const char *text) {
    int chars = 0;
    for (const char *p = text; *p; p++)
        if (((unsigned char)*p & 0xC0) != 0x80) chars++; /* count characters, not UTF-8 bytes */
    {
        int x = region->x + (centred ? (region->w - text_width(chars)) / 2 : 0);
        bitmap_font_draw_text(fb, x, region->y, DATE_SCALE, text, PIXEL_WHITE);
    }
}

void clock_face_render(Framebuffer *fb, ClockLayout layout, const LocalTime *now) {
    ClockFaceRegions r;
    char text[DATE_LINE_MAX];

    clock_face_regions(layout, &r);
    for (int i = 0; i < FB_WIDTH * FB_HEIGHT; i++) fb->pixels[i] = PIXEL_BLACK;
    time_display_draw(fb, shape_of(layout), now);

    date_line_text(now, text, sizeof text);
    if (r.date_line_count == 1) {
        draw_date_text(fb, &r.date_line[0], 1, text);
    } else {
        /* The Date line is "date weekday"; layout D puts them on two lines.
         * An unreadable clock has no weekday: its dashes take the first line. */
        char *space = strchr(text, ' ');
        if (space) *space = '\0';
        draw_date_text(fb, &r.date_line[0], 0, text);
        if (space) draw_date_text(fb, &r.date_line[1], 0, space + 1);
    }

    if (r.has_calendar)
        month_calendar_draw(fb, r.calendar.x, r.calendar.y, CAL_CELL_W, CAL_CELL_H, now);
}
