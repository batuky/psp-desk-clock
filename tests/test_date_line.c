#include <string.h>

#include "bitmap_font.h"
#include "date_line.h"
#include "minitest.h"

/* Check one date. The expected text comes from the calendar, not from the
 * code: each weekday below is a well-known date. */
static int text_is(int y, int mo, int d, const char *expected) {
    LocalTime t = {y, mo, d, 12, 0, 0};
    char out[DATE_LINE_MAX];
    int n = date_line_text(&t, out, sizeof out);
    return n == (int)strlen(expected) && strcmp(out, expected) == 0;
}

static void the_date_line_shows_day_month_year_and_the_weekday(void) {
    CHECK(text_is(2026, 10, 17, "17.10.2026 Cumartesi"));
}

/* Every weekday name, including the three that need Turkish letters. UTF-8
 * bytes are spelled out: \xC3\x87 is C-cedilla, \xC5\x9F is s-cedilla,
 * \xC4\xB1 is dotless i. Adjacent literals stop a hex escape from swallowing
 * the next letter. */
#define CCEDILLA "\xC3\x87"
#define SCEDILLA "\xC5\x9F"
#define DOTLESS_I "\xC4\xB1"

static void every_weekday_has_its_turkish_name(void) {
    CHECK(text_is(2026, 10, 12, "12.10.2026 Pazartesi"));
    CHECK(text_is(2026, 10, 13, "13.10.2026 Sal" DOTLESS_I));
    CHECK(text_is(2026, 10, 14, "14.10.2026 " CCEDILLA "ar" SCEDILLA "amba"));
    CHECK(text_is(2026, 10, 15, "15.10.2026 Per" SCEDILLA "embe"));
    CHECK(text_is(2026, 10, 16, "16.10.2026 Cuma"));
    CHECK(text_is(2026, 10, 17, "17.10.2026 Cumartesi"));
    CHECK(text_is(2026, 10, 18, "18.10.2026 Pazar"));
}

static void leap_days_and_year_changes_land_on_the_right_weekday(void) {
    CHECK(text_is(2024, 2, 29, "29.02.2024 Per" SCEDILLA "embe"));  /* leap day */
    CHECK(text_is(2000, 2, 29, "29.02.2000 Sal" DOTLESS_I));        /* 2000 is a leap year */
    CHECK(text_is(2000, 1, 1, "01.01.2000 Cumartesi"));
    CHECK(text_is(1999, 12, 31, "31.12.1999 Cuma"));
    CHECK(text_is(2026, 12, 31, "31.12.2026 Per" SCEDILLA "embe"));
    CHECK(text_is(2027, 1, 1, "01.01.2027 Cuma"));
    CHECK(text_is(2100, 3, 1, "01.03.2100 Pazartesi"));              /* 2100 is not a leap year */
    CHECK(text_is(2038, 1, 19, "19.01.2038 Sal" DOTLESS_I));
}

static void a_missing_or_impossible_date_shows_dashes(void) {
    char out[DATE_LINE_MAX];
    LocalTime t = {2026, 10, 17, 12, 0, 0};

    CHECK(date_line_text(NULL, out, sizeof out) == 10);
    CHECK(strcmp(out, "--.--.----") == 0);

    t.month = 13; CHECK(text_is(t.year, t.month, t.day, "--.--.----"));
    t.month = 0;  CHECK(text_is(t.year, t.month, t.day, "--.--.----"));
    CHECK(text_is(2026, 10, 0, "--.--.----"));
    CHECK(text_is(2026, 10, 32, "--.--.----"));
    CHECK(text_is(2026, 4, 31, "--.--.----"));   /* April has 30 days */
    CHECK(text_is(2026, 2, 29, "--.--.----"));   /* 2026 is not a leap year */
    CHECK(text_is(1900, 2, 29, "--.--.----"));   /* 1900 is not a leap year */
    CHECK(text_is(0, 1, 1, "--.--.----"));
    CHECK(text_is(10000, 1, 1, "--.--.----"));
}

static void a_buffer_that_is_too_small_gives_an_empty_string(void) {
    LocalTime t = {2026, 10, 17, 12, 0, 0};
    char out[8];
    memset(out, 'x', sizeof out);
    CHECK(date_line_text(&t, out, sizeof out) == -1);
    CHECK(out[0] == '\0');
    CHECK(date_line_text(&t, out, 0) == -1); /* nothing to write into */
}

/* The Date line and the font must agree: everything the Date line can say
 * has a glyph. Checked for every weekday and for the dashes. */
static void everything_the_date_line_says_can_be_drawn_in_the_font(void) {
    char out[DATE_LINE_MAX];
    for (int day = 12; day <= 18; day++) { /* 12-18 October 2026 is Monday to Sunday */
        LocalTime t = {2026, 10, day, 12, 0, 0};
        CHECK(date_line_text(&t, out, sizeof out) > 0);
        CHECK(bitmap_font_can_draw(out));
    }
    CHECK(date_line_text(NULL, out, sizeof out) > 0);
    CHECK(bitmap_font_can_draw(out));
}

int main(void) {
    RUN(the_date_line_shows_day_month_year_and_the_weekday);
    RUN(everything_the_date_line_says_can_be_drawn_in_the_font);
    RUN(every_weekday_has_its_turkish_name);
    RUN(leap_days_and_year_changes_land_on_the_right_weekday);
    RUN(a_missing_or_impossible_date_shows_dashes);
    RUN(a_buffer_that_is_too_small_gives_an_empty_string);
    return mt_report();
}
