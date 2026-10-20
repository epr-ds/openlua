# Development Guide

## Prerequisites

- Docker (recommended) or a local PSPSDK toolchain (`psp-gcc`, `psp-ld`, ...).
- `gcc`, `make` and `bash` for the host tests.
- Python 3 for asset generators.

The container image used by the build script is `pspdev/pspdev:latest`.

## Build

```bash
# Standard build (Docker)
bash docker/build.sh

# Clean rebuild; run the two steps separately
bash docker/build.sh clean
bash docker/build.sh
```

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

Only the storage layer is PSP-free today:

```bash
bash test/host/run.sh
```

Expected: `transform: 144 checks, 0 failed` followed by
`RESULT: 46 passed, 0 failed`.

Repository hygiene check:

```bash
bash tools/check_repo_hygiene.sh
```

This fails if `*.o`, `*.elf`, `*.pbp`, `EBOOT.PBP`, `PARAM.SFO` or an AppImage
is tracked.

The script builds Lua 5.1 from `third_party/lua/src`, links
`engine/core/olreg.c`, `engine/fs/*`, and `engine/sys/savedata.c`, then runs
`test/host/test_phase3.lua` in `out/`.

Adding coverage for timers, maps, sound parsing and transform math is tracked
in #28.

## Project layout

```text
engine/
  core/   main.c, olreg.c/h, stb_impl.c
  gfx/    gfx.c/h (surface + GE), draw.c/h (primitives)
  img/    image.c/h (Lua image module, stb decode)
  font/   font.c/h (SFont)
  snd/    sound.c/h (WAV/VMA + PSP audio)
  input/  buttons.c/h
  util/   timers.c/h
  fs/     olfs.c/h (platform VFS), files.c/h, ini.c/h
  sys/    savedata.c/h
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
| Memory Stick | `ms0:/` |
| PSP Go internal storage | internal volume / M2 (see #31) |
| Host tests | `./`, `out/` |

`chdir_to_appdir` uses `argv[0]`, so launching through a different directory
can change relative asset paths. Keep assets relative to the app directory.

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
| `attempt to index ... (userdata)` | Image has no `__index`; use `image.foo(img)` (#27) |
| Negative timer value | Operand order in `timers.c` (#21) |
| Save not found on PSP | Save root probe did not create the directory (#24) |
| Audio silent on a chosen channel | Channel was not reserved (#25) |

## PSP Go notes

PSP Go support is tracked in #31. Until it lands, treat `ms0:` as the only
guaranteed storage root and test both internal storage and M2 when making
storage changes.

## Continuous integration

There is no CI workflow yet (#28). The intended jobs are:

1. host `test/host/run.sh`;
2. PSPSDK Docker build producing `EBOOT.PBP`;
3. artifact upload.
