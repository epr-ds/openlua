# OpenLua — PSP Lua Engine

A clean-room, OneLua-API-compatible Lua 5.1 engine for the PlayStation
Portable, built with PSPSDK.

[![Build](https://github.com/epr-ds/openlua/actions/workflows/build.yml/badge.svg)](https://github.com/epr-ds/openlua/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Project board](https://img.shields.io/badge/project-board-orange)](https://github.com/users/epr-ds/projects/4)
[![Lua 5.1](https://img.shields.io/badge/Lua-5.1-2c2d72)](third_party/lua)

## Overview

OpenLua runs a single `script.lua` on the PSP and exposes game-oriented Lua
modules on top of the hardware:

- Lua 5.1 interpreter (vendored, unmodified).
- GE sprite renderer: PNG loading, frames, flips, scale, rotation, tint.
- SFont bitmap text with `screen.print`.
- Native WAV/VMA audio, pumped from `screen.flip()`.
- Portable storage: files, INI, JSON, save slots, M2/PSP Go volumes and tilemaps.
- Read-only battery telemetry through the OneLua `batt` table.
- Power management through the OneLua `power` table.
- OS system queries and safe utilities through an extended Lua `os` table.
- Hardware identity, sound-registry volume/mute, headphone detection and audio
  routing through the OneLua `hw` table.
- Builds with PSPSDK in Docker; storage and platform modules are host-testable.

## Quick start

```bash
git clone https://github.com/epr-ds/openlua.git
cd openlua
bash docker/build.sh
```

Output: `EBOOT.PBP`. Copy it, `script.lua` and any assets into
`PSP/GAME/OpenLua/`, then launch from the XMB or PPSSPP.

In PPSSPP, disable software rendering and enable fast memory if needed.

Host tests (no PSP toolchain required):

```bash
bash test/host/run.sh              # 301 C checks + 377 Lua assertions
bash tools/check_repo_hygiene.sh   # no tracked build artifacts
```

## Example

```lua
local red = color.new(255, 0, 0)
local img = image.load("assets/test_sheet.png", 64, 64)
local f   = font.new("assets/font.png", 8, 16)

screen.font(f)

while true do
    screen.clear(color.new(16, 24, 48))

    image.reset(img)
    image.blitsprite(img, 60, 80, 0)

    image.fliph(img, true)
    image.blitsprite(img, 140, 80, 1)
    image.fliph(img, false)

    draw.fillrect(20, 20, 120, 4, red)
    screen.print(20, 40, "Hello PSP")
    screen.flip()
end
```

Image instance functions work in both forms: `image.getw(img)` and `img:getw()`.
See [docs/API.md](docs/API.md) for every function and its current caveats.

## Status

| Area | State |
|---|---|
| Lua host, `color`, `screen`, `draw` | Implemented |
| Images and sprite frames | Implemented |
| Grid fonts / `screen.print` | Implemented |
| OneLua `sfont` | Implemented; host-tested ([#37]) |
| Sound (WAV/VMA) | Implemented; host-tested ([#25]) |
| Buttons | Implemented (`waitforkey` pending) |
| Timers | Implemented; elapsed-time fix [#21] |
| OneLua math helpers | Implemented; host-tested and PPSSPP-verified ([#32]) |
| Battery telemetry | Implemented; host-tested and PPSSPP-verified ([#33]) |
| Power management | Implemented; host-tested; `plugged`/`tick` PPSSPP-verified, lock/unlock HLE missing ([#34]) |
| OS system queries | Implemented; host-tested and PPSSPP-verified; password/CFW/dialog/PRX limits documented ([#35]) |
| Hardware info/audio | Implemented; host-tested and PPSSPP-verified; kernel-only ID/LED fields return documented fallbacks ([#36]) |
| Files / INI / Savedata | Implemented; hardening [#24] |
| JSON | Implemented; host-tested ([#30]) |
| Tilemaps | Implemented; host-tested ([#26]) |
| Renderer correctness | Fixed and PPSSPP-verified; PSP hardware pending ([#19], [#20]) |
| PSP Go | Storage/boot paths implemented; device verification [#31] |
| PMF/PSMF video + ATRAC3+ audio | Implemented; PPSSPP-verified ([#5]) |
| WiFi/HTTP | Not started |

The [project board](https://github.com/users/epr-ds/projects/4) is the source
of truth for remaining work.

## Project layout

```text
engine/
  core/   boot, Lua registration, stb implementation
  gfx/    GE surfaces, blits, draw primitives
  img/    image loader and Lua bindings
  font/   grid and SFont loaders, shared renderer
  snd/    WAV/VMA decoder and PSP audio
  input/  controller polling
  util/   timers, OneLua math helpers
  fs/     portable VFS, files, INI
  data/   JSON parser/serializer, PSMF header parser
  sys/    save slots, battery, power, OS queries, hardware info/audio
  maps/   tilemap renderer
  video/  PMF playback through sceMpeg
third_party/
  lua/    Lua 5.1.5
  stb/    stb_image, stb_image_write
assets/   shared demo/test assets
demos/    runnable Lua demos
docs/     architecture, API, roadmap, development guide
test/     host and on-device tests
tools/    asset generators and local PPSSPP
docker/   PSPSDK build script
```

## Documentation

| Document | Purpose |
|---|---|
| [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) | Build, test, debug, add a module |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Boot flow and internals |
| [docs/API.md](docs/API.md) | Complete Lua API reference |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Phases, issues and priorities |
| [demos/README.md](demos/README.md) | Runnable examples |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Workflow and definition of done |
| [CHANGELOG.md](CHANGELOG.md) | Recent changes |
| [docs/onelua-reference](docs/onelua-reference) | OneLua compatibility reference |

## Build

```bash
bash docker/build.sh

# Clean rebuild — run the two steps separately
bash docker/build.sh clean
bash docker/build.sh
```

Requirements: Docker and the `pspdev/pspdev:latest` image. The first run builds
a local `openlua-pspdev:local` builder image with `ccache`; later builds reuse
compiled objects from `docker/.cache/ccache` (git-ignored). Set `JOBS=8` to
override the parallel job count. See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md)
for a local toolchain build, host tests and debugging.

## OneLua compatibility

| OneLua module | OpenLua | Notes |
|---|---|---|
| `screen` | `clear`, `flip`, `print`, `font`, `setcolor`, `textwidth` | Implemented |
| `image` | `load`, `new`, `blit`, `blitsprite`, `fliph`, `flipv`, transforms | Implemented; method + module-call forms; [#19]/[#20] device pending |
| `draw` | `line`, `rect`, `fillrect`, `circle` | `gradrect` pending |
| `buttons` | `read` + button constants | `waitforkey`/`interval` pending |
| `timers` | `timers.new([initial_ms])` + methods | Implemented; host-tested ([#21]) |
| `math` | `minmax`, `bitnot`, `bitand`, `bitor` | Extends Lua 5.1 `math`; 32-bit signed bitwise results; host-tested ([#32]) |
| `batt` | `exists`, `charging`, `lifepercent`, `lifetime`, `temp`, `volt`, `remaincap`, `fullcap`, `low` | Read-only telemetry; serial/mode/EEPROM operations are safe no-ops ([#33]) |
| `power` | `plugged`, `suspend`, `lock`, `unlock`, `tick`, `off` | Implemented; `off` requests standby because PSPSDK lacks software shutdown; PPSSPP 1.20.4 lacks lock/unlock HLE; host-tested ([#34]) |
| `os` | `delay`, `getdate`, `nick`, `mac`, `language`, `password`, `ram`, `totalram`, `cpu`, `bus`, `infoms0`, `infoef0`, `cfw`, `versiontxt`, `restart`, dialogs/PRX | Extends stock Lua `os`; host-tested and PPSSPP-verified; password/CFW and dialog/PRX limits documented ([#35]) |
| `hw` | `getmodel`, `firmware`, `region`, `baryon`, `tachyon`, `pommel`, `fuseid`, `fusecfg`, `kirk`, `spock`, `nandseed`, `ofwinitial`, `board`, `gen`, `pandora`, `nand`, `volume`, `mute`, `headphone`, `audioout`, `led` | Host-tested and PPSSPP-verified; ID/fuse/NAND/LED values need kernel mode and return documented fallbacks; volume/mute use the sound registry with a session fallback; audio routing uses `sceAudioRouting` ([#36]) |
| `font` / `sfont` | `font.new`/`font.load`, `print`, `textwidth`; `sfont.load`, `print`, `letters`, `width` | Grid PNG sheets and proportional SFont strips share handles and `screen.print`; host-tested ([#37]); PGF loading tracked in [#38] |
| `sound` | `load`, `play`, `pause`, `stop`, `loop`, `vol` | WAV/VMA; host-tested ([#25]) |
| `files` | `exists`, `copy`, `move`, `delete`, `list`, ... | Archives pending |
| `ini` | `read`, `write` | Implemented; host-tested |
| `savedata` | `data.save/load/delete` | Plain-text slots |
| `maps` | `map.new/blit/set/get` | Host-tested ([#26]) |
| `json` | `decode`, `encode`, `load`, `save` | Implemented; host-tested ([#30]) |
| `http` / `wlan` | — | Not started |

## License

MIT — see [LICENSE](LICENSE). OpenLua is clean-room code; no OneLua source was
used or modified.

[#5]: https://github.com/epr-ds/openlua/issues/5
[#19]: https://github.com/epr-ds/openlua/issues/19
[#20]: https://github.com/epr-ds/openlua/issues/20
[#21]: https://github.com/epr-ds/openlua/issues/21
[#22]: https://github.com/epr-ds/openlua/issues/22
[#23]: https://github.com/epr-ds/openlua/issues/23
[#24]: https://github.com/epr-ds/openlua/issues/24
[#25]: https://github.com/epr-ds/openlua/issues/25
[#26]: https://github.com/epr-ds/openlua/issues/26
[#27]: https://github.com/epr-ds/openlua/issues/27
[#30]: https://github.com/epr-ds/openlua/issues/30
[#31]: https://github.com/epr-ds/openlua/issues/31
[#32]: https://github.com/epr-ds/openlua/issues/32
[#33]: https://github.com/epr-ds/openlua/issues/33
[#34]: https://github.com/epr-ds/openlua/issues/34
[#35]: https://github.com/epr-ds/openlua/issues/35
[#36]: https://github.com/epr-ds/openlua/issues/36
[#37]: https://github.com/epr-ds/openlua/issues/37
[#38]: https://github.com/epr-ds/openlua/issues/38
