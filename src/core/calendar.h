/* Gregorian calendar facts shared by the Date line and the Month calendar. */
#ifndef CALENDAR_H
#define CALENDAR_H

#define CALENDAR_MIN_YEAR 1583 /* the Gregorian calendar, so the leap rule holds */
#define CALENDAR_MAX_YEAR 9999 /* four digits */

/* 1 when the year, month (1-12) and day name a real date between
 * CALENDAR_MIN_YEAR and CALENDAR_MAX_YEAR, else 0. */
int calendar_date_exists(int year, int month, int day);

/* Number of days in a month. Only meaningful for a month from 1 to 12. */
int calendar_days_in_month(int year, int month);

/* Weekday of a real date: 0 = Sunday ... 6 = Saturday. */
int calendar_weekday(int year, int month, int day);

#endif
