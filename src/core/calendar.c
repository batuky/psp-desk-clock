#include "calendar.h"

static int is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int calendar_days_in_month(int year, int month) {
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return month == 2 && is_leap_year(year) ? 29 : days[month - 1];
}

int calendar_date_exists(int year, int month, int day) {
    return year >= CALENDAR_MIN_YEAR && year <= CALENDAR_MAX_YEAR && month >= 1 && month <= 12 &&
           day >= 1 && day <= calendar_days_in_month(year, month);
}

/* Sakamoto's method. */
int calendar_weekday(int year, int month, int day) {
    static const int offset[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3) year--;
    return (year + year / 4 - year / 100 + year / 400 + offset[month - 1] + day) % 7;
}
