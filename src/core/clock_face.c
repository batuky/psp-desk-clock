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

/* The longest text each Date line region has to hold. */
#define WHOLE_DATE_SAMPLE "31.08.2026 Pazartesi"
#define DATE_SAMPLE "31.08.2026"
#define WEEKDAY_SAMPLE "Pazartesi"

/* Layout D's Month calendar: 7 columns, and a header plus the most week rows
 * a month can need. */
#define CAL_CELL_W 30
#define CAL_CELL_H 17
#define CAL_W (MONTH_CALENDAR_COLUMNS * CAL_CELL_W)
#define CAL_H ((1 + MONTH_CALENDAR_MAX_ROWS) * CAL_CELL_H)

/* What differs between the Layouts. A value that is not a Layout gets
 * layout D, the default. */
typedef struct {
    TimeDisplayShape shape;
    int split_date; /* 1: the date over the weekday, lower left, plus the Month
                       calendar; 0: the whole Date line centred under the time */
} LayoutSpec;

static const LayoutSpec SPECS[CLOCK_LAYOUT_COUNT] = {
    [CLOCK_LAYOUT_B] = {{84, 130, 12, 3, 12}, 0},
    [CLOCK_LAYOUT_D] = {{80, 120, 11, 3, 8}, 1},
};

static ClockLayout valid_layout(ClockLayout layout) {
    return (unsigned)layout < CLOCK_LAYOUT_COUNT ? layout : CLOCK_LAYOUT_D;
}

static const LayoutSpec *spec_of(ClockLayout layout) { return &SPECS[valid_layout(layout)]; }

ClockLayout clock_face_next_layout(ClockLayout layout) {
    return (valid_layout(layout) + 1) % CLOCK_LAYOUT_COUNT;
}

static int text_width(const char *text) { return bitmap_font_text_width(text, DATE_SCALE); }

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
    const LayoutSpec *spec = spec_of(layout);
    const TimeDisplayShape *shape = &spec->shape;
    int below = shape->top + shape->digit_h; /* top of the free area under the time */
    int free_h = FB_HEIGHT - below;

    memset(out, 0, sizeof *out);
    out->time = time_region(shape);

    if (!spec->split_date) {
        ScreenRect line = {(FB_WIDTH - text_width(WHOLE_DATE_SAMPLE)) / 2,
                           below + (free_h - DATE_TEXT_H) / 2, text_width(WHOLE_DATE_SAMPLE),
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
        ScreenRect date = {EDGE_MARGIN, block_y, text_width(DATE_SAMPLE), DATE_TEXT_H};
        ScreenRect weekday = {EDGE_MARGIN, block_y + DATE_TEXT_H + DATE_LINE_GAP,
                              text_width(WEEKDAY_SAMPLE), DATE_TEXT_H};
        ScreenRect calendar = {FB_WIDTH - EDGE_MARGIN - CAL_W, below + (free_h - CAL_H) / 2,
                               CAL_W, CAL_H};
        out->date_line_count = 2;
        out->date_line[0] = date;
        out->date_line[1] = weekday;
        out->has_calendar = 1;
        out->calendar = calendar;
    }
}

typedef enum { ALIGN_LEFT, ALIGN_CENTRE } Align;

/* Draws `text` inside `region`, centred or against its left edge. */
static void draw_date_text(Framebuffer *fb, const ScreenRect *region, Align align,
                           const char *text) {
    int x = region->x + (align == ALIGN_CENTRE ? (region->w - text_width(text)) / 2 : 0);
    bitmap_font_draw_text(fb, x, region->y, DATE_SCALE, text, PIXEL_WHITE);
}

void clock_face_render(Framebuffer *fb, ClockLayout layout, const LocalTime *now) {
    ClockFaceRegions regions;
    char text[DATE_LINE_MAX];

    clock_face_regions(layout, &regions);
    fb_clear(fb, PIXEL_BLACK);
    time_display_draw(fb, &spec_of(layout)->shape, now);

    date_line_text(now, text, sizeof text);
    if (regions.date_line_count == 1) {
        draw_date_text(fb, &regions.date_line[0], ALIGN_CENTRE, text);
    } else {
        /* The Date line is "date weekday"; layout D puts them on two lines.
         * An unreadable clock has no weekday: its dashes take the first line. */
        char *space = strchr(text, ' ');
        if (space) *space = '\0';
        draw_date_text(fb, &regions.date_line[0], ALIGN_LEFT, text);
        if (space) draw_date_text(fb, &regions.date_line[1], ALIGN_LEFT, space + 1);
    }

    if (regions.has_calendar)
        month_calendar_draw(fb, regions.calendar.x, regions.calendar.y, CAL_CELL_W, CAL_CELL_H,
                            now);
}
