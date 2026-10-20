# Build Instructions

The full development guide is [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md).
This page is the short version.

## Prerequisites

- Docker
- The `pspdev/pspdev:latest` image (pulled automatically by the script)

Local PSPSDK builds are also supported: install `pspdev`, then run `make`
with `psp-config` on `PATH`.

## Build

```bash
bash docker/build.sh
```

Clean rebuild — run the two commands separately:

```bash
bash docker/build.sh clean
bash docker/build.sh
```

Do not combine `clean` and the build in one invocation; the PSPSDK makefile can
run them in parallel and the link may race.

## Output

- `EBOOT.PBP` — deploy to `PSP/GAME/OpenLua/`
- `openlua.elf` — intermediate ELF

## Host tests

```bash
bash test/host/run.sh
```

Expected result: `46 passed, 0 failed`.

## Layout

| Directory | Contents |
|---|---|
| `engine/` | Engine modules (one directory per area) |
| `third_party/lua` | Lua 5.1.5 |
| `third_party/stb` | stb_image / stb_image_write |
| `assets/` | Demo and test assets |
| `demos/` | Runnable Lua scripts |
| `docs/` | Architecture, API, roadmap, development guide |
| `test/` | Host and on-device tests |
| `tools/` | Asset generators and local PPSSPP |

## Adding source files

Add the new object to `OBJS` in `Makefile`, then register its Lua API from
`engine/core/main.c`. Keep PSP-specific includes behind `#if defined(__PSP__)`
when the code should build on the host.
