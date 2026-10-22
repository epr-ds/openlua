# Development Guide

## Prerequisites

- Docker (recommended) or a local PSPSDK toolchain (`psp-gcc`, `psp-ld`, ...).
- `gcc`, `make` and `bash` for the host tests.
- Python 3 for asset generators.

The build script uses `pspdev/pspdev:latest` through a local
`openlua-pspdev:local` builder image that adds `ccache`.

## Build

```bash
# Standard incremental build (Docker)
bash docker/build.sh

# Clean rebuild; run the two steps separately
bash docker/build.sh clean
bash docker/build.sh
```

The first build creates the builder image and populates
`docker/.cache/ccache`; subsequent builds, including clean rebuilds, reuse
cached compiler output. The cache directory is git-ignored and can be deleted
at any time with `rm -rf docker/.cache/ccache`.

Environment overrides:

| Variable | Default | Purpose |
|---|---|---|
| `JOBS` | detected CPU count | Parallel `make` jobs |
| `PSPDEV_IMAGE` | `pspdev/pspdev:latest` | Base toolchain image |
| `OPENLUA_BUILDER_IMAGE` | `openlua-pspdev:local` | Cached builder image tag |

When not running as root, the container uses the current user ID so generated
objects and `EBOOT.PBP` stay owned by you.

Outputs:

- `EBOOT.PBP` — deploy to `PSP/GAME/OpenLua/`
- `openlua.elf` — intermediate ELF

Do not run `bash docker/build.sh clean all` in one invocation: the PSPSDK
makefile parallelises them and the link can race.

Local toolchain build:

```bash
make            # uses $(psp-config --pspsdk-path)
make clean
```

## Host tests

Host-testable pieces (VFS, timers, OneLua math/battery/OS helpers, power/OS bindings, transform math, Lua modules):

```bash
bash test/host/run.sh
```

Expected: `olfs: 37 checks, 0 failed`, `transform: 144 checks, 0 failed`,
`psmf: 20 checks, 0 failed`, `battery: 16 checks, 0 failed`,
`RESULT: 10 passed, 0 failed` for the OS C helpers,
`maps: 26 checks, 0 failed`, `sound: 48 checks, 0 failed` (301 C checks
total), then `RESULT: 46 passed, 0 failed`, `RESULT: 21 passed, 0 failed` for
timers, `RESULT: 23 passed, 0 failed` for math, `RESULT: 24 passed, 0 failed`
for INI, `RESULT: 89 passed, 0 failed` for JSON, `RESULT: 64 passed, 0 failed`
for savedata, `RESULT: 22 passed, 0 failed` for battery,
`RESULT: 14 passed, 0 failed` for power, `RESULT: 61 passed, 0 failed` for
the OS Lua surface, and `RESULT: 13 passed, 0 failed` for sound (377 Lua
assertions total). Build output is kept in `out/build.log`; the script prints it
only when a build fails.

Repository hygiene check:

```bash
bash tools/check_repo_hygiene.sh
```

This fails if `*.o`, `*.elf`, `*.pbp`, `EBOOT.PBP`, `PARAM.SFO` or an AppImage
is tracked.

The script builds all host targets with `-Wall -Wextra -Werror` and runs the C
tests (`test_olfs.c`, `test_transform.c`, `test_psmf.c`, `test_batt.c`,
`test_os.c`, `test_maps.c`, `test_sound.c`) plus the Lua suites for
files/INI/JSON/savedata/timers/math/battery/power/os/sound in `out/`. Compiler
output is captured in `out/build.log` and shown only if a build fails.

## Project layout

```text
engine/
  core/   main.c, olreg.c/h, stb_impl.c
  gfx/    gfx.c/h (surface + GE), draw.c/h (primitives)
  img/    image.c/h (Lua image module, stb decode)
  font/   font.c/h (SFont)
  snd/    sound.c/h (WAV/VMA + PSP audio)
  input/  buttons.c/h
  util/   timers.c/h, olmath.c/h (OneLua math helpers)
  fs/     olfs.c/h (platform VFS), files.c/h, ini.c/h
  data/   json.c/h
  sys/    savedata.c/h, battery.c/h, power.c/h, os.c/h
  maps/   maps.c/h
third_party/
  lua/src Lua 5.1.5
  stb/    stb_image.h, stb_image_write.h
assets/   demo/test assets
demos/    runnable Lua demos
docs/     architecture, API, roadmap, reference
test/     host and on-device tests
tools/    asset generators and local PPSSPP
```

