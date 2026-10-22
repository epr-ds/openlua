# Changelog

All notable changes to OpenLua are recorded here. The project is pre-1.0 and
APIs may still change; see [docs/API.md](docs/API.md) for the current surface.

## Unreleased

### Added
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
- PSP Go storage support: savedata probes M2 (`ms0:`) and internal storage
  (`ef0:`), keeps existing saves on their volume, and loads/deletes across
  both. Boot falls back to `app0:/` when the `argv[0]` directory cannot be
  entered (#31).
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
