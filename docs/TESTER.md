# Hardware test menu

`demos/tester.lua` is a single-file, on-device test menu for every OpenLua
module. It is meant to be run from `EBOOT.PBP` on a real PSP, but it is also
useful in PPSSPP: the same menu runs there and marks anything the emulator
cannot provide as `INFO` or `SKIP` instead of failing.

The menu was originally written without real PSP hardware. It is verified on
PPSSPP v1.20.4 and on a physical PSP-1000 (firmware 6.61); PSP-2000/3000,
PSP-E1000 and PSP Go runs are still pending. Treat PPSSPP results as emulator
verification only.

## Supported models

The menu runs on every PSP model with a custom firmware that can launch
homebrew:

| Model | Common name | Notes |
|---|---|---|
| PSP-1000 | Fat | 32 MB RAM, no `ef0:` |
| PSP-2000 / PSP-3000 | Slim | 64 MB RAM, no `ef0:` |
| PSP-E1000 | Street | no WLAN, no `ef0:` |
| PSP-N1000 | Go | internal storage is `ef0:`; M2 is `ms0:` |

`hw.getmodel()` can only reliably distinguish the Go, so `SELECT` cycles a
**declared profile** (Fat / Slim / 3000 / Street / Go). The choice is saved to
`tester.cfg` next to the EBOOT and is written into the report. Set it to match
the physical unit before collecting results.

## Deploy

Build the engine first:

```bash
bash docker/build.sh
```

Then stage the menu with `EBOOT.PBP`, `script.lua` and `assets/`:

```bash
bash tools/package_tester.sh
```

The output defaults to `out/tester-package/`:

```text
PSP/GAME/OpenLua/EBOOT.PBP
PSP/GAME/OpenLua/script.lua
PSP/GAME/OpenLua/assets/
PSP/GAME/OpenLuaProbe/EBOOT.PBP   (when the probe was built)
README.txt
```

Copy that `PSP/` directory to the root of a Memory Stick (`ms0:`) or the PSP Go
internal storage (`ef0:`) and launch **OpenLua** from the Game menu. You can
also copy the three items by hand instead of running the script.

## Boot probe (8002013C)

`tools/bootprobe` builds a deliberately minimal EBOOT (`PSP/GAME/OpenLuaProbe`)
that imports only the core display/input/IO libraries. It asks the firmware to
load the optional net, AV codec, USB camera and IrDA modules, then writes
`openlua_probe.txt` next to itself and to `ms0:/` and `ef0:/`.

Use it when the main EBOOT reports `8002013C`
(`SCE_KERNEL_ERROR_LIBRARY_NOTFOUND`):

- **Probe runs, tester does not** — the console can run homebrew; the tester
  still imports a library this firmware cannot resolve. Send the probe log.
- **Probe also fails to start** — the problem is the CFW/HEN or the launcher,
  not the OpenLua build.

Build it locally with `bash docker/build.sh all probe` (or `make probe` inside
the PSPDEV environment). `tools/package_tester.sh` includes it automatically
when `tools/bootprobe/EBOOT.PBP` exists; override with `OPENLUA_PROBE_EBOOT`.

The menu needs no bundled system font. It tries, in order:

1. `flash0:/font/ltn0.pgf` through the PGF loader (opt-in builds only; the
   standard EBOOT is built with `OPENLUA_NO_PGF`, so this step returns `nil`),
2. `assets/font.png` as a fixed-cell font,
3. a built-in 5x7 bitmap font.

The standard PSP build does not link `sceLibFont`: on real hardware it is
provided by the per-game user module `libfont.prx`, and a static import makes
the firmware reject the EBOOT with `8002013C`. The PNG and built-in fallbacks
therefore keep the menu readable without PGF.

## Controls

| Input | Action |
|---|---|
| UP / DOWN | move the selection or scroll a result page |
| X or O | open the selected test page |
| O | go back |
| X | run / re-run the page, or trigger its documented action |
| SELECT | cycle the declared model (saved to `tester.cfg`) |
| START | run all non-interactive checks and write `test_report.txt` |

