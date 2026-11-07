# Changelog

All notable changes to OpenLua are recorded here. The project is pre-1.0 and
APIs may still change; see [docs/API.md](docs/API.md) for the current surface.

## Unreleased

### Added
- OneLua networking modules: `wlan` (`init`, `term`, `connect`, `disconnect`,
  `isconnected`, `status`, `getip`, `strength`, `over`, `getconfigs`, `scan`,
  `autoconnect`, `autostatus`), TCP `socket` (`connect`, `server`, `accept`,
  `send`, `recv`, `close`, `term`), synchronous `http` (`getfile`, `postfile`,
  `request`, `term`), read-only passive `ftp` (`init`, `term`, `state`),
  `adhoc` (`init`, `term`, `getmac`, `scan`, `gettotal` plus documented
  matching/PDP stubs) and a `bluetooth` compatibility stub. A shared
  refcounted `engine/net` layer initialises `sceNet`/`sceNetInet` once and
  tears it down with the last user, so modules can be started and stopped
  independently. WLAN profile mutation and Sony's dialog are not exposed;
  HTTP is plaintext HTTP/1.x only (no TLS, 16 MB body cap, four redirects,
  in-memory cookies) and the OneLua background flag runs synchronously; FTP is
  anonymous/read-only and rejects path escapes and write commands; adhoc
  requires the WLAN switch and the matching/PDP calls return empty results.
  Includes 79 C checks with live loopback TCP/HTTP/FTP servers, 94 Lua
  assertions, `demos/network.lua`, a PPSSPP capture and API/architecture/demo/
  verification documentation (#45).
- OneLua peripheral modules: read-only `umd` (`init`, `present`, `size`,
  `type`, `gameid`, `info`, `term`), USB mass storage `usb` (`mstick`, `umd`,
  `flash0`-`flash3`, `stop`, `isconnected`, `isactive`), plus API-compatible
  `sio` (`init`, `baud`, `available`, `read`, `write`, `print`, `putstring`,
  `term`) and `ir` (`init`, `baud`, `state`, `get`, `put`, `read`, `write`,
  `term`). `umd.init` mounts `disc0:` and waits for READY; `umd.gameid`/`info`
  reuse the shared bounds-checked PARAM.SFO parser, and `umd.size` walks the
  mounted ISO9660 tree with a depth/entry budget and returns GB with four
  decimals. `umd.dumpiso`/`umd.launch` are intentionally not registered.
  `usb.umd`/`flash*` need Sony's optional `_usbdevice.prx` and fail safely
  without it; flash modes are always read-only, and USB storage and the camera
  are documented as mutually exclusive. `sio`/`ir` register their documented
  signatures but stay inactive in the user-mode build because raw UART/IR
  access needs a kernel driver. Every module returns documented empty values
  when the hardware is absent. Includes 12 C checks, 89 Lua assertions,
  `demos/peripherals.lua`, a PPSSPP capture and API/architecture/demo/
  verification documentation (#44).
- OneLua `cam` module for the PSP GO! CAM USB accessory: `init`, `begin`,
  `state`, `available`, `render` (native or scaled), `shot` (PNG snapshot),
  `toimage` (ordinary image handle), `direction`, brightness/contrast/
  sharpness/saturation/zoom/EV/effect controls, autoreverse, mirror and
  `term`. Video frames are read as JPEG and decoded in memory through the
  shared image path. Every call is safe when the camera is absent: init/begin
  return `0`, getters `-1`, state/available `false`, and render/shot/toimage
  do nothing or return `0`/`nil`. OneLua's 1-based effect/EV enums and 1x..5x
  zoom are mapped and clamped to the PSPSDK ranges. Includes 23 C checks, 90
  Lua assertions, `demos/cam.lua`, a PPSSPP capture with dummy camera frames,
  and API/architecture/demo/verification documentation (#43).
- OneLua `game` module (read-only subset): `game.info(path)` returns every
  valid PARAM.SFO entry in a Lua table, converting `0x0404` integers to numbers
  and `0x0004`/`0x0204` UTF-8 values to strings. `game.geticon0`,
  `game.getpic0` and `game.getpic1` extract ICON0/PIC0/PIC1 from the PBP and
  return normal `image` handles decoded in memory, ready for `image.blit`,
  `image.scale` and the usual GC path. A new bounds-checked PBP/PARAM.SFO
  parser rejects truncated headers, unsorted or out-of-range sections, bad keys
  and short values; missing or malformed files return `nil`. Section reads are
  capped (128 KB for SFO, 4 MB per image). `game.pack`, `game.add`,
  `game.remove`, `game.unpack` and `game.launch` are intentionally not
  registered, and ISO/CSO images are not parsed yet. Includes 53 C checks, 46
  Lua assertions, the generated `assets/sample.pbp` fixture
  (`tools/gen_pbp.py`), `demos/game.lua`, a PPSSPP capture and
  API/architecture/demo/verification documentation (#40).
- OneLua on-screen keyboard module: `osk.init(desc, text [, type [, textmax
  [, lines [, mode]]]])` opens the PSP system keyboard, supports normal and
  numeric modes, clamps length (`1..512`), lines (`1..8`) and language
  (`-1..10`), and returns the edited UTF-8 string, `""` for an empty field or
  `nil` on cancel/start failure. The module restores the 2D GE state, clear
  colour and both display buffers so the frame loop can resume immediately.
  Chinese modes use the firmware language because PSPSDK lacks a Chinese enum.
  Includes 54 host assertions, `demos/osk.lua`, PPSSPP open/edit captures and
  API/verification documentation; PPSSPP has no OSK cancel control, so the
  `nil` branch is host-tested and PSP-only (#42).
- OneLua splash module: `splash.show`, `splash.old` and `splash.oldschool`
  display a single PNG or a slideshow of up to 32 images for about two seconds
  each, with a short fade, centred 480x272 scaling, fresh Cross/Circle/Start
  skip input and black letterboxing. Missing, unreadable and oversized images
  are skipped; if nothing loads an original geometric fallback is drawn.
  Splash restores the 2D GE state and previous clear colour, so normal drawing
  continues afterwards. Includes 20 host assertions, the generated
  `assets/splash*.png` trio (reproducible with `tools/gen_splash.py`),
  `demos/splash.lua`, PPSSPP captures and API/demo documentation (#41).
- OneLua 3D module family: `amg` scene control, `cam3d` cameras,
  `texture3d` textures and `model3d` models on top of a new original OBJ/MTL
  parser and a PSP GE 3D renderer. `amg.begin`/`mode2d`/`update` make the
  2D↔3D state switch explicit; models are lit by material `Kd`/`d`, textured
  through `map_Kd`, batched per material and depth-tested; `amg.fog`,
  `amg.perspective` and points/lines/triangles render styles are supported.
  Includes 26 C parser checks, 33 Lua assertions, the generated
  `assets/3d/` cube/MTL/checker, a rotating `demos/3d.lua` PPSSPP capture and a
  2D `sfont` regression capture. Lighting, animation, transparent face sorting
  and real-hardware verification remain out of scope and are documented in
  `docs/API.md` (#39).
- OneLua PGF font support: `font.load(path)` opens a PSP PGF through
  `sceLibFont`, rasterizes printable ASCII (`0x20`..`0x7E`) into a cached
  <=512 px RGBA atlas, and returns a proportional font handle;
  `font.setdefault(handle_or_path)` selects it for `screen.print`. Fixed-cell
  `font.new` and the three-argument `font.load(path, cellW, cellH)` alias keep
  working, and `sfont` is unchanged. Includes 17 C checks, 20 Lua assertions,
  `demos/pgf.lua` and a PPSSPP capture of `flash0:/font/ltn0.pgf` (#38).
- OneLua `sfont` compatibility: `sfont.load`, `sfont.print`, `sfont.letters`
  and `sfont.width` for classic variable-width SFont sheets (magenta top-row
  separators, glyphs from `!`) and fixed 95-cell fallbacks. Existing fixed-cell
  `font.new` and `font.load(path, cellW, cellH)` handles, `font.print` and
  `font.textwidth` metrics are unchanged. Font sheets are repacked into atlases
  no wider than 512 px for the PSP GE; the generated fixed and classic assets
  now both load correctly. Includes `assets/sfont.png`, 20 C checks, 19 Lua
  assertions and
  the graphical `demos/sfont.lua` demo (#37).
- OneLua `hw` hardware compatibility: `getmodel`, `firmware`, `region`,
  `baryon`, `tachyon`, `pommel`, `fuseid`, `fusecfg`, `kirk`, `spock`,
  `nandseed`, `ofwinitial`, `board`, `gen`, `pandora`, `nand`, `volume`,
  `mute`, `headphone`, `audioout` and `led`. Model/firmware use public
  user-mode queries; volume/mute use the `/CONFIG/SYSTEM/SOUND` registry with
  clamped 1..30 and 0/1 inputs; headphone detection and audio routing use
  `sceHprm`/`sceAudioRouting`. Kernel-only motherboard/fuse/NAND/region/LED
  values return documented fallbacks instead of importing a kernel driver.
  PPSSPP 1.20.4 lacks the sound registry category, so it reports the
  session-local `27`/`0` fallback. Includes 54 host formatting/clamping checks,
  67 Lua assertions and a self-contained graphical demo (#36).
- OneLua `os` system compatibility: extends the stock Lua `os` table with
  `delay`, `getdate`, `nick`, `mac`, `language`, `password`, `ram`, `totalram`,
  `cpu`, `bus`, `infoms0`, `infoef0`, `cfw`, `versiontxt` and an explicit
  PSP-only `restart`. System parameters, RTC, WLAN, memory, clock and
  filesystem capacity use public PSPSDK APIs with documented host fallbacks.
  `password`/`cfw` report safe fallbacks, `restart` uses
  `sceKernelLoadExec`, and dialog/PRX calls are predictable no-side-effect
  stubs. Includes 10 host formatting checks, 61 Lua assertions and a
  self-contained graphical demo (#35).
- OneLua `power` management module: `plugged`, `lock`, `unlock`, `tick`,
  `suspend` and `off` over the public PSPSDK power API. `power.off()` requests
  standby because PSPSDK has no user-mode software shutdown; suspend/standby
  calls use inert host stubs and are omitted from the demo. Includes 14 host
  assertions and a self-contained graphical demo (#34).
- OneLua `batt` battery telemetry: read-only `exists`, `charging`,
  `lifepercent`, `lifetimemin`, `lifetime`, `temp`, `volt`, `remaincap`,
  `fullcap` and `low` bindings over `scePower`, with host-tested formatting
  and a self-contained demo. Battery serial/mode queries, mode changes and
  EEPROM backup/restore are documented safe no-ops because the public PSPSDK
  user-mode API does not expose them (#33).
- OneLua `math` compatibility helpers: `math.minmax`, `math.bitnot`,
  `math.bitand` and `math.bitor` extend the standard Lua 5.1 table with
  32-bit signed bitwise semantics. Includes 23 host assertions and a
  self-contained graphical demo (#32).
- PMF/PSMF playback through the PSP `sceMpeg` libraries: blocking
  `pmf.run(path [, x, y])`, global `onPmfPlay(playing)` callbacks,
  `pmf.play`/`pause`/`stop`, full-screen and positioned video scanout, and
  ATRAC3+ audio output. A portable PSMF header parser validates the magic,
  version, stream range, timestamps and dimensions, with 20 host checks (#5).
- JSON module with `decode`, `encode`, `load` and `save`: bounded file reads,
  atomic writes, strict malformed-input errors, escape/unicode handling and
  host coverage (#30).
- Host coverage for map geometry, savedata slot/layout validation and traversal
  rejection, plus a GitHub Actions workflow that runs the host suite and builds
  an `EBOOT.PBP` artifact (#28).
- Architecture, API, roadmap, development and contributing documentation.
- `LICENSE` (MIT).
- Runnable demos under `demos/` with shared assets under `assets/`.

### Changed
- PSP Go storage support: savedata probes M2 (`ms0:/`) and internal storage
  (`ef0:/`), creates the missing `PSP/SAVEDATA` directory, keeps existing saves
  on their volume, and loads/deletes across both. Device paths use the
  canonical `ms0:/...`/`ef0:/...` separator, script discovery has explicit
  `app0:/` fallbacks, and `files.move` falls back to recursive copy + delete
  across volumes because `sceIoRename` cannot cross filesystems. Boot falls
  back to `app0:/` when the `argv[0]` directory cannot be entered. Includes
  cross-filesystem host tests and `demos/storage.lua`; PPSSPP 1.20.4 verifies
  `app0:`/`ms0:` boot, savedata and files, but has no PSP Go model and does not
  emulate `ef0:` (#31).
- Docker builds now use a local builder image with `ccache`, run `make` with
  the host CPU count by default, and keep generated files owned by the
  invoking user. The compiler cache lives in git-ignored
  `docker/.cache/ccache`; CI restores and saves the same cache (#8).
- Documentation split into a navigable `docs/` set; stale design notes moved
  to `docs/archive/`.
- Demo and asset paths consolidated; generators now default to `assets/`.
- Project board populated with fields, views and audit-derived backlog.
- OneLua module-index audit added scoped parity issues #33–#45 (battery,
  power, OS, hardware, fonts, 3D, game, splash, OSK, camera, peripherals and
  networking) to the project board.

### Fixed
- The generated 5x7 font sheet now draws `N` with a left-to-right diagonal
  instead of a mirrored glyph, so `OPENLUA` and other generated text read
  correctly at every scale. `assets/font.png`, `assets/sfont.png` and the
  splash images were regenerated; the 3D and SFont verification captures were
  refreshed.
- Sound playback now reserves explicit channels through `sceAudioChReserve`,
  releases them on stop/free/natural end, rejects invalid channel arguments,
  rejects files shorter than the RIFF signature without over-reading, bounds
  WAV chunk lengths, and surfaces resampling/allocation failure (#25).
- Host builds now use their mocked audio-channel API instead of accidentally
  overriding it with fallback macros; PSP builds call the real pspaudio
  functions.
- Renderer now draws one transformed quad per sprite, with correct source
  stride, pivot, scale, flip, tint, per-blit alpha and no temporary-buffer leak
  (#19, #20).
- GE draw/display/depth buffers are now passed as EDRAM offsets instead of
  absolute `0x04000000` pointers. `sceGuSwapBuffers` adds the EDRAM base
  itself, so the previous values presented main-memory garbage.
  `gfx_vram_fb()` still returns an absolute address for PMF scanout.
- 2D depth setup now uses a `0..0xFFFF` range, clears depth to zero and keeps
  `GU_GEQUAL`, so the default `z=0` primitives render and larger z-indexes draw
  in front. Face culling is disabled because flips/rotation reverse winding.
- Sprite pixel buffers are 16-byte aligned, and blit UVs are converted from
  normalised coordinates to the texel space the GE samples. This fixes
  spritesheet frames and non-zero source rectangles.
- The transforms demo now passes the image handle to `image.rotate` /
  `image.scale` / `image.tint` and rotates the asymmetric triangle frame so the
  rotation is visible.
- Frame clear now happens after the buffer swap, so the presented frame is no
  longer cleared (#19).
- `image.blit` accepts a source rectangle without an explicit alpha argument.
- Timer elapsed values are non-negative and monotonic, `stoptime()` is stable,
  `timers.new(initial_ms)` and `reset(initial_ms)` apply their offset, and timer
  methods are exposed through `__index` (#21).
- INI writes now insert middle/first/last section keys under the correct header,
  keep sectionless keys before the first section, stream lines without the 2 MB
  static buffer, and use a temp file plus backup when replacing the original
  (#23).
- Portable FS cwd copies respect the destination cap, absolute PSP `chdir`
  updates the cached cwd, `mkdir_p` creates absolute chains and accepts existing
  directories, and directory listings reject names/paths that do not fit instead
  of truncating them (#22).
- Savedata now creates and probes `PSP/SAVEDATA` before writing, rejects unsafe
  game IDs and overlong subtitles, documents the highest-numbered-slot load
  semantics, and drops the unused local `mkdir_p` helper (#24, #28).
- Maps anchor their tileset through the Lua registry until `map.free`, validate
  every map cell before allocating, and no longer leak a partial map when a row
  is malformed; explicit `map.free` ownership is documented (#26).
- Image userdata now expose instance methods through `__index`, so `img:getw()`
  and other method calls work alongside `image.getw(img)`. Added `flipx`/`flipy`
  aliases, a `font.load` alias, clearer `image.load` argument forms, and
  `image.save` now reports write failures (#27).
- PMF cleanup no longer frees the `sceMpeg`-owned AVC elementary-stream
  buffer, which caused end-of-playback hangs and invalid memory accesses.
- PSP link order no longer triggers `psp-fixup-imports` stub warnings, including
  the `sceUtility` stub pulled in by libcglue (#6).
- Documentation no longer claims unimplemented APIs are complete.

## 2026-10-19

### Added
- Phase 3 modules: `files`, `ini`, `data`/`savedata`, `map`.
- Host test harness for the portable storage layer (46 assertions).
- Native WAV/VMA sound module with per-flip audio pumping.
- SFont rendering and `screen.print` / `screen.textwidth`.
- Sprite transforms: flip, scale, tint, pivot rotation, z-index.
- Project board at <https://github.com/users/epr-ds/projects/4>.

### Changed
- Standardised Lua binding registration and cross-platform headers.
- Build library order adjusted for `psp-fixup-imports`.

## Earlier

- Phase 1: Lua host, GE pipeline, image loading, draw primitives, buttons,
  timers, and the first PSP EBOOT.
