# PSP desk clock: feasibility and platform facts

Researched 2026-10-17. Scope: always-on desk clock, user-mode homebrew on PSP custom firmware, tested in PPSSPP, built from Windows.

Source policy: first-party code and docs only (pspdev org, PPSSPP repo and docs, SDL repo). Every source is pinned to a commit; links go to the file at that commit and the text gives line numbers. Items with no first-party source are listed under "Unverified / open", not asserted. Claims marked *(inference)* are my reading of the code, not something a source states. Secondary sources are labelled where used.

## Summary

| # | Question | Answer | Confidence | Key source |
|---|----------|--------|------------|------------|
| 1a | Read current time | `sceRtcGetCurrentClockLocalTime(ScePspDateTime*)` (has a `microsecond` field), or `sceRtcGetCurrentTick(u64*)` + `sceRtcGetTickResolution()`. All are user-mode stubs. | High | [psprtc.h][psprtc], [sceRtc.S][rtc-user] |
| 1b | Timezone / DST | Read-only for apps. `sceUtilityGetSystemParamInt` ids 6 (offset, minutes east of UTC) and 7 (DST flag). pspsdk's libc init copies them into `TZ` once at startup. Whether the RTC local-time calls apply DST on real hardware is undocumented. | High (API), Low (hardware DST) | [sysparam.h][sysparam], [timezone.c][tz] |
| 1c | Set the RTC from an app | No. `sceRtcSetCurrentTick` exists only in the kernel (`sceRtc_driver`) stub. Design: user sets time in the XMB, the app only reads. | High (stubs), Medium (firmware enforcement) | [sceRtc_driver.S][rtc-drv] L147 |
| 1d | PPSSPP time | Host wall clock captured once at boot, plus emulated "virtual" time. Local conversions use the host timezone, not the emulated sysparam. | High | [sceRtc.cpp][pp-rtc] L80-84, L146-155 |
| 2a | `scePowerTick` | Constants ALL=0, SUSPEND=1, DISPLAY=6. Header says it prevents power-off and display-off. Exact per-flag meaning (suspend vs display-off vs dim) is not documented. No-op in PPSSPP. | Medium (existence), Low (per-flag semantics) | [psppower.h][psppower] L47-55, L251-259 |
| 2b | Brightness from user mode | No user-mode setter in the SDK. `sceDisplaySetBrightness` (0-100) and `sceImposeSetParam(BACKLIGHT_BRIGHTNESS)` are kernel/driver-library only. User-mode `sceImposeSetBacklightOffTime` exists. PPSSPP stores brightness but renders nothing. | High (SDK), Medium (hardware) | [pspdisplay_kernel.h][disp-k], [pspimpose_driver.h][imp-drv], [pspimpose.h][imp-user] |
| 2c | Display off | `sceDisplayEnable/Disable` are driver-only. User-mode "off" means draw black. Not emulated in PPSSPP. | Medium | [psplibdoc display.csv][ld-disp] |
| 2d | Power callbacks | `scePowerRegisterCallback` plus `PSP_POWER_CB_*` flags; polling via `scePowerIsPowerOnline`, `scePowerGetBatteryLifePercent`, etc. PPSSPP: fixed AC/100%, one initial callback, no later events. | High | [psppower.h][psppower] L23-143, [scePower.cpp][pp-pow] |
| 3a | Windows toolchain | No native Windows release. Official guide: WSL Ubuntu with the Linux tarball, or Docker `pspdev/pspdev`. | High | [installation/windows.md][site-win], [releases][pspdev-rel] |
| 3b | Build | `psp-cmake ..` then `make` gives `EBOOT.PBP`. The CMake project is about 20 lines using `create_pbp_file`. | High | [hello/CMakeLists.txt][site-cmake], [CreatePBP.cmake][createpbp] |
| 3c | Run in PPSSPP | `PPSSPPWindows64.exe [--escape-exit] [--windowed] EBOOT.PBP`. A headless screenshot/timeout mode exists on master but is not in the v1.20.4 tag. | High (GUI flags), Low (headless in releases) | [ppsspp.org CLI docs][pp-cli] |
| 4 | Graphics | For a big clock face, a custom raw-framebuffer or libGU renderer beats the debug screen (fixed 8x8 font). intraFont and SDL3 (+SDL3_ttf) are both packaged. LCD only; no burn-in mention in any source. | High (facts), Medium (recommendation) | [scr_printf.c][scr], [intraFont.h][intrah], [psp-packages][pspkg] |
| 5 | Network / NTP | Feasible in user mode (`sceNetApctl*` + BSD sockets, working sample exists), but no NTP code in pspdev and a Wi-Fi profile must exist in the XMB. PPSSPP: WLAN off by default. Not worth it for v1; manual XMB time is enough. | Medium | [net/simple/main.c][net-sample], [Config.cpp][pp-cfg] L1065 |

## 1. Time

### API surface (all user-mode, library `sceRtc`)
- `sceRtcGetTickResolution()` returns "# of ticks per second" ([psprtc.h][psprtc] L33-38). `sceRtcGetCurrentTick(u64*)` ([L40-46][psprtc]). `sceRtcGetCurrentClock(ScePspDateTime*, int tz)` takes tz in minutes from UTC ([L48-55][psprtc]). `sceRtcGetCurrentClockLocalTime(ScePspDateTime*)` ([L57-63][psprtc]). `sceRtcConvertUtcToLocalTime` / `sceRtcConvertLocalTimeToUTC` operate on `u64` ticks ([L65-81][psprtc]).
- `sceRtcGetDayOfWeek(y,m,d)` returns 0 = Monday ([L100-108][psprtc]). Useful for week-day labels without a lookup of your own.
- The type is `ScePspDateTime` (there is no `pspTime`): `u16 year, month, day, hour, minute, second; u32 microsecond` ([psptypes.h][psptypes] L420-428).
- Tick epoch: year 0001. PPSSPP implements `rtcMagicOffset = 62135596800000000` microseconds ([sceRtc.cpp][pp-rtc] L56), and the SDL3 PSP port does the same conversion with `DELTA_EPOCH_0001_OFFSET = 62135596800` ([SDL_systime.c][sdl-time] L32, L76-90). SDL3 divides by `sceRtcGetTickResolution()` rather than hardcoding it.
- libc path: `_gettimeofday` calls `sceKernelLibcGettimeofday` ([glue.c][glue] L737-750); `clock_gettime` likewise ([L806-][glue]). This gives UTC; local time then depends on `TZ` (below).

