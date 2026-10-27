# Architecture

OpenLua is a single-threaded Lua 5.1 host for the PSP. It embeds the vendored
Lua runtime, registers engine modules as global Lua tables, and runs one
`script.lua` to completion.

```text
script.lua
    │
    ▼
engine/core/main.c ── register_api() ──────────────────────────────┐
    │                                                              │
    ├── gfx/     GE pipeline, surfaces, primitives  (screen, draw,   │
    │            splash)                                             │
    ├── img/     PNG/JPEG/BMP decode via stb_image   (image)        │
    ├── font/    PGF + SFont atlas renderer          (font, sfont)  │
    ├── snd/     WAV/VMA decode + PSP audio          (sound)        │
    ├── input/   sceCtrl polling                     (buttons)      │
    ├── util/    timers + OneLua math helpers         (timers, math) │
    ├── fs/      portable VFS, files, ini            (files, ini)   │
    ├── data/    JSON + PSMF header parsing          (json)         │
    ├── sys/     saves + battery + power + os + hw                  │
    ├── maps/    tile grid renderer                  (map)          │
    ├── 3d/      OBJ/MTL parser, camera, 3D textures  (amg, cam3d,   │
    │            and model state                      texture3d,    │
    │                                                 model3d)      │
    └── video/   PMF playback via sceMpeg            (pmf)          │
                                                                   │
                                                            Lua globals
```

## Boot sequence

`engine/core/main.c:203`

1. Register the PSP exit callback.
2. `chdir` to the directory containing the executable (`argv[0]`), falling
   back to `app0:/`.
3. Initialise the GE (`gfx_init`).
4. Create the Lua state and open the standard libraries.
5. Register engine modules (`register_api`).
6. Find and run `script.lua` (or `SCRIPT.LUA` / `boot.lua`, or an `argv` path).
7. On error, write `openlua.log` and fall back to the debug screen.

Boot tracing also appends short markers to
`flash0:/openlua_boot.txt`, with an `app0:` fallback.

## Lua binding pattern

Every module follows the same shape:

```c
static const luaL_Reg module_funcs[] = { ... };

void module_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, module_funcs);
    lua_setglobal(L, "module");
}
```

`engine/core/olreg.c` is a tiny helper that pushes each C function and sets it
as a field. Handles are 1-based integers into fixed-size global arrays
(`FONT_MAX`, `SND_MAX`, `MAP_MAX`). Userdata is only used for `image` and
`timers`, primarily to attach `__gc`.

## Rendering pipeline

`engine/gfx/gfx.c`

- `OL_Surface` is a 32-bit ABGR pixel buffer with power-of-two `realW`/`realH`
  pitch. Pixel storage is 16-byte aligned and textures are uploaded straight
  from this buffer.
- `gfx_init` allocates three 512x272x4 buffers as 8192-byte-aligned EDRAM
  offsets and configures the GE for 2D sprites. GU draw/display/depth buffer
  APIs take VRAM offsets, not absolute `0x04000000` pointers;
  `gfx_vram_fb()` adds `sceGeEdramGetAddr()` only for callers that need a real
  scanout address (PMF).
- The 2D depth range is `0..0xFFFF`, the depth buffer clears to 0 and the
  comparison is `GU_GEQUAL`, so the default `z=0` plane renders and larger
  z-indexes draw in front. Face culling is disabled because flips and rotation
  can reverse quad winding.
- `gfx_blit` is the per-sprite entry point. `transform.c` computes the four
  destination vertices and normalised UVs; `gfx_blit` converts UVs to the
  texel coordinates the GE samples, uploads the surface and draws a textured
  `GU_TRIANGLE_FAN` with nearest sampling. Scale, rotation, flips, pivot and
  tint are all applied to that single quad.
- `draw.c` submits primitives with `GU_DIRECT` and flat shading.
- `screen.flip()` calls `sound_pump()` and then `gfx_flip_scene()`.

### Fonts

