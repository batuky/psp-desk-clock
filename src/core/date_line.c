#include "date_line.h"

#include <stdio.h>

#include "calendar.h"

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

int date_line_text(const LocalTime *now, char *out, size_t size) {
    int written;
    if (size == 0) return -1;

    if (now && calendar_date_exists(now->year, now->month, now->day)) {
        written = snprintf(out, size, "%02d.%02d.%04d %s", now->day, now->month, now->year,
                           WEEKDAY[calendar_weekday(now->year, now->month, now->day)]);
    } else {
        written = snprintf(out, size, "%s", NO_DATE);
    }
    if (written < 0 || (size_t)written >= size) {
        out[0] = '\0';
        return -1;
    }
    return written;
}
