# PSP Desk Clock

A clock app for a Sony PSP that stays powered on a desk and shows the current time and date at a glance.

## Language

**Clock face**:
The single full screen the app shows: the Time display, the Date line and, in some Layouts, the Month calendar on a black background.
_Avoid_: Screen, UI, dashboard

**Layout**:
One of the two arrangements of the Clock face. Layout B is the big Time display with the Date line centred under it and no Month calendar. Layout D, the default, has a slightly smaller Time display, the Date line on two lines at the lower left and the Month calendar at the lower right. Called by their letters, B and D, from the layout design.
_Avoid_: Theme, mode, view

**Time display**:
The large white retro seven-segment digits showing hours and minutes. It has no seconds.
_Avoid_: Clock digits, timer

**Date line**:
The Turkish text giving the day-month-year date and weekday name, for example `17.10.2026 Cumartesi`. One line in Layout B; in Layout D it is split in two, the date over the weekday name.
_Avoid_: Date label, date text

**Month calendar**:
A grid of the current month's days, weeks starting on Monday, with today's day highlighted.
_Avoid_: Calendar view, planner

**Local time**:
The date and time of day in the timezone the PSP itself is set to, expected to be UTC+3 (Turkey, no daylight saving).
_Avoid_: System time, device time

**Settings file**:
The small text file `settings` next to the EBOOT that remembers the chosen Layout between runs. The app writes it whenever the Layout changes and reads it at start; a missing, unreadable or corrupt file means Layout D, and a failed write never stops the clock.
_Avoid_: Config, preferences, save file
