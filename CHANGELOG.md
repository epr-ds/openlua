# Changelog

All notable changes to OpenLua are recorded here. The project is pre-1.0 and
APIs may still change; see [docs/API.md](docs/API.md) for the current surface.

## Unreleased

### Added
- JSON module planned (#30).
- PSP Go compatibility plan (#31).
- Architecture, API, roadmap, development and contributing documentation.
- `LICENSE` (MIT).
- Runnable demos under `demos/` with shared assets under `assets/`.

### Changed
- Documentation split into a navigable `docs/` set; stale design notes moved
  to `docs/archive/`.
- Demo and asset paths consolidated; generators now default to `assets/`.
- Project board populated with fields, views and audit-derived backlog.

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
- PSP link order no longer triggers `psp-fixup-imports` stub warnings (#6).
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
