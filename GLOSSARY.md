# PSP Desk Clock

A clock app for a Sony PSP that stays powered on a desk and shows the current time and date at a glance.

## Language

**Clock face**:
The single full screen the app shows: the Time display, the Date line and the Month calendar on a black background.
_Avoid_: Screen, UI, dashboard

**Time display**:
The large white retro seven-segment digits showing hours and minutes, with a smaller seconds readout beside them.
_Avoid_: Clock digits, timer

**Ghost segment**:
A seven-segment bar that is not lit, drawn very faintly so the full digit shape stays visible.
_Avoid_: Off segment, shadow

**Date line**:
The Turkish text line giving the day-month-year date and weekday name, for example `17.10.2026 Cumartesi`.
_Avoid_: Date label, date text

**Month calendar**:
A grid of the current month's days, weeks starting on Monday, with today's day highlighted.
_Avoid_: Calendar view, planner

**Local time**:
The time of day in the timezone the PSP itself is set to, expected to be UTC+3 (Turkey, no daylight saving).
_Avoid_: System time, device time