`engine/font/font.c` keeps up to eight font handles and dispatches three
loaders. `font.load(path)` opens a PGF through `sceLibFont` (`pgf.c`),
`font.new(path, cellW, cellH)` describes a fixed cell grid from a PNG, and
`sfont.load` accepts a classic SFont strip with magenta top-row separators and
variable advances (or a fixed 95-cell sheet as a fallback). All three share the
same handle table, print/textwidth path and `gfx_blit` renderer.

PGF loading asks the system library for printable ASCII (`0x20`..`0x7E`)
glyph metrics and 4-bit coverage bitmaps, packs those bitmaps into a <=512 px
atlas, and expands them to white ABGR with alpha. Advances and bearings remain
26.6 fixed-point internally, so PGF text is proportionally spaced. No PGF file
is bundled: the demo uses `flash0:/font/ltn0.pgf`.

The PNG and SFont loaders decode the source image and repack its glyphs into a
texture atlas no wider than 512 px on first draw. The PSP GE cannot address
textures wider than 512 px, so the 760x16 fixed sheet and the ~782x8 classic
strip cannot be uploaded as-is. Repacking is internal: glyph counts, cell
metrics, advances, handles and Lua-visible results are unchanged. Fixed grids
use a <=512 px multi-row atlas; SFont glyphs are packed variable-width row by
row. PGF atlases use the same width limit and a single generated height.

### Frame lifecycle

Each blit starts, finishes and syncs its own GE display list. `gfx_flip_scene`
swaps buffers, waits for vblank, then clears colour and depth on the new draw
buffer. Clearing after the swap means the next frame starts from a clean
buffer instead of erasing the frame that is about to be presented. Batching all
draws into one list is still a possible optimisation.

Textures written by the CPU are made visible to the GE with
`sceKernelDcacheWritebackAll()`. The current code flushes very frequently;
targeted `sceKernelDcacheWritebackRange` is the intended optimisation.

### 3D pipeline

`engine/3d/` keeps the portable data and `engine/gfx/gfx3d.c` owns all GE
state:

- `obj.c` is an original OBJ/MTL parser. It loads positions, `vt` UVs,
  `usemtl`/`Kd`/`d`, and resolves `map_Kd` relative to the MTL file. Convex
  faces are fan-triangulated; objects come from `o`/`g` groups.
- `model3d.c` converts each object into batches grouped by material, loads
  material textures through `texture3d`, and stores per-object
  position/rotation/scale. `render` walks the batches and submits them.
- `texture3d.c` wraps an `OL_Surface` (ABGR 8888) in a 1-based handle. PNG
  loading uses the same stb_image and surface allocator as `image.*`.
- `cam3d.c` stores position, look-at target, up vector and scene rotation;
  `cam3d.set` copies the state into `gfx3d.c`.
- `amg.c` owns scene lifetime, perspective, render style, fog and the 2D/3D
  switch. `amg.begin` starts a 3D depth-tested scene, `amg.mode2d(1)` restores
  depth-disabled 2D state, and `amg.update` swaps buffers and clears the next
  frame.

`gfx3d.c` uses `sceGumPerspective`/`sceGumLookAt`/`sceGumTranslate` etc. for
matrices, then submits `GU_TRIANGLES` with `GU_TEXTURE_32BITF |
GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D`. Vertex data is repacked
into the GE's texture/colour/position order before `sceGuDrawArray`; the
public `OL_3DVertex` stays in a readable position/UV/colour order. 3D texture
coordinates are normalised (0..1), unlike the texel-space UVs used by the 2D
blitter. Depth tests use `GU_LEQUAL` with a far-plane clear; culling is off
because the importers do not guarantee winding.

### Splash screens

`engine/gfx/splash.c` reuses the 2D blitter rather than adding another render
path. `show`/`old` collect one path or a table of paths, decode PNGs into
`OL_Surface` buffers with stb_image, and present each one for about two
seconds with a short fade. The image is centred on black and uniformly
downscaled if it is larger than 480x272 (each side must stay within the 512 px
GE texture limit). Missing images are skipped; a built-in geometric fallback
is drawn when nothing loads.