### Timezone / DST
- `PSP_SYSTEMPARAM_ID_INT_TIMEZONE = 6`, commented "Timezone offset from UTC in minutes, (EST = -300)"; `..._DAYLIGHTSAVINGS = 7` with `STD = 0`, `SAVING = 1`; `TIME_FORMAT = 5` (0 = 24h, 1 = 12h); `DATE_FORMAT = 4` (YYYYMMDD / MMDDYYYY / DDMMYYYY) ([psputility_sysparam.h][sysparam] L26-34, L69-85). Getter: `sceUtilityGetSystemParamInt(id, int*)`, failure value `0x80110103` ([L51-52, L128][sysparam]).
- pspsdk's libc init builds `TZ` from these at startup: `"GMT" + (offset<0 ? "+" : "-") + HH:MM + ("daylight" if DST flag)` ([timezone.c][tz] L18-33), called from `__libcglue_init` ([init.c][cinit] L81). Read once at launch, so a change made in the XMB is picked up on the next launch, not while the app runs *(inference)*.
- `sceUtilitySetSystemParamInt` is declared ([sysparam.h][sysparam] L103-110). PPSSPP's implementation accepts only `ADHOC_CHANNEL` and `WLAN_POWERSAVE`, with the comment "PSP can only set above int parameters" ([sceUtility.cpp][pp-util] L1532-1546). Treat timezone/DST/format as read-only.
- Not documented in any source I found: whether `sceRtcGetCurrentClockLocalTime` / `sceRtcConvertUtcToLocalTime` apply the DST flag on real hardware, or where their offset comes from. SDL3 uses `sceRtcConvertUtcToLocalTime` for local time ([SDL_systime.c][sdl-time] L106-125), which is evidence it is the intended local-time path, not a statement of DST behaviour.

### Setting the RTC
- `sceRtcSetCurrentTick`, `sceRtcSetCurrentSecureTick`, `sceRtcSetConf`, `sceRtcInit` and friends appear only in the `sceRtc_driver` stub ([sceRtc_driver.S][rtc-drv] L147 for `SetCurrentTick`); the user `sceRtc` stub has no `SetCurrentTick` ([sceRtc.S][rtc-user], 0 matches). psplibdoc agrees: `SetCurrentTick` is exported by `sceRtc_driver` ([rtc.csv][ld-rtc] L80). Alarm functions (`sceRtcSetAlarmTick`, `sceRtcIsAlarmed`) are in the user library ([rtc.csv][ld-rtc] L42-45).
- PPSSPP has no `sceRtcSetCurrentTick` handler (the HLE table, [sceRtc.cpp][pp-rtc] L1112-, has no entry), so it could not be exercised there either.
- Consequence: v1 reads the RTC; the user sets it in the XMB. A kernel-mode setter would need a PRX/CFW path (open question below).

### Drift
- No documentation of real-hardware RTC drift found in any primary source. See "Unverified / open".

### Sub-second resolution
- `ScePspDateTime.microsecond` exists ([psptypes.h][psptypes] L427). PPSSPP reports 1,000,000 ticks/s ([sceRtc.cpp][pp-rtc] L273-276) and fills `microsecond` from virtual time ([L325-343][pp-rtc]). Real-hardware granularity is undocumented; a clock face needs only 1 Hz.

### How PPSSPP implements it
- `__RtcGetCurrentTick() = CoreTiming::GetGlobalTimeUs() + rtcBaseTicks` ([sceRtc.cpp][pp-rtc] L80-84). `__RtcInit` takes the base from `gettimeofday` once, with `tv_usec` zeroed ("the only case we use gettimeofday()"; everything else is "virtual time") ([L146-155][pp-rtc]). `__RtcTimeOfDay` adds emulated elapsed microseconds ([L169-175][pp-rtc]). `sceKernelLibcGettimeofday` uses the same `__RtcTimeOfDay` ([sceKernelTime.cpp][pp-ktime] L171-184); `sceKernelLibcTime` instead uses a separate `start_time` plus emulated seconds (L156-168).
- *(inference)* Because time is host-base plus emulated time, it can drift from the host wall clock if emulation runs slower or faster than real time, or is paused, and it starts up to 1 s off the host's sub-second phase.
- Local time in PPSSPP comes from the **host** timezone: `sceRtcGetCurrentClockLocalTime` uses `localtime()` ([L325-343][pp-rtc]); the UTC/local converters use `_get_timezone` / `tm_gmtoff` with a `TODO : Let the user select his timezone / daylight saving instead of taking system param ?` ([L436-480][pp-rtc]).
- Meanwhile `sceUtilityGetSystemParamInt(TIMEZONE)` returns `g_Config.iTimeZone` (default 0) and DST comes from a settings checkbox ([sceUtility.cpp][pp-util] L1579-1583; [Config.cpp][pp-cfg] L1104-1105; [GameSettingsScreen.cpp][pp-gss] L1476). I found no timezone control in `GameSettingsScreen.cpp`, only the config key. Net effect: inside PPSSPP, libc `localtime()` (TZ from sysparam, default GMT+0) and `sceRtcGetCurrentClockLocalTime` (host local time) can disagree.

## 2. Display and power