The key used to open a page is consumed by the menu and does not act on the
page in the same frame. On a live-demo page (GRAPHICS, FONTS, GAME, 3D,
CAMERA), press **X** to start the demo, then **O** to leave the demo and
return to the page, then **O** again to return to the menu. On a result page,
**O** returns to the menu directly. START and SELECT do not start a live demo.

Every test call is wrapped in `pcall`, so a broken module shows a `FAIL` line
instead of taking the menu down.

## Pages

| Page | Covers |
|---|---|
| SYSTEM / OS / HARDWARE / POWER | `os`, `hw`, `batt`, `power` |
| STORAGE / FILES | `files`, `io`, `files.cdir`, `ms0`/`ef0`, `app0` |
| SAVEDATA | `data` save slots, metadata, limits, delete |
| INI / JSON | `ini`, `json` round trips and rejection paths |
| INPUT / TIMERS / MATH | `buttons`, `timer`, `math` helpers |
| GRAPHICS / DRAW / IMAGE | `image`, `draw`, `screen`, `color` |
| FONTS / SFONT | `font` PGF/fixed-cell, `sfont` |
| GAME / MAP | `game.info`/artwork, `map` |
| SOUND / AUDIO | `sound` WAV load, volume, loop, play/pause/stop |
| 3D / AMG / CAM3D / MODELS | `amg`, `cam3d`, `texture3d`, `model3d` |
| SPLASH | `splash.show` / `old` / `oldschool` |
| ON-SCREEN KEYBOARD | `osk.init` |
| PMF / PSMF VIDEO | `pmf.run` and error paths |
| GO! CAM USB CAMERA | `cam` lifecycle, render, snapshot |
| UMD / USB / SERIAL / IR / BT | `umd`, `usb`, `sio`, `ir`, `bluetooth`, `adhoc` |
| WLAN / SOCKET / HTTP / FTP | `wlan`, `socket`, `http`, `ftp`, `adhoc` |
| BATCH REPORT | the last START results and the report path |

## Interactive actions

Hardware and state-changing actions only run when you press the key shown in
the page footer. Nothing is started at boot.

| Page | Key | Action |
|---|---|---|
| SOUND | X / TRIANGLE / SQUARE | play or stop / cycle volume / toggle loop |
| SPLASH | X / TRIANGLE / SQUARE | `splash.show` / `splash.old` / `splash.oldschool` |
| OSK | X | open the system keyboard |
| PMF | X | play `movie.pmf` if present |
| CAMERA | X | live camera preview (SQUARE takes a snapshot inside the demo) |
| UMD / USB | X / TRIANGLE / SQUARE / L / R | UMD mount / USB mass storage / stop / USB UMD (SKIP) / USB flash0 (SKIP) |
| NETWORK | X / TRIANGLE / SQUARE / L / R | WLAN connect / disconnect / FTP toggle / adhoc init / adhoc term |

The following are never called automatically and are listed in every report:

- `os.exit`, `os.restart`
- `power.suspend`, `power.off`, `power.lock`/`unlock`
- flash writes
- the FTP server (only the explicit SQUARE toggle starts it)

USB mass storage exposes the Memory Stick or internal storage over USB until
`usb.stop()`; press SQUARE on the UMD/USB page to stop it. The USB mass
storage and GO! CAM modes cannot be active at the same time.

`usb.umd()` and `usb.flash*()` are no-ops in the standard build. They need
Sony's optional `_usbdevice.prx`; importing it statically makes the EBOOT fail
to launch with `8002013C`, so the build omits it. L/R on this page therefore
record `SKIP`, not `PASS`.

## Report

START runs every non-interactive page and writes `test_report.txt`:

- in the app directory (next to `EBOOT.PBP`),
- in `writable_base()/test_report.txt` when that is a real directory,
- in `ms0:/PSP/OPENLUA_TEST/` and `ef0:/PSP/OPENLUA_TEST/` when available.

