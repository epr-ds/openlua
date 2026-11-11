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
- OneLua PGF system fonts with `font.load`/`font.setdefault`, plus fixed-cell
  PNG and classic variable-width SFont text with `font`/`sfont` and
  `screen.print`.
- OneLua 3D subset: `amg` scene control, `cam3d` cameras, `texture3d` textures
  and `model3d` OBJ/MTL models rendered through the GE.
- Start-up splash screens with `splash.show`, `splash.old` and
  `splash.oldschool`, including slideshows and a built-in missing-asset
  fallback.
- On-screen keyboard input through `osk.init`, with normal/numeric modes,
  length/line limits and the OneLua language list.
- Read-only PBP inspection through `game.info` and
  `game.geticon0`/`getpic0`/`getpic1`, with bounds-checked PARAM.SFO parsing
  and artwork decoded as normal image handles.
- USB camera support through `cam.init`/`begin`/`render`/`toimage`, with
  snapshot capture and the OneLua image controls. Every call is safe when the
  GO! CAM accessory is absent.
- Read-only UMD probing/mounting through `umd.init`/`present`/`type`/`gameid`/
  `info`/`size`, and USB mass storage through `usb.mstick`/`umd`/`flash*`/
  `stop`/`isconnected`/`isactive`. Both report absent hardware safely.
- `sio` and `ir` are registered with their documented signatures; the
  user-mode build reports them as unavailable because raw serial/IR access
  needs a kernel driver.
- Native WAV/VMA audio, pumped from `screen.flip()`.
- Portable storage: files, INI, JSON, save slots, M2/PSP Go volumes and tilemaps.
- Read-only battery telemetry through the OneLua `batt` table.
- Power management through the OneLua `power` table.
- OS system queries and safe utilities through an extended Lua `os` table.
- Hardware identity, sound-registry volume/mute, headphone detection and audio
  routing through the OneLua `hw` table.
- An on-device hardware test menu (`demos/tester.lua`) that covers every
  module, saves a declared PSP model and writes a PASS/FAIL/INFO/SKIP report;
  stage it with `bash tools/package_tester.sh` (see [docs/TESTER.md](docs/TESTER.md)).
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

Prebuilt, versioned releases (EBOOT.PBP plus the full tester package) are
published automatically by GitHub Actions on every push to `main`; see
[docs/RELEASING.md](docs/RELEASING.md) or the repository's Releases page.

Host tests (no PSP toolchain required):

