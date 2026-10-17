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

There is no EBOOT build yet, only the core and its tests.

## License

TBD