The report starts with the version, date, declared model, detected model,
firmware, nickname/CFW and free space. Each section ends with a
`pass/fail/info/skip` count, and the file ends with totals plus the list of
things that were intentionally not run.

Status meanings:

- `PASS` — the call completed and the expected result was observed.
- `FAIL` — the call raised or returned an unexpected value. This is a bug.
- `INFO` — a value was read but cannot be judged in this environment, or the
  asset/hardware is absent. Not a failure.
- `SKIP` — needs a key press, real hardware, or is intentionally not run.
  Not a failure.

The menu also keeps the latest batch result on the **BATCH REPORT** page until
the app exits.

## Current PPSSPP result

The full START batch on PPSSPP v1.20.4 (SDL offscreen + Mesa llvmpipe OpenGL)
produced, with the standard PGF-disabled PSP build:

```text
TOTAL 225 checks: 185 pass, 0 fail, 21 info, 19 skip
```

`INFO` items are values PPSSPP cannot emulate (for example `ef0:` absent,
UMD metadata, kernel-only hardware IDs, battery capacity/voltage). `SKIP`
items are interactive-only tests (including camera init/begin/render), the
deliberately unrun destructive calls, and `FONT.LOAD PGF` (the standard build
does not link `sceLibFont`; an opt-in PGF build adds the remaining checks).
Zero `FAIL` confirms that every module's non-interactive surface works in the
emulator; it does **not** prove real-hardware behaviour.

## Current real-hardware result

A PSP-1000 (FAT) on firmware 6.61, launched from `ms0:/PSP/GAME/OpenLua/`,
produced the same totals with the standard PGF-disabled build:

```text
TOTAL 225 checks: 185 pass, 0 fail, 21 info, 19 skip
```

The header reports `declared model PSP-1000 (FAT)`, `detected model UNK`,
firmware `6.61`, nickname `PSP 1001` and CFW `UNK`. `detected model UNK` is
expected: user mode can only identify the Go, so the SELECT profile is the
authoritative model on 1000/2000/3000/E1000. `ef0:` was absent, as expected on
a 1000, so the EF0 and cross-volume rows are `SKIP`.

This run also covers the `hw.getmodel()` fix: the 1000's CFW exposes an empty
`ef0:` stub (`statvfs` succeeds with zero capacity), which previously made the
menu report `Go`. Detection now requires non-zero capacity and matches
`os.infoef0()`.

Battery telemetry, power state and the `ms0:` file/savedata paths returned real
values. `INFO` items are values the menu does not judge (battery capacity) or
cannot read in user mode (kernel-only hardware IDs, UMD metadata). The
interactive rows were not run, so this does not verify OSK, PMF playback,
camera capture, USB, FTP, adhoc or a WLAN connection on hardware.

Things PPSSPP cannot verify and that need a real PSP:

- `ef0:` internal storage and cross-volume copy/move on the PSP Go.
- Real UMD discs (`gameid`, title, size). The USB flash/UMD modes are
  unavailable in the standard build, so they need a custom build with
  `_usbdevice.prx`, not just real hardware.
- Battery voltage/capacity and temperature on all models.
- The GO! CAM accessory and real camera frames.
- WLAN profiles, access points, adhoc peers and an actual connection.
- Infrared and serial hardware (the user-mode build is a documented stub).
- PGF rendering from the real `flash0:` font (PGF is disabled in the standard
  build; use an opt-in `OPENLUA_NO_PGF`-less build that loads `libfont.prx`).
- PMF playback and the OSK cancel/confirm variants (PPSSPP's OSK HLE has no
  cancel control).
- `power.lock`/`unlock` on firmware where PSPSDK HLE is missing in PPSSPP.

When reporting results on real hardware, include the model, firmware/CFW,
whether `ms0:` or `ef0:` was used, and the full `test_report.txt`.