## Adding a Lua module

1. Create `engine/<area>/<name>.c` and `.h` with a
   `void <name>_api_register(lua_State *L)` function.
2. Build the table with `lua_newtable` + `ol_setfuncs` + `lua_setglobal`.
3. Declare the register function in the header and call it from
   `register_api()` in `engine/core/main.c`.
4. Add the object file to `OBJS` in `Makefile`.
5. Prefer pure logic in a separate function so it can be host-tested.
6. Document the API in `docs/API.md` and add a demo/test if user-facing.

Keep handle tables small and fixed unless there is a clear ownership reason to
allocate dynamically. If a module stores a pointer owned by another Lua value
(for example a map's tileset), anchor it with a registry reference; see #26.

## Debugging boot

- `openlua.log` in the app directory receives boot markers and Lua errors.
- `flash0:/openlua_boot.txt` (with `app0:` fallback) receives short early-boot
  markers from `main.c`.
- On a Lua error, the engine closes the state and displays the error on the
  debug screen.
- In PPSSPP: disable software rendering and enable fast memory if needed.

If `main()` never runs, the failure is before Lua starts: check the EBOOT,
toolchain link order and module imports (#6, #7).

## Storage paths

| Context | Paths |
|---|---|
| PSP app directory | `app0:/`, current directory after `chdir_to_appdir` |
| Memory Stick / M2 | `ms0:/` |
| PSP Go internal storage | `ef0:/` |
| Host tests | `./`, `out/` |

`chdir_to_appdir` uses `argv[0]` and falls back to `app0:/` when the launch
path is unavailable, so relative `script.lua`/asset paths work from Memory
Stick and PSP Go internal storage. `files.*` accepts either volume explicitly.
Savedata creates `PSP/SAVEDATA` on the first writable root (`ms0:` then
`ef0:`), keeps an existing game on its current volume, and searches both
volumes for loads and deletes.

## Coding conventions

- C99, 4-space indent, braces on the same line for functions and control flow.
- Keep platform code inside `#if defined(__PSP__)`; expose a portable core.
- Check PSP API return values; audio and file calls can fail when a channel or
  path is unavailable.
- Pixel buffers are ABGR and must use the `realW` stride, never the logical
  width, when addressing rows.
- Power-of-two padding is mandatory for GE textures.
- CPU writes to texture memory must be visible before the GE reads them
  (cache writeback). Prefer a targeted range over a global flush.
- Do not log secrets or dump whole user files.

## Common pitfalls

| Symptom | Likely cause |
|---|---|
| Blank frame after `screen.flip()` | Frame clear/swap ordering (#19) |
| Garbled scaled sprite | Wrong stride or non-power-of-two upload (#20) |
| Save not found on PSP | Save root probe did not create the directory (#24) |
| `power.off()` enters standby instead of switching off | PSPSDK has no user-mode software shutdown; use the hardware power switch (#34) |
| Script unexpectedly relaunches | `os.restart()` is destructive and PSP-only; keep it behind an explicit user action (#35) |
| `os.password()`/`os.cfw()` look empty or `UNK` | PSPSDK has no public password or reliable CFW query; these are documented fallbacks (#35) |

## PSP model compatibility

| Model | Application storage | Save/file roots | Status |
|---|---|---|---|
| PSP-1000 / 2000 / 3000 | `ms0:` | `ms0:` | Code path and host tests; device verification pending |
| PSP Go N1000 | `ef0:` internal or `ms0:` M2 | `ef0:` internal + `ms0:` M2 | Code path and host tests; device/PPSSPP verification pending (#31) |
| Host tests | `./` | `./` + `data/` | Covered by the host suite |

The build targets the 6.00+ PSPSDK API (`_PSP_FW_VERSION=600`) and is intended
for current custom firmware. Verify boot, controller, display and audio on
real hardware before release.

## Continuous integration

`.github/workflows/build.yml` runs on every push and pull request:

1. host `test/host/run.sh` plus the repository hygiene check;
2. PSPSDK Docker build producing `EBOOT.PBP`;
3. `EBOOT.PBP` artifact upload.
