#include "date_line.h"

#include <stdio.h>

#define MIN_YEAR 1583 /* the Gregorian calendar, so the leap rule below holds */
#define MAX_YEAR 9999 /* four digits */
#define NO_DATE "--.--.----"

/* Weekday names starting with Sunday. UTF-8 bytes are spelled out: \xC3\x87
 * is C-cedilla, \xC5\x9F is s-cedilla, \xC4\xB1 is dotless i. Adjacent
 * literals stop a hex escape from swallowing the next letter. */
static const char *const WEEKDAY[7] = {
    "Pazar",
    "Pazartesi",
    "Sal\xC4\xB1",
    "\xC3\x87" "ar" "\xC5\x9F" "amba",
    "Per" "\xC5\x9F" "embe",
    "Cuma",
    "Cumartesi",
};

static int is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

static int days_in_month(int year, int month) {
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return month == 2 && is_leap_year(year) ? 29 : days[month - 1];
}

static int date_exists(const LocalTime *t) {
    return t->year >= MIN_YEAR && t->year <= MAX_YEAR && t->month >= 1 && t->month <= 12 &&
           t->day >= 1 && t->day <= days_in_month(t->year, t->month);
}

/* 0 = Sunday ... 6 = Saturday (Sakamoto's method). */
static int weekday_of(int year, int month, int day) {
    static const int offset[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3) year--;
    return (year + year / 4 - year / 100 + year / 400 + offset[month - 1] + day) % 7;
}

int date_line_text(const LocalTime *now, char *out, size_t size) {
    int written;
    if (size == 0) return -1;

    if (now && date_exists(now)) {
        written = snprintf(out, size, "%02d.%02d.%04d %s", now->day, now->month, now->year,
                           WEEKDAY[weekday_of(now->year, now->month, now->day)]);
    } else {
        written = snprintf(out, size, "%s", NO_DATE);
    }
    if (written < 0 || (size_t)written >= size) {
        out[0] = '\0';
        return -1;
    }
    return written;
}
