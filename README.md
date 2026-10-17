# psp-desk-clock

A desk clock for the Sony PSP, written as PSP homebrew.

> Status: just started. Nothing works yet.

## Idea

Run a clock app on a PSP (custom firmware) so the handheld can sit on a desk and show the time.

## Planned

- Show the current time and date
- Keep the screen on while plugged in
- Simple, readable layout for a desk

## Build

Uses the [PSPDEV](https://github.com/pspdev/pspdev) toolchain through Docker (no native Windows build exists). The first run builds a small dev image on top of `pspdev/pspdev`.

```sh
tools/dev.sh make test              # host-side tests of the pure core
tools/dev.sh make core-psp-check    # core also compiles with psp-gcc
tools/dev.sh make update-golden     # regenerate golden images, review before committing
```

```sh
tools/dev.sh make eboot             # builds build/psp/EBOOT.PBP
```

## Running on a PSP

1. Put the PSP in USB mode and copy `build/psp/EBOOT.PBP` to `PSP/GAME/psp-desk-clock/` on the Memory Stick. Flush or safely eject before leaving USB mode.
2. If your custom firmware is temporary (PRO-C), run its launcher from the Game menu (for example `661 PRO-C Fast Recovery`) after every boot. Without it the app fails with "This game cannot be started. The data is corrupted."
3. Start **PSP Desk Clock** from Game → Memory Stick. HOME exits.
4. Set the PSP's timezone in the XMB (Turkey: +03:00); the clock shows the PSP's own local time.

The same EBOOT runs in the PPSSPP emulator, but PPSSPP does not emulate power, CPU clock or screen-off behaviour.

## License

TBD