### `scePowerTick`
- Constants: `PSP_POWER_TICK_ALL 0`, `PSP_POWER_TICK_SUSPEND 1`, `PSP_POWER_TICK_DISPLAY 6` ([psppower.h][psppower] L47-55). Doc: "Generate a power tick, preventing unit from powering off and turning off display", `type` is one of the three ([L251-259][psppower]).
- The header does not say which flag disables suspend vs display-off vs backlight dim; names are "Suspend" and "Display" only. The official pspsdk power sample sends `scePowerTick(0)` every loop iteration when toggled on ([samples/power/main.c][pow-sample] L136-138). Required cadence is undocumented.
- PPSSPP: `scePowerTick` ignores its argument and does nothing ([scePower.cpp][pp-pow] L604-607). No idle suspend/dim is emulated *(inference: no idle-timer code; `scePowerIdleTimerEnable` etc. are null handlers, L617-621)*. Keep-awake logic cannot be validated in PPSSPP.

### Brightness from user mode
- User headers expose no setter: `psppower.h` has no backlight function; `pspdisplay.h` has no brightness function ([pspdisplay.h][disp-h] L91-163).
- `sceDisplaySetBrightness(level, unk)` "0 (black screen) to 100" and `sceDisplayGetBrightness` live in `pspdisplay_kernel.h` ([L51-65][disp-k]); their stubs are in `sceDisplay_driver.S` ([L54, L57][disp-drv]); the user `sceDisplay.S` stub has no brightness entry. psplibdoc: `SetBrightness` only in `sceDisplay_driver` ([display.csv][ld-disp] L41); `GetBrightness` is exported by both `sceDisplay` and `sceDisplay_driver` ([L15, L26][ld-disp]). A user-mode read might therefore be possible with a hand-made import, untested.
- `sceImposeGetParam/SetParam` with `PSP_IMPOSE_BACKLIGHT_BRIGHTNESS = 0x2` and `BACKLIGHT_OFF_INTERVAL = 0x200` are in `pspimpose_driver.h` ([L27-53][imp-drv]), stubbed from the `sceImpose_driver` library ([sceImpose_driver.S][imp-drv-s] L6, L33, L45): kernel only.
- User-mode `sceImpose` exposes only `sceImposeGetBacklightOffTime` / `sceImposeSetBacklightOffTime` ("backlight timer in seconds ... 30 to a lot of seconds") ([pspimpose.h][imp-user] L22-37). Whether that persists into the system setting is undocumented.
- `scePowerGetBacklightMaximum` is in the user stub ([scePower.S][pow-s] L12) but not declared in `psppower.h`, so its signature is undocumented. No `scePowerSetBacklight` appears in any pspsdk stub (`grep -i backlight` of `scePower.S` / `scePower_driver.S`: only `GetBacklightMaximum`). Driver-only `sceDisplaySetBacklightSel/GetBacklightSel` exist in psplibdoc ([display.csv][ld-disp] L52-54).
- What "kernel mode" means for the EBOOT: module attribute `PSP_MODULE_USER = 0`, `PSP_MODULE_KERNEL = 0x1000` ([pspmoduleinfo.h][modinfo] L38-43); the official hello sample declares `PSP_MODULE_INFO("Hello World", 0, 1, 0)` and `PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER)` ([hello/main.c][site-hello] L6-7). `PSP_FW_VERSION` (default 600) and `PSP_LARGE_MEMORY` are Makefile build knobs that set `-D_PSP_FW_VERSION` and the SFO `MEMSIZE` flag ([build.mak][buildmak] L37-66); the CMake path hardcodes `-D_PSP_FW_VERSION=600` ([pspdev.cmake][pspdevcmake] L35) and passes `MEMSIZE` (default `2`) to `mksfoex` ([CreatePBP.cmake][createpbp] L165-166, L179). None of these grants kernel privileges. Whether CFW runs an `0x1000` EBOOT from `PSP/GAME` in kernel mode has no first-party source (open).
- Levels: PPSSPP's comments say "Standard levels on a PSP: 44, 60, 72, 84 (AC only)" ([sceDisplay.cpp][pp-disp] L1165) and `sceImposeGetParam(BACKLIGHT_BRIGHTNESS)` returns 3 ([sceImpose.cpp][pp-imp] L199). That points to 4 hardware levels, but it is an emulator author's knowledge, not hardware documentation (treat as secondary).
- PPSSPP: `sceDisplaySetBrightness` only stores the value ("Note: Only usable in kernel mode.") and has no visual effect ([sceDisplay.cpp][pp-disp] L1177-1181). The same table is registered under both `sceDisplay` and `sceDisplay_driver` ([L1215-1221][pp-disp]), and I saw no kernel-mode check on it *(inference)*, so PPSSPP would accept a user-mode call that real hardware may reject. Impose backlight-off-time is stored only ([sceImpose.cpp][pp-imp] L118-125).

### Turning the display off
- `sceDisplayEnable` / `sceDisplayDisable` are in `sceDisplay_driver` only ([sceDisplay_driver.S][disp-drv] L24, L27; [display.csv][ld-disp] L28, L33) = kernel. PPSSPP implements neither (no matches in `sceDisplay.cpp`).
- User-mode "blank": point `sceDisplaySetFrameBuf` at an all-black buffer ([pspdisplay.h][disp-h] L91-101). That is always available but does not turn the backlight off.

