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
    ├── gfx/     GE pipeline, surfaces, primitives  (screen, draw) │
    ├── img/     PNG/JPEG/BMP decode via stb_image   (image)        │
    ├── font/    SFont tilemap renderer              (font, screen) │
    ├── snd/     WAV/VMA decode + PSP audio          (sound)        │
    ├── input/   sceCtrl polling                     (buttons)      │
    ├── util/    gettimeofday timers                 (timers)       │
    ├── fs/      portable VFS, files, ini            (files, ini)   │
    ├── data/    JSON parser/serializer              (json)         │
    ├── sys/     save slots                          (data)         │
    └── maps/    tile grid renderer                  (map)          │
                                                                   │
                                                            Lua globals
```

## Boot sequence

`engine/core/main.c:203`

1. Register the PSP exit callback.
2. `chdir` to the directory containing the executable (`argv[0]`).
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
  pitch. Textures are uploaded straight from this buffer.
- `gfx_init` allocates three 512x272x4 buffers from VRAM (`0x04000000`) and
  configures the GE for 2D sprites.
- `gfx_blit` is the per-sprite entry point. `transform.c` computes the four
  destination vertices and UVs; `gfx_blit` uploads the surface and draws a
  textured `GU_TRIANGLE_FAN` with nearest sampling. Scale, rotation, flips,
  pivot and tint are all applied to that single quad.
- `draw.c` submits primitives with `GU_DIRECT` and flat shading.
- `screen.flip()` calls `sound_pump()` and then `gfx_flip_scene()`.

### Frame lifecycle

Each blit starts, finishes and syncs its own GE display list. `gfx_flip_scene`
swaps buffers, waits for vblank, then clears colour and depth on the new draw
buffer. Clearing after the swap means the next frame starts from a clean
buffer instead of erasing the frame that is about to be presented. Batching all
draws into one list is still a possible optimisation.

Textures written by the CPU are made visible to the GE with
`sceKernelDcacheWritebackAll()`. The current code flushes very frequently;
targeted `sceKernelDcacheWritebackRange` is the intended optimisation.

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
`ms0:` and `flash0:`.

## Memory and limits

- `PSP_HEAP_SIZE_KB(20000)` — roughly 20 MB.
- Images are capped at 1024x1024 and padded to power-of-two dimensions.
- Fonts: 8 slots, cells up to 256x256.
- Sounds: 8 slots, 8 MB source file cap.
- Maps: 8 slots, tileset anchored via the Lua registry until `map.free`.
- INI: 512-byte parse lines, 256 KB file cap.

## Host testing

The PSP headers keep most graphics/input code out of host builds. The storage
layer, timers, transform math, map geometry, sound parsing/channel bookkeeping,
`engine/core/olreg.c`, and the Lua sources compile and run on the host.
`test/host/run.sh` covers OLFS paths/cwd, transforms, map geometry, sound, and
Lua-level files/INI/savedata/timers/sound tests. `.github/workflows/build.yml`
runs the suite and produces an `EBOOT.PBP` artifact on every push and PR.

## Module map

| Directory | Responsibility | Public API |
|---|---|---|
| `engine/core` | boot, Lua registration | `color`, `screen.clear/flip` |
| `engine/gfx` | surfaces, GE, primitives | `draw.*` |
| `engine/img` | image decode, blits, transforms | `image.*` |
| `engine/font` | SFont loader/renderer | `font.*`, `screen.print` |
| `engine/snd` | audio decode and output | `sound.*` |
| `engine/input` | controller polling | `buttons.*` |
| `engine/util` | millisecond timers | `timers.*` |
| `engine/fs` | files, INI, portable VFS | `files.*`, `ini.*` |
| `engine/data` | JSON parser/serializer | `json.*` |
| `engine/sys` | save slots | `data.*` / `savedata.*` |
| `engine/maps` | tilemaps | `map.*` |
| `third_party` | Lua 5.1.5, stb_image/stb_image_write | — |