```bash
bash test/host/run.sh              # 506 C checks + 815 Lua assertions
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
| PGF / SFont / `sfont` / `screen.print` | Implemented; host-tested and PPSSPP-verified ([#37], [#38]) |
| 3D modules (`amg`, `cam3d`, `texture3d`, `model3d`) | Implemented; host-tested and PPSSPP-verified; lighting, animation and transparent passes documented as unsupported ([#39]) |
| Splash screens (`splash`) | Implemented; host-tested and PPSSPP-verified ([#41]) |
| On-screen keyboard (`osk`) | Implemented; host-tested; edit/confirm PPSSPP-verified, cancel is PSP-only ([#42]) |
| PBP metadata/artwork (`game`) | Implemented; host-tested and PPSSPP-verified; read-only subset, ISO/CSO unsupported ([#40]) |
| USB camera (`cam`) | Implemented; host-tested; active path PPSSPP-verified with dummy frames; real GO! CAM hardware pending ([#43]) |
| UMD + USB storage (`umd`, `usb`) | Implemented; host-tested; UMD present/type and USB flags PPSSPP-verified; game-disc metadata and USB flash modes need real hardware/CFW ([#44]) |
| Serial + IR (`sio`, `ir`) | API registered; documented as unavailable in the user-mode build; raw access needs a kernel driver ([#44]) |
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
| PSP Go | `app0:`/`ms0:` boot, savedata and file paths verified in PPSSPP (no PSP Go model or `ef0:` emulation); `ef0:` internal-storage and M2 cross-volume check needs hardware ([#31]) |
| PMF/PSMF video + ATRAC3+ audio | Implemented; PPSSPP-verified ([#5]) |
| Networking (`wlan`, `socket`, `http`, `ftp`, `adhoc`, `bluetooth`) | Documented subsets implemented; host-tested with in-process loopback servers and PPSSPP-verified for safe absence; real WiFi/adhoc hardware and TLS/background transfers are not available ([#45]) |
| Hardware test menu (`demos/tester.lua`) | Implemented; all modules covered; PPSSPP batch reports 230 checks with 0 fail; real PSP-1000/2000/3000/E1000/Go runs pending ([docs/TESTER.md](docs/TESTER.md)) |

The [project board](https://github.com/users/epr-ds/projects/4) is the source
of truth for remaining work.

## Project layout

```text
engine/
  core/   boot, Lua registration, stb implementation
  gfx/    GE surfaces, blits, draw primitives, splash screens
  img/    image loader and Lua bindings
  font/   PGF, fixed-cell and SFont loader/renderer
  snd/    WAV/VMA decoder and PSP audio
  input/  controller polling
  util/   timers, OneLua math helpers
  fs/     portable VFS, files, INI
  data/   JSON parser/serializer, PSMF header parser
  net/    shared network stack init/refcount, TCP/DNS helpers
  sys/    save slots, battery, power, OS queries, hardware info/audio, cam,
          umd/usb/sio/ir, wlan/socket/http/ftp/adhoc/bluetooth
  maps/   tilemap renderer
  3d/     OBJ/MTL parser, camera/model/texture state, 3D scene control
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
| [docs/TESTER.md](docs/TESTER.md) | On-device hardware test menu |
| [docs/RELEASING.md](docs/RELEASING.md) | Versioning, CI/CD and release assets |
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
| `font` / `sfont` | `font.load`/`setdefault`, `font.new`/`print`/`textwidth`; `sfont.load`, `print`, `width`, `letters` | PGF system fonts (host/opt-in builds only; the standard PSP EBOOT omits `sceLibFont` so real firmware does not fail with `8002013C`); fixed-cell PNG and classic variable-width sheets still supported; sheets are repacked to <=512px GE atlases ([#37], [#38]) |
| `amg` / `cam3d` / `texture3d` / `model3d` | Scene control, camera, textures, OBJ/MTL models | Static textured models with depth testing; lighting, animation and transparent passes are not implemented ([#39]) |
| `splash` | `show`, `old`, `oldschool` | Single images or slideshows, timing and skip input, built-in missing-asset fallback ([#41]) |
| `osk` | `init` | Normal/numeric keyboard, length/line/language options, blocking edit/confirm and `nil` cancel; restores GE/display state ([#42]) |
| `game` | `info`, `geticon0`, `getpic0`, `getpic1` | Read-only PBP/PARAM.SFO metadata and artwork as normal image handles; missing/malformed input returns `nil`; pack/add/remove/unpack/launch and ISO/CSO are not exposed ([#40]) |
| `cam` | `init`, `begin`, `state`, `available`, `render`, `shot`, `toimage`, `direction`, image controls, `term` | GO! CAM USB camera; absent hardware returns `false`/`0`/`nil`/`-1`; PPSSPP dummy-frame path verified, real camera hardware pending ([#43]) |
| `umd` | `init`, `present`, `size`, `type`, `gameid`, `info`, `term` | Read-only mount/probing; `dumpiso` and `launch` are not exposed; `size` is a bounded estimate of the mounted tree; game metadata needs a game disc ([#44]) |
| `usb` | `mstick`, `umd`, `flash0`-`flash3`, `stop`, `isconnected`, `isactive` | USB mass storage; only the standard memory-stick mode is built in, because a static `_usbdevice.prx` import makes the EBOOT fail with `8002013C`; shares the USB port with `cam` ([#44]) |
| `sio` | `init`, `baud`, `available`, `read`, `write`, `print`, `putstring`, `term` | Registered for compatibility; user-mode PSPSDK has no safe SIO syscall, so `init` is `0`, `read` is `-1` and sending is a no-op ([#44]) |
| `ir` | `init`, `baud`, `state`, `get`, `put`, `read`, `write`, `term` | Registered for compatibility; raw IR needs a kernel SIRC/IrDA driver, so `init` is `0`, `get` is `-1`, `read` is `nil` and `write` is `-1` ([#44]) |
| `sound` | `load`, `play`, `pause`, `stop`, `loop`, `vol` | WAV/VMA; host-tested ([#25]) |
| `files` | `exists`, `copy`, `move`, `delete`, `list`, ... | Archives pending |
| `ini` | `read`, `write` | Implemented; host-tested |
| `savedata` | `data.save/load/delete` | Plain-text slots |
| `maps` | `map.new/blit/set/get` | Host-tested ([#26]) |
| `json` | `decode`, `encode`, `load`, `save` | Implemented; host-tested ([#30]) |
| `wlan` | `init`, `term`, `connect`, `disconnect`, `isconnected`, `status`, `getip`, `strength`, `over`, `getconfigs`, `scan`, `autoconnect`, `autostatus` | Read-only connection state plus explicit connect/disconnect; Sony's profile dialog and add/copy/delete/backup/restore calls are not exposed; `getconfigs`/`scan` return empty tables and `autoconnect` is a validated no-op ([#45]) |
| `socket` | `connect`, `server`, `accept`, `send`, `recv`, `close`, `term` | Blocking TCP client/server userdata with `__gc`; no UDP or TLS; at most 64 live handles and 1 MB per `recv` ([#45]) |
| `http` | `getfile`, `postfile`, `request`, `term` | Synchronous HTTP/1.x GET/POST only; no TLS; 15 s timeout, 4 redirects, 16 MB body cap, in-memory cookies; the OneLua background flag is accepted but runs synchronously ([#45]) |
| `ftp` | `init`, `term`, `state` | Anonymous read-only passive FTP server rooted at the working directory; writes/renames/deletes are rejected; default port 21, override with `OPENLUA_FTP_PORT` ([#45]) |
| `adhoc` | `init`, `term`, `getmac`, `scan`, `gettotal`; matching/PDP stubs | Lifecycle and peer-list subset; init needs the WLAN switch on; matching/PDP calls return their documented empty results ([#45]) |
| `bluetooth` | `init`, `term`, `state`, `available` | Compatibility stub; always `false` because the PSP has no built-in Bluetooth radio and PSPSDK exposes no stack ([#45]) |

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
[#39]: https://github.com/epr-ds/openlua/issues/39
[#40]: https://github.com/epr-ds/openlua/issues/40
[#41]: https://github.com/epr-ds/openlua/issues/41
[#42]: https://github.com/epr-ds/openlua/issues/42
[#43]: https://github.com/epr-ds/openlua/issues/43
[#44]: https://github.com/epr-ds/openlua/issues/44
[#45]: https://github.com/epr-ds/openlua/issues/45
