/* Clock face: the whole screen the app shows, composed from the Time
 * display, the Date line and the Month calendar in one of the Layouts. See
 * GLOSSARY.md. */
#ifndef CLOCK_FACE_H
#define CLOCK_FACE_H

#include "framebuffer.h"
#include "local_time.h"

typedef enum {
    CLOCK_LAYOUT_B, /* big time and date only */
    CLOCK_LAYOUT_D, /* big time, date and calendar */
    CLOCK_LAYOUT_COUNT
} ClockLayout;

/* The Layout shown when nothing says otherwise. */
#define CLOCK_LAYOUT_DEFAULT CLOCK_LAYOUT_D

/* The most lines a layout's Date line is split over. */
#define CLOCK_FACE_MAX_DATE_LINES 2

typedef struct {
    int x, y, w, h;
} ScreenRect;

/* The area each piece of a Clock face is confined to. Whatever a piece
 * draws, for any date and time, stays inside its rectangle. */
typedef struct {
    ScreenRect time;
    int date_line_count;      /* 1: the whole Date line; 2: the date, then the weekday */
    ScreenRect date_line[CLOCK_FACE_MAX_DATE_LINES];
    int has_calendar;         /* 0 when the layout has no Month calendar */
    ScreenRect calendar;      /* only meaningful when has_calendar */
} ClockFaceRegions;

/* The Layout after `layout`, wrapping round to the first. A value that is
 * not a Layout counts as layout D. */
ClockLayout clock_face_next_layout(ClockLayout layout);

/* Where the pieces of `layout` go. Fixed for the lifetime of the program. */
void clock_face_regions(ClockLayout layout, ClockFaceRegions *out);

/* Clears the Framebuffer and draws the Clock face for `now`. When `now` is
 * NULL (the clock could not be read) the Time display and the Date line show
 * dashes and the Month calendar only its header, so the face never shows a
 * wrong time or date. */
void clock_face_render(Framebuffer *fb, ClockLayout layout, const LocalTime *now);

#endif