### Power callbacks and battery
- Register: `scePowerRegisterCallback(slot 0-15 or -1, cbid)` where `cbid` comes from `sceKernelCreateCallback` ([psppower.h][psppower] L65-73). Callback type `void (*)(int unknown, int powerInfo)` ([L57-63][psppower]); the official sample uses a 3-arg form registered from a callback thread ([power sample][pow-sample] L36-73).
- Flags ([psppower.h][psppower] L23-45): `POWER_SWITCH 0x80000000`, `HOLD_SWITCH 0x40000000`, `STANDBY 0x00080000`, `RESUME_COMPLETE 0x00040000` (comment: only seems triggered when another event happens), `RESUMING 0x00020000`, `SUSPENDING 0x00010000` (comment: "seems to occur due to inactivity"), `AC_POWER 0x1000`, `BATTERY_LOW 0x100`, `BATTERY_EXIST 0x80`, `BATTPOWER 0x7F` (unknown).
- Polling: `scePowerIsPowerOnline` (1/0), `IsBatteryExist`, `IsBatteryCharging`, `IsLowBattery`, `GetBatteryLifePercent` (0-100), `GetBatteryLifeTime` (minutes), temperature, volts ([L85-165][psppower]). `scePowerGetBatteryElec` is documented as crashing in user mode ([L157-160][psppower]).
- Clocks: `scePowerSetClockFrequency(pll, cpu, bus)` is user-mode: pll 19-333, cpu 1-333, bus 1-167, `cpu <= pll`, `bus*2 <= pll` ([L215-228][psppower]). A lower clock for an always-on app is possible; effect on stability and power is unverified.
- PPSSPP: always AC online, battery present, 100%, not charging, not low ([scePower.cpp][pp-pow] L199-231). Registering a callback fires it once with `AC_POWER|BATTERY_EXIST|0x64` ([L251-290][pp-pow], arg at L284); no later events. `scePowerRequestSuspend/Standby` are warned no-ops ([L241-249][pp-pow]). `scePowerLock`, `IdleTimer*`, `GetBacklightMaximum` have null handlers ([L617-642][pp-pow]). A slot >= the public slot count returns `SCE_KERNEL_ERROR_PRIV_REQUIRED` ([L255-257][pp-pow]).

### PSP idle behaviour by default
- The only first-party statements are header comments: `PSP_POWER_CB_SUSPENDING` "seems to occur due to inactivity" ([psppower.h][psppower] L36-37) and the `scePowerTick` doc above. Actual timers and the XMB power-save settings are not documented in pspdev sources (open).

## 3. Toolchain on Windows

### Install
- pspdev README: "grabbing the latest development pre-releases from the releases' page for your platform and extract them to the `pspdev` directory", then export `PSPDEV` and add `$PSPDEV/bin` to `PATH` ([README.md][pspdev-readme] L29-36). Latest release `v20261001` (2026-10-01) assets: debian, fedora, macOS (arm64, x86_64), ubuntu (x86_64, arm64). **No Windows asset** ([releases][pspdev-rel]). CI builds only macOS and Ubuntu runners plus Debian/Fedora containers ([compilation.yml][pspdev-ci] L33-42, L98-110).
- Official Windows guide: WSL (`wsl --install`), then in Ubuntu download `pspdev-ubuntu-latest-x86_64.tar.gz`, extract, set `PSPDEV="$HOME/pspdev"` and `PATH`, verify with `psp-config --pspdev-path` ([installation/windows.md][site-win] L10-23, L49-73). The site's download page labels the Ubuntu tarball "Windows/Ubuntu" ([downloads.md][site-dl] L12). Needs packages `build-essential cmake pkgconf libreadline8 libusb-0.1 libgpgme11 libarchive-tools fakeroot wget` ([L34-42][site-win]).
- Docker: `docker pull pspdev/pspdev:latest`; `docker run -ti -v $PWD:/source pspdev/pspdev:latest` ([installation/docker.md][site-docker] L20-30). Windows caveats: absolute Unix-style path (e.g. `/c/Users/...`), code must be under the user's home folder, and `--privileged=true` if `gmake` reports `Operation not permitted` under `psp-cmake` ([L36-42][site-docker]). The image is Alpine with `make`, `cmake`, `git`, `bash`; `PSPDEV=/usr/local/pspdev` ([Dockerfile][pspdev-docker] L35-53). The separate `pspdev/pspdev-docker` repo is archived (last push 2022-02-12), so use the image built from `pspdev/pspdev`.
- PPSSPP's own homebrew page recommends the modern WSL-based PSPSDK over the old Windows-based one ([ppsspp.org homebrew-development][pp-hb]).
- The release build installs the `psp-libraries` package group via `psp-pacman` ([003-psp-packages.sh][pspdev-pk-script] L6). The group includes `sdl`, `sdl2`, `sdl3`, `libintrafont`, `oslib`, `stb`, `curl`, `freetype2` (each `PSPBUILD` has `groups=('psp-libraries')`; [sdl3][pk-sdl3], [libintrafont][pk-intra]). That the release tarball contains them is *(inference)* from CI running the full `build-all.sh` ([compilation.yml][pspdev-ci] L62-69).

### Build
- `psp-cmake` is `cmake -DCMAKE_TOOLCHAIN_FILE="${PSPDEV}/psp/share/pspdev.cmake" "$@"` ([psp-cmake][psp-cmake] L9-11). The toolchain file defines `__PSP__`, `PSP`, `_PSP_FW_VERSION=600` and includes `CreatePBP.cmake` ([pspdev.cmake][pspdevcmake] L33-39).
- Minimal project (official sample): `cmake_minimum_required(VERSION 3.11)`, `project(hello)`, `add_executable(${PROJECT_NAME} main.c)`, `target_link_libraries(... PRIVATE pspdebug pspdisplay pspge)`, then `create_pbp_file(TARGET ... ICON_PATH NULL BACKGROUND_PATH NULL PREVIEW_PATH NULL TITLE ... VERSION 01.00)` ([hello/CMakeLists.txt][site-cmake] L1-21). Build: `mkdir build; cd build; psp-cmake ..; make` produces `EBOOT.PBP` in `build` ([how_to_use.md][site-how] L76-85).
- `create_pbp_file` options: `ICON_PATH` (144x82 png), `BACKGROUND_PATH`/`PREVIEW_PATH` (480x272), `MEMSIZE`, `BUILD_PRX`, `ENC_PRX` etc. ([CreatePBP.cmake][createpbp] L15-34). Post-build it runs `psp-fixup-imports` ([L122][createpbp]), `mksfoex` ([L179][createpbp]) and `pack-pbp` ([L199][createpbp]); strips only when `CMAKE_BUILD_TYPE=Release` ([L98-104][createpbp]).
- The same SDK ships the sample programs under `src/samples` (gu, power, net, utility/systemparam, ...) ([pspsdk tree][pspsdk-samples]).

