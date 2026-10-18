# psp-desk-clock

A desk clock for the Sony PSP, written as PSP homebrew.

> Status: the clock works and HOME exits cleanly. Still to come: a 24-hour soak test on the device.

## Idea

Run a clock app on a PSP (custom firmware) so the handheld can sit on a desk and show the time.

## What it does

- Shows the time in large seven-segment digits (hours and minutes, no seconds) and the date in Turkish, for example `17.10.2026 Cumartesi`.
- Two Layouts of the Clock face. **Layout D** (the default) has the time, the date on two lines at the lower left, and a Month calendar at the lower right with today highlighted. **Layout B** has a bigger time with the date on one line and no calendar.
- Press **L** or **R** to switch between the two Layouts. The app remembers the choice in a small `settings` file next to the EBOOT, so it starts in the same Layout next time (Layout D if the file is missing or unreadable).
- Keeps the screen on and the PSP awake while it runs.

## Planned

- 24-hour soak test on the device

See the open issues for the full list.

## Build

Uses the [PSPDEV](https://github.com/pspdev/pspdev) toolchain through Docker (no native Windows build exists). The first run builds a small dev image on top of `pspdev/pspdev`.

```sh
tools/dev.sh make test              # host-side tests of the pure core
tools/dev.sh make core-psp-check    # core also compiles with psp-gcc
tools/dev.sh make update-golden     # regenerate golden images, review before committing
tools/dev.sh make eboot             # builds build/psp/EBOOT.PBP
```

## Running on a PSP

1. Put the PSP in USB mode and copy `build/psp/EBOOT.PBP` to `PSP/GAME/psp-desk-clock/` on the Memory Stick. Flush or safely eject before leaving USB mode.
2. If your custom firmware is temporary (PRO-C), run its launcher from the Game menu (for example `661 PRO-C Fast Recovery`) after every boot. Without it the app fails with "This game cannot be started. The data is corrupted."
3. Start **PSP Desk Clock** from Game → Memory Stick. **L** or **R** switches the Layout. HOME exits.
4. Set the PSP's timezone in the XMB (Turkey: +03:00); the clock shows the PSP's own local time.

The same EBOOT runs in the PPSSPP emulator, but PPSSPP does not emulate power, CPU clock or screen-off behaviour.

## License

TBD
