/* Local time: the date and time of day in the timezone the PSP is set to.
 * See GLOSSARY.md. Fields are as read from the clock, so a broken reading
 * can hold out-of-range values; users of the type must not assume they are
 * valid. */
#ifndef LOCAL_TIME_H
#define LOCAL_TIME_H

typedef struct {
    int year;   /* for example 2026 */
    int month;  /* 1-12 */
    int day;    /* 1-31 */
    int hour;   /* 0-23 */
    int minute; /* 0-59 */
    int second; /* 0-59 */
} LocalTime;

static inline int local_time_equal(const LocalTime *a, const LocalTime *b) {
    return a->year == b->year && a->month == b->month && a->day == b->day &&
           a->hour == b->hour && a->minute == b->minute && a->second == b->second;
}

#endif