Before the first frame the module calls `gfx3d_finish()` so a preceding 3D
scene is left in the documented 2D state, clears the current draw buffer with
`gfx_clear_scene()`, and saves the clear colour so it can be restored when the
last image has been shown. Frames are presented through `gfx_flip_scene()`,
which also keeps audio fed via `sound_pump()`. Input uses the same `sceCtrl`
edge detection as `buttons.read`: a fresh Cross/Circle/Start press advances
one image. The host build decodes and validates the files but does not enter
the timing loop, so the logic is covered by `test/host/test_splash.lua`.

## Audio pipeline

`engine/snd/sound.c`

1. `sound.load` reads the whole file (8 MB cap) and parses:
   - WAV: PCM 16-bit, mono/stereo, any sample rate;
   - VMA/VOX: 4-byte sample count + 22050 Hz mono PCM.
2. Stereo is downmixed; non-22050 rates are linearly resampled.
3. `sound.play` reserves a hardware channel (`sceAudioChReserve`); explicit
   channels 1..8 and automatic next-free allocation share the same path.
4. `sound_pump` runs once per `screen.flip()`. When the channel buffer drains
   below one 1024-sample chunk, it submits the next chunk. This keeps audio
   paced by the hardware even if the frame loop slows down.
5. Channels are released on stop, free, or natural end of playback.

## PMF playback

`engine/video/pmf.c` is PSP-only. `pmf.run` loads the AV codec modules, opens
the file with `sceIoOpen`, validates the 2048-byte PSMF header through the
portable `engine/data/psmf.c` parser, and streams the MPEG program stream
through a custom `SceMpegRingbuffer`. H.264 frames are decoded to a 512x272
main-memory buffer and composited into the inactive GE display buffer at the
requested position; ATRAC3+ audio is decoded and played through a reserved
audio channel. The callback can pause, resume or stop playback between frames.

The parser is host-tested, while the decoder is exercised on-device/PPSSPP.
`pmf.run` is intentionally blocking; scripts should keep `onPmfPlay` short.

## Storage and data

`engine/fs/olfs.c` is the only platform-specific filesystem layer. It provides
PSP (`sceIo*`) and POSIX backends behind the same API. Higher layers are pure
logic and are host-testable:

| Module | API | Storage |
|---|---|---|
| `files.c` | `files.*` | any mounted path |
| `ini.c` | `ini.read/write` | text INI, section-aware |
| `savedata.c` | `data.save/load/delete` | `ROOT/PSP/SAVEDATA/<GAMEID>/` |
| `json.c` | `json.decode/encode/load/save` | bounded files + atomic writes |
| `maps.c` | `map.*` | in-memory tile grid over a tileset |

On host builds `savedata` falls back to `./PSP/SAVEDATA`. On PSP it probes
M2 (`ms0:`) and PSP Go internal storage (`ef0:`), creates `PSP/SAVEDATA` when
needed, and searches both roots for loads and deletes.

## Battery telemetry

`engine/sys/battery.c` registers the OneLua `batt` table. The read-only
functions map directly to the documented `scePower` API:

| Lua | PSPSDK |
|---|---|
| `batt.exists()` | `scePowerIsBatteryExist()` |
| `batt.charging()` | `scePowerIsBatteryCharging()` |
| `batt.lifepercent()` | `scePowerGetBatteryLifePercent()` |
| `batt.lifetimemin()` | `scePowerGetBatteryLifeTime()` |
| `batt.temp()` | `scePowerGetBatteryTemp()` |
| `batt.volt()` | `scePowerGetBatteryVolt()` |
| `batt.remaincap()` | `scePowerGetBatteryRemainCapacity()` |
| `batt.fullcap()` | `scePowerGetBatteryFullCapacity()` |
| `batt.low()` | `scePowerIsLowBattery()` |

Formatting helpers (`batt_format_lifetime`, `batt_format_volt`,
`batt_format_capacity`) avoid floating-point printf and are covered by
`test/host/test_batt.c`. The host build substitutes fixed mock values so the
Lua table can be exercised by `test_batt.lua`.

Battery serial/mode queries, mode changes, and EEPROM backup/restore have no
safe public user-mode PSPSDK API. Those Lua entry points keep the OneLua
signatures but return `"-"` or `0`; they never touch flash, EEPROM or NAND.

