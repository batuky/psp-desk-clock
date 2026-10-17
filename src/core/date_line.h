/* Date line: the Turkish text shown for a date, for example
 * "17.10.2026 Cumartesi" (day.month.year, then the weekday). See
 * GLOSSARY.md. The text is UTF-8 (Ç, ş and ı are two-byte characters). */
#ifndef DATE_LINE_H
#define DATE_LINE_H

#include <stddef.h>

#include "local_time.h"

/* Room for any Date line with its terminating NUL. The longest, such as
 * "14.10.2026 Çarşamba", is 21 bytes plus the NUL (Ç and ş take two bytes
 * each); the rest is spare. */
#define DATE_LINE_MAX 32

/* Writes the Date line for the date in `now` into `out`. When `now` is NULL
 * (the clock could not be read) or holds a date that does not exist (month
 * 13, 30 February, ...), it writes "--.--.----" instead, so the display
 * never shows a wrong date. Only the year, month and day of `now` are used.
 * Returns the text length, or -1 when `size` is too small (then `out` holds
 * an empty string, if size is at least 1). */
int date_line_text(const LocalTime *now, char *out, size_t size);

#endif