### Running under PPSSPP from the CLI
- Official docs list `--escape-exit`, `--pause-menu-exit`, `--fullscreen`, `--windowed`, `-d/-v` (log level), `-j/-i/-r/-J` (CPU backend), and Windows-only `-l` (log console), `-s` (don't save settings), `-direct3d11 -vulkan -gles -software` ([ppsspp.org command-line][pp-cli]). Invocation form: `PPSSPPWindows64.exe [options] [FILE]` (usage text, [CmdLine.cpp][pp-cmdline] L291, at the pinned master commit). Latest release is v1.20.4 (2026-05-16) with a `PPSSPP-v1.20.4-Windows-x64.zip` asset ([release][pp-rel]).
- PPSSPP debugger reads symbols from an `.elf` next to the `EBOOT.PBP`, so keep the ELF ([homebrew-development][pp-hb]).
- Headless mode ("primarily meant as a non-interactive test tool") with `--screenshot-save`, `--timeout-emulated`, `--state`, etc. exists at master ([CmdLine.cpp][pp-cmdline] L188-222, usage L271-296). At the v1.20.4 tag `Core/CmdLine.cpp` does not exist and `headless/Headless.cpp` lacks those flags (my grep, 0 matches), so they are newer than the latest release. The headless tool is a separate build target; whether the release zip contains it is unverified.

### What PPSSPP can and cannot emulate for this app
- Can: RTC (host-based, see section 1), 480x272 framebuffer, GU, sysparam reads, bundled firmware fonts `ltn0-15.pgf`, `jpn0.pgf`, `kr0.pgf` ([assets/flash0/font][pp-flash0]), so intraFont's `flash0:/font/*.pgf` has data to load.
- Cannot / fake: brightness (stored, not rendered), `scePowerTick` (no-op), battery (fixed), power callbacks (one initial call), idle suspend/dim, `sceDisplayEnable/Disable`, RTC set. PPSSPP always renders at 32-bit colour regardless of requested framebuffer depth ([known-differences][pp-known]); the OS is "simulated, not emulated" (same page).

## 4. Graphics and text

Screen: LCD, 480x272 ("LCD MAX 480x272 at 59.94 Hz") ([pspdisplay.h][disp-h] L44-52). Pixel formats: 565, 5551, 4444, 8888 ([L21-31][disp-h]). Buffer width ("must be power of 2") is 512 in every official example ([scr_printf.c][scr] L22-24; [gu/text/main.c][gu-text] L70-72).

| Option | Ships by default? | Pros | Cons |
|--------|-------------------|------|------|
| `pspDebugScreen*` | Yes, pspsdk `pspdebug` (hello sample links `pspdebug pspdisplay pspge`) | Zero setup. Init sets 480x272 LCD mode and an 8888 framebuffer at uncached VRAM `0x40000000 \| sceGeEdramGetAddr()` ([scr_printf.c][scr] L136-147). | Fixed 8x8 bitmap font (`msx`, glyph = 8 rows x 8 bits, no scaling) ([L233-262][scr]), so 60x34 characters max *(arithmetic)*. Unsuitable for a big clock face. |
| Raw framebuffer (`sceDisplaySetMode` + `sceDisplaySetFrameBuf`) | Yes, `pspdisplay` | No dependencies. 1 Hz content is trivial to redraw. Pure "render into a buffer" logic is host-testable. Full control of colour/dimming. | You write the rasteriser (digit shapes or bitmap font). No scaling or filtering. See [pspdisplay.h][disp-h] L91-101. |
| libGU (`pspgu`) | Yes, `src/gu` plus samples | Hardware sprites, scaling, alpha. Standard setup: draw buffer 8888 at 512 stride, 480x272 viewport, `sceGuSwapBuffers` per vblank ([gu/text/main.c][gu-text] L154-160, L215-216). | More setup and state to manage. Official docs: textures must have power-of-2 width ([basic_programs.md][site-basic] L92). |
| intraFont (`libintrafont` 4.1) | In `psp-libraries` group ([PSPBUILD][pk-intra]) | Uses firmware PGF fonts: "generates a 4-bit paletted bitmap font texture on the fly ... accurate to 1/4 pixel" ([README][intra-readme]). `intraFontSetStyle(font, float size, color, shadow, angle, options)` ([intraFont.h][intrah] L256) gives scaled text. `INTRAFONT_CACHE_ASCII` preloads ASCII. A clock only needs digits and ':'. | Draws through GU, so GU must be initialised. Known issues: GU texture state must be reset after intraFont calls; more than 100 distinct glyphs per frame can glitch on the default cache ([README][intra-readme]). Large-size quality from a bitmap texture is unverified. |
| SDL (SDL3 / SDL2 / SDL1.2) | `sdl3` 3.4.14, `sdl2` 2.32.10, `sdl` 1.2.15 in `psp-libraries` ([sdl3][pk-sdl3]); PSP backend is upstream SDL ([README-psp.md][sdl-psp]; `src/render/psp`, `src/time/psp`, `src/power/psp`) | Official pspdev samples use SDL3 and the same code "will also build on Linux" ([basic_programs.md][site-basic] L239), so a host preview is possible. SDL3 maps time to `sceRtc*` and battery to `scePower*` ([SDL_systime.c][sdl-time], [SDL_syspower.c][sdl-pow]). SDL_ttf: `sdl3-ttf` 3.2.2 ([PSPBUILD][pk-sdl3ttf]). | Heavier. SDL_ttf needs a `.ttf` file shipped beside the EBOOT ([basic_programs.md][site-basic] L375). SDL README lists dialogs and screen keyboard as to-do ([README-psp.md][sdl-psp] L34-36). `scePowerTick` is not called anywhere in `libsdl-org/SDL` (GitHub code search: 0 hits, so no automatic keep-awake; you must call it yourself). |

- Burn-in: no pspsdk, PPSSPP or pspdev doc source mentions burn-in or image retention (code search of both repos plus a grep of every downloaded doc: 0 hits).
- PPSSPP renders 32-bit regardless of format ([known-differences][pp-known]), so 565 banding or dithering cannot be judged in the emulator.

## 5. Network

- API surface in pspsdk: `pspnet.h` (`sceNetInit(poolsize, calloutprio, calloutstack, netintrprio, netintrstack)`, [L41][pspnet-h]), `pspnet_apctl.h` (`sceNetApctlInit/Connect/GetState/GetInfo/AddHandler`), `pspnet_inet.h`, `pspnet_resolver.h`, plus a libc BSD-socket layer (`src/libcglue/socket.c`, `netdb.c`) ([pspsdk tree][pspsdk-net]).
- Working user-mode sample: load `PSP_NET_MODULE_COMMON` and `PSP_NET_MODULE_INET` with `sceUtilityLoadNetModule`, `sceNetApctlConnect(config)`, poll `sceNetApctlGetState` every 50 ms, read the IP via `sceNetApctlGetInfo(8, ...)`; the sample hardcodes profile index 1 ([net/simple/main.c][net-sample] L183-215, L238-243, L259-260). The `utility/netconf` and `utility/netdialog` samples use the system net-config dialog (`sceUtilityNetconfInitStart`) so the user picks the profile ([netdialog][netdialog-sample] L211-227).
- No NTP/SNTP client exists anywhere in the pspdev org (code search `sntp`: no relevant hits). You would write a UDP/123 client over the libc sockets; not verified end to end. `curl` 7.64.1 with mbedtls is also packaged ([curl PSPBUILD][pk-curl] L9).
- Constraint implied by the sample: a Wi-Fi profile must already be configured in the XMB (the app connects by profile index).
- PPSSPP: `EnableWlan` defaults to `false` ([Config.cpp][pp-cfg] L1065). `sceNetApctlConnect` returns `SCE_NET_APCTL_ERROR_WLAN_SWITCH_OFF` when it is off ([sceNet.cpp][pp-net] L1512-1514). Infrastructure mode is built around online games and an infrastructure DNS server setting ([Config.cpp][pp-cfg] L1071); `sceNetInetSocket` creates host sockets through `g_socketManager` ([sceNetInet.cpp][pp-inet] L459-470). Whether a UDP NTP exchange works end to end through it is unverified.
- Because PPSSPP's RTC is the host clock (section 1), an NTP path cannot be meaningfully validated there beyond plumbing.
- Time without network: the XMB date/time is the source of truth and the app only reads (section 1). This is the only mechanism with first-party API support in user mode.
- Recommendation: keep network out of v1. Treat NTP as a later opt-in feature with a clear plan for how it would set time (user-mode apps cannot set the RTC, so it could only correct a displayed offset).

## Implications for design

1. **Time is an injected port.** Put "now" behind an interface returning (y, m, d, h, mi, s, us) plus 12/24h and date-order preferences. All formatting, date layout and rollover logic is pure and unit-testable on the Windows host with no SDK. The PSP adapter is a thin wrapper over `sceRtcGetCurrentClockLocalTime` and `sceUtilityGetSystemParamInt`.
2. **v1 fits entirely in user mode.** Reading the RTC, `scePowerTick`, power callbacks, battery polling, sysparams and `scePowerSetClockFrequency` are all user-mode. Hardware brightness and real display-off are kernel/driver-only, so do not plan them as features. A software "dim" (darker palette, or a black frame) is the user-mode substitute.
3. **Pick one source of local time and verify it on hardware.** In PPSSPP, libc `localtime()` follows the emulated sysparam (default GMT+0), while the `sceRtc` local-time calls follow the host timezone. Prefer the `sceRtc` path (what SDL3 uses) and add a hardware check for the DST flag. Do not mix the two.
4. **Keep-awake and power events live behind a PowerPort.** `scePowerTick`, the callbacks and the battery values are no-ops or constants in PPSSPP, so none of it is testable there. Inject fake battery/AC/suspend events in tests. On resume, re-read the RTC instead of accumulating frame counts.
5. **Rendering is 1 Hz, so cost is not a concern.** A custom segment or bitmap renderer on the raw framebuffer or GU gives big digits with no dependencies and can be pure and host-testable. intraFont and SDL3_ttf are viable but add dependencies (GU state, a TTF file). The debug screen's 8x8 font is too small.
6. **Toolchain: WSL or Docker, not native Windows.** Edit on Windows, build with `psp-cmake .. && make`, run the resulting `EBOOT.PBP` in the Windows PPSSPP. Keep the ELF next to it for the debugger. The project's CMake file is about 20 lines.
7. **Automated screenshot testing in PPSSPP is a maybe.** Headless flags are on master but not in the v1.20.4 tag. Plan unit tests on pure logic first, and treat PPSSPP runs as manual smoke tests until headless availability is confirmed.
8. **Network is out of v1.** No NTP code exists in pspdev, setting the RTC is kernel-only, and PPSSPP has WLAN off by default. If added later, it is an opt-in feature behind its own port.

## Unverified / open

Confirm on real hardware or in headers:

- Per-flag meaning of `scePowerTick` types 1 and 6, whether they stop backlight dim and/or display-off, and how often a tick must be sent. (The header gives only names and a one-line doc.)
- Whether `sceRtcGetCurrentClockLocalTime` and `sceRtcConvertUtcToLocalTime` apply the XMB DST flag, and where their offset comes from.
- How newlib treats a `TZ` of the form `GMT+05:00daylight` with no DST rule (the string pspsdk builds when the DST flag is set).
- Real RTC drift, battery-backed behaviour across power-off, and whether the XMB can set time "via internet" (no pspdev source found).
- Real-hardware value of `sceRtcGetTickResolution` and actual RTC granularity.
- Whether CFW runs an EBOOT with `PSP_MODULE_KERNEL` (0x1000) from the Game menu in kernel mode, and what a kernel-mode path to `sceDisplaySetBrightness`, `sceImposeSetParam` or `sceRtcSetCurrentTick` would require (PRX/CFW plugin).
- Value ranges and effect of `sceDisplaySetBrightness` (header says 0-100) vs `sceImposeSetParam(BACKLIGHT_BRIGHTNESS)` (PPSSPP implies 0-3); whether `sceDisplayGetBrightness` can be imported from user mode as psplibdoc suggests.
- Whether `sceImposeSetBacklightOffTime` persists to the system setting, and the signature/use of `scePowerGetBacklightMaximum`.
- Whether the default idle suspend still triggers on AC power; XMB timers are not documented in pspdev sources.
- Stability and power effect of low CPU/bus clocks with GU/SDL rendering.
- intraFont quality at large sizes (bitmap texture upscaling).
- PPSSPP: whether the Windows release zip includes a headless binary and whether the headless flags ship in a release; whether Windows PPSSPP loads an `EBOOT.PBP` straight from a `\\wsl$` path; real size of virtual-time drift vs host clock; whether UDP NTP works through its `sceNetInet` layer with WLAN enabled.
- Whether the release tarball contains the `psp-libraries` group (inferred from CI, not checked in the tarball).
- No source found on burn-in or LCD image retention for any PSP model.

## Sources

Pinned commits (all fetched 2026-10-17):
- pspdev/pspsdk `6ef2b21dc440ef8d4125f000301faaa2a470975e`
- hrydgard/ppsspp `52ba05d9e7a416c9bc182ed7c3e63475edf6abd0` (master); release v1.20.4 tag `3a31057b7e44270b4d5cef8c31b6559d51802a3b`
- pspdev/pspdev `99dee465c534679f16cc7e0d9fc6a5b92d759d7d`; pspdev/pspdev.github.io `a844c0970915b83bc3ead9ba67643eeb1e0ca6cd`
- pspdev/psp-packages `7077dcaa14a2e187b8ef120aa4e449608b098f9a`; pspdev/psptoolchain-extra `b5f01b00e428a604832e0dfb4bfbb991d1397e3f`
- libsdl-org/SDL `d5b715173c7ea7afbc4ddbe159063e71c2f349ff`; pspdev/psplibdoc `5e15a3cff6c3642f3217b93aeb2f6b759cee1b02`

pspsdk:
[psprtc]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/rtc/psprtc.h
[psptypes]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/base/psptypes.h
[rtc-user]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/rtc/sceRtc.S
[rtc-drv]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/rtc/sceRtc_driver.S
[sysparam]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/utility/psputility_sysparam.h
[tz]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/libcglue/timezone.c
[cinit]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/libcglue/init.c
[glue]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/libcglue/glue.c
[psppower]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/power/psppower.h
[pow-s]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/power/scePower.S
[pow-sample]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/samples/power/main.c
[disp-h]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/display/pspdisplay.h
[disp-k]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/display/pspdisplay_kernel.h
[disp-drv]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/display/sceDisplay_driver.S
[imp-user]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/user/pspimpose.h
[imp-drv]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/kernel/pspimpose_driver.h
[imp-drv-s]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/kernel/sceImpose_driver.S
[modinfo]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/user/pspmoduleinfo.h
[buildmak]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/base/build.mak
[pspdevcmake]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/base/pspdev.cmake
[createpbp]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/base/CreatePBP.cmake
[scr]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/debug/scr_printf.c
[gu-text]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/samples/gu/text/main.c
[pspnet-h]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/net/pspnet.h
[net-sample]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/samples/net/simple/main.c
[netdialog-sample]: https://github.com/pspdev/pspsdk/blob/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/samples/utility/netdialog/main.c
[pspsdk-samples]: https://github.com/pspdev/pspsdk/tree/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/samples
[pspsdk-net]: https://github.com/pspdev/pspsdk/tree/6ef2b21dc440ef8d4125f000301faaa2a470975e/src/net

PPSSPP (source at master commit; docs on ppsspp.org):
[pp-rtc]: https://github.com/hrydgard/ppsspp/blob/52ba05d9e7a416c9bc182ed7c3e63475edf6abd0/Core/HLE/sceRtc.cpp
[pp-ktime]: https://github.com/hrydgard/ppsspp/blob/52ba05d9e7a416c9bc182ed7c3e63475edf6abd0/Core/HLE/sceKernelTime.cpp
[pp-pow]: https://github.com/hrydgard/ppsspp/blob/52ba05d9e7a416c9bc182ed7c3e63475edf6abd0/Core/HLE/scePower.cpp
[pp-disp]: https://github.com/hrydgard/ppsspp/blob/52ba05d9e7a416c9bc182ed7c3e63475edf6abd0/Core/HLE/sceDisplay.cpp
[pp-imp]: https://github.com/hrydgard/ppsspp/blob/52ba05d9e7a416c9bc182ed7c3e63475edf6abd0/Core/HLE/sceImpose.cpp
[pp-util]: https://github.com/hrydgard/ppsspp/blob/52ba05d9e7a416c9bc182ed7c3e63475edf6abd0/Core/HLE/sceUtility.cpp
[pp-net]: https://github.com/hrydgard/ppsspp/blob/52ba05d9e7a416c9bc182ed7c3e63475edf6abd0/Core/HLE/sceNet.cpp
[pp-inet]: https://github.com/hrydgard/ppsspp/blob/52ba05d9e7a416c9bc182ed7c3e63475edf6abd0/Core/HLE/sceNetInet.cpp
[pp-cfg]: https://github.com/hrydgard/ppsspp/blob/52ba05d9e7a416c9bc182ed7c3e63475edf6abd0/Core/Config.cpp
[pp-gss]: https://github.com/hrydgard/ppsspp/blob/52ba05d9e7a416c9bc182ed7c3e63475edf6abd0/UI/GameSettingsScreen.cpp
[pp-cmdline]: https://github.com/hrydgard/ppsspp/blob/52ba05d9e7a416c9bc182ed7c3e63475edf6abd0/Core/CmdLine.cpp
[pp-flash0]: https://github.com/hrydgard/ppsspp/tree/52ba05d9e7a416c9bc182ed7c3e63475edf6abd0/assets/flash0/font
[pp-rel]: https://github.com/hrydgard/ppsspp/releases/tag/v1.20.4
[pp-cli]: https://www.ppsspp.org/docs/reference/command-line/
[pp-hb]: https://www.ppsspp.org/docs/development/homebrew-development/
[pp-known]: https://www.ppsspp.org/docs/development/known-differences/

pspdev toolchain and docs:
[pspdev-readme]: https://github.com/pspdev/pspdev/blob/99dee465c534679f16cc7e0d9fc6a5b92d759d7d/README.md
[pspdev-docker]: https://github.com/pspdev/pspdev/blob/99dee465c534679f16cc7e0d9fc6a5b92d759d7d/Dockerfile
[pspdev-ci]: https://github.com/pspdev/pspdev/blob/99dee465c534679f16cc7e0d9fc6a5b92d759d7d/.github/workflows/compilation.yml
[pspdev-pk-script]: https://github.com/pspdev/pspdev/blob/99dee465c534679f16cc7e0d9fc6a5b92d759d7d/scripts/003-psp-packages.sh
[pspdev-rel]: https://github.com/pspdev/pspdev/releases/tag/v20261001
[psp-cmake]: https://github.com/pspdev/psptoolchain-extra/blob/b5f01b00e428a604832e0dfb4bfbb991d1397e3f/patches/psp-cmake
[site-win]: https://github.com/pspdev/pspdev.github.io/blob/a844c0970915b83bc3ead9ba67643eeb1e0ca6cd/installation/windows.md
[site-docker]: https://github.com/pspdev/pspdev.github.io/blob/a844c0970915b83bc3ead9ba67643eeb1e0ca6cd/installation/docker.md
[site-dl]: https://github.com/pspdev/pspdev.github.io/blob/a844c0970915b83bc3ead9ba67643eeb1e0ca6cd/downloads.md
[site-how]: https://github.com/pspdev/pspdev.github.io/blob/a844c0970915b83bc3ead9ba67643eeb1e0ca6cd/how_to_use.md
[site-basic]: https://github.com/pspdev/pspdev.github.io/blob/a844c0970915b83bc3ead9ba67643eeb1e0ca6cd/basic_programs.md
[site-cmake]: https://github.com/pspdev/pspdev.github.io/blob/a844c0970915b83bc3ead9ba67643eeb1e0ca6cd/_includes/samples/hello/CMakeLists.txt
[site-hello]: https://github.com/pspdev/pspdev.github.io/blob/a844c0970915b83bc3ead9ba67643eeb1e0ca6cd/_includes/samples/hello/main.c
[pk-sdl3]: https://github.com/pspdev/psp-packages/blob/7077dcaa14a2e187b8ef120aa4e449608b098f9a/sdl3/PSPBUILD
[pk-sdl3ttf]: https://github.com/pspdev/psp-packages/blob/7077dcaa14a2e187b8ef120aa4e449608b098f9a/sdl3-ttf/PSPBUILD
[pk-intra]: https://github.com/pspdev/psp-packages/blob/7077dcaa14a2e187b8ef120aa4e449608b098f9a/libintrafont/PSPBUILD
[pk-curl]: https://github.com/pspdev/psp-packages/blob/7077dcaa14a2e187b8ef120aa4e449608b098f9a/curl/PSPBUILD
[pspkg]: https://github.com/pspdev/psp-packages/tree/7077dcaa14a2e187b8ef120aa4e449608b098f9a
[intrah]: https://github.com/pspdev/libintraFont/blob/fd8aabd12dacbfe7b42a387e5beeb1c2a738ba26/include/intraFont.h
[intra-readme]: https://github.com/pspdev/libintraFont/blob/master/README.md
[sdl-psp]: https://github.com/libsdl-org/SDL/blob/d5b715173c7ea7afbc4ddbe159063e71c2f349ff/docs/README-psp.md
[sdl-time]: https://github.com/libsdl-org/SDL/blob/d5b715173c7ea7afbc4ddbe159063e71c2f349ff/src/time/psp/SDL_systime.c
[sdl-pow]: https://github.com/libsdl-org/SDL/blob/d5b715173c7ea7afbc4ddbe159063e71c2f349ff/src/power/psp/SDL_syspower.c
[ld-disp]: https://github.com/pspdev/psplibdoc/blob/5e15a3cff6c3642f3217b93aeb2f6b759cee1b02/PSPLibDoc/kd/display.csv
[ld-rtc]: https://github.com/pspdev/psplibdoc/blob/5e15a3cff6c3642f3217b93aeb2f6b759cee1b02/PSPLibDoc/kd/rtc.csv

Notes on provenance: psplibdoc is a community NID database hosted in the pspdev org (reverse-engineered, not Sony documentation) and PPSSPP code comments reflect the emulator authors' observations; both are first-party to their projects but secondary for hardware behaviour. The libintraFont README link points at `master`, not a pinned commit.