## Power management

`engine/sys/power.c` registers the OneLua `power` table:

| Lua | PSPSDK |
|---|---|
| `power.plugged()` | `scePowerIsPowerOnline()` |
| `power.suspend()` | `scePowerRequestSuspend()` |
| `power.lock()` | `scePowerLock(0)` |
| `power.unlock()` | `scePowerUnlock(0)` |
| `power.tick()` | `scePowerTick(PSP_POWER_TICK_ALL)` |
| `power.off()` | `scePowerRequestStandby()` |

The public PSPSDK has no user-mode software shutdown. `power.off()` therefore
requests standby, the closest safe operation; a full power-off still requires
the hardware power switch. The host build provides inert stubs, and the PPSSPP
demo exercises only `plugged`, `lock`, `unlock` and `tick` so it never suspends
or powers down the emulator. PPSSPP 1.20.4 does not implement
`scePowerLock`/`scePowerUnlock`, so the demo shows those two calls as failures
with an `HLE N/A` note; `plugged` and `tick` work in the emulator.

## OS compatibility

`engine/sys/os.c` extends the stock Lua `os` table; `os.time`, `os.date` and
`os.exit` are not replaced. The OneLua additions map to public PSPSDK calls:

| Lua | PSPSDK / implementation |
|---|---|
| `os.delay(ms)` | `sceKernelDelayThread(ms * 1000)` |
| `os.getdate()` | `sceRtcGetCurrentClockLocalTime()` |
| `os.nick()` | `sceUtilityGetSystemParamString(PSP_SYSTEMPARAM_ID_STRING_NICKNAME, ...)` |
| `os.mac()` | `sceWlanGetEtherAddr()` |
| `os.language()` | `sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, ...)` |
| `os.ram()` | `sceKernelTotalFreeMemSize()` |
| `os.totalram()` | `sceKernelTotalFreeMemSize()` cached at module registration |
| `os.cpu([mhz])` | `scePowerGetCpuClockFrequency()` / `scePowerSetCpuClockFrequency()` |
| `os.bus()` | `scePowerGetBusClockFrequency()` |
| `os.infoms0()` / `os.infoef0()` | `statvfs()` on `ms0:/` and `ef0:/` |
| `os.cfw()` | `"UNK"`: no reliable public CFW detector |
| `os.versiontxt()` | `flash0:/etc/version.txt`, trimmed |
| `os.restart()` | `sceKernelLoadExec(cwd/EBOOT.PBP)`, explicit and PSP-only |

The filesystem helpers return `{ max, free, used }` in bytes. A zero-capacity
or failed `statvfs()` becomes `nil`, which is how PPSSPP and hosts without the
volume are reported. `os.password()` returns `""`; the public user-mode PSPSDK
has no parental-password query.

`os.message`, `os.messagebox`, `os.initprx`, `os.stopprx` and `os.requiere`
are deliberate stubs in this pass. They validate the OneLua argument shape and
return the documented failure/no-selection values without opening a utility
dialog, loading a PRX or changing module state.

Host builds substitute fixed system-parameter, MAC, memory and clock mocks so
the Lua surface can be exercised without touching the test machine. The
`os.lua` demo calls only read-only queries; `os.exit()`, `os.restart()`, the
dialog calls and the PRX helpers are never invoked by the demo or host tests.

## Hardware information and audio

`engine/sys/hardware.c` registers the OneLua `hw` table. The user-mode subset
uses public PSPSDK calls:

| Lua | PSPSDK / implementation |
|---|---|
| `hw.getmodel()` | PPSSPP nickname marker, `ef0:/` probe, or `"UNK"` |
| `hw.firmware()` | `sceKernelDevkitVersion()` formatted as `major.minorRevision` |
| `hw.volume([level])` | `/CONFIG/SYSTEM/SOUND` `main_volume`, clamped 1..30 |
| `hw.mute([mode])` | `/CONFIG/SYSTEM/SOUND` `mute`, clamped 0/1 |
| `hw.headphone()` | `sceHprmIsHeadphoneExist()` |
| `hw.audioout([mode])` | `sceAudioRoutingSetMode()` / `sceAudioRoutingGetMode()` |
| `hw.led(led, state)` | `-1`; `sceSysconCtrlLED` is kernel-only |

The motherboard/fuse/NAND and region values require kernel-mode IDStorage or
syscon access. Their OneLua entry points are retained with predictable
fallbacks (`"UNK"`, `"0"`, `"false"`) rather than importing a kernel driver
from the user EBOOT. Registry access is isolated in
`hw_read_sound_key`/`hw_write_sound_key` and never runs in host builds. The
audio-route setter is the only call that changes console state, and the demo
never invokes it.

PPSSPP 1.20.4 does not expose `/CONFIG/SYSTEM/SOUND`, so the PSP build keeps a
session-local `27`/`0` fallback and still attempts the registry write. The
`hw.audioout()` getter reflects PPSSPP's HLE, which always returns `0`. The
`hardware.lua` demo calls only getters; setters and LED control are marked as
not run because they change console state.

Host builds use the same formatting/clamping helpers with fixed mocks. The
pure helpers are covered by `test/host/test_hardware.c`; the Lua surface is
covered by `test/host/test_hardware.lua`.

## Memory and limits

- `PSP_HEAP_SIZE_KB(20000)` — roughly 20 MB.
- Images are capped at 1024x1024 and padded to power-of-two dimensions.
- Fonts: 8 slots, cells up to 256x256. Decoded sheets are repacked into
  atlases no wider than 512 px, the PSP GE texture limit.
- Sounds: 8 slots, 8 MB source file cap.
- Maps: 8 slots, tileset anchored via the Lua registry until `map.free`.
- INI: 512-byte parse lines, 256 KB file cap.

## Host testing

The PSP headers keep most graphics/input code out of host builds. The storage
layer, timers, OneLua math helpers, battery formatting and host mocks, power
bindings and stubs, OS formatting and host mocks, hardware formatting/clamping
and host mocks, transform math, OBJ/MTL parsing, map geometry, sound parsing
and channel bookkeeping, `engine/core/olreg.c`, and the Lua sources compile and
run on the host. `test/host/run.sh` covers OLFS paths/cwd, transforms, OBJ/MTL
parsing, map geometry, sound, battery/OS/hardware formatting, and Lua-level
files/INI/savedata/timers/math/battery/power/os/hardware/sound/3D/splash
tests.
`.github/workflows/build.yml`
runs the suite and produces an `EBOOT.PBP` artifact on every push and PR.

## Module map

| Directory | Responsibility | Public API |
|---|---|---|
| `engine/core` | boot, Lua registration | `color`, `screen.clear/flip` |
| `engine/gfx` | surfaces, GE, primitives, splash screens | `draw.*`, `splash.*` |
| `engine/img` | image decode, blits, transforms | `image.*` |
| `engine/font` | PGF via `sceLibFont`, PNG/SFont loaders, atlas packer | `font.*`, `sfont.*`, `screen.print` |
| `engine/snd` | audio decode and output | `sound.*` |
| `engine/input` | controller polling | `buttons.*` |
| `engine/util` | millisecond timers, OneLua math helpers | `timers.*`, `math.*` |
| `engine/fs` | files, INI, portable VFS | `files.*`, `ini.*` |
| `engine/data` | JSON parser/serializer, PSMF header parser | `json.*`, PMF validation |
| `engine/sys` | save slots, battery, power, OS queries, hardware info/audio | `data.*` / `savedata.*`, `batt.*`, `power.*`, `os.*`, `hw.*` |
| `engine/maps` | tilemaps | `map.*` |
| `engine/3d` | OBJ/MTL parser, camera/model/texture state, 3D scene control | `amg.*`, `cam3d.*`, `texture3d.*`, `model3d.*` |
| `engine/gfx/gfx3d.c` | GE 3D state and textured triangle submission | shared by the 3D modules |
| `engine/video` | PMF playback through `sceMpeg` | `pmf.*` |
| `third_party` | Lua 5.1.5, stb_image/stb_image_write | — |
