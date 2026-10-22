# Lua API Reference

This is the API **as implemented**, not the OneLua target. Signatures are
shown in the form they are actually parsed by the C bindings. If something
here disagrees with a demo, the demo is a bug.

- Lua 5.1, one global table per module.
- Screen is 480x272.
- Colours are 32-bit **ABGR** integers: `0xAABBGGRR` (alpha high byte, red low
  byte). `color.new` builds them for you.
- Image/font/sound/map handles are 1-based integers. The image and timer
  values are userdata with a `__gc` metamethod.
- Functions that fail usually raise a Lua error (`luaL_error`) rather than
  returning nil. Exceptions are noted below.

## colour

```lua
local c = color.new(r, g, b [, a])   -- a defaults to 255
```

Returns an ABGR integer.

## screen

| Function | Description |
|---|---|
| `screen.clear(color)` | Set the colour used to clear the next frame. It does not draw immediately. |
| `screen.flip()` | Present the frame and pump audio. |
| `screen.font([handle])` | Set/get the default font handle. |
| `screen.setcolor([abgr])` | Set/get the default text tint. |
| `screen.print(x, y, text [, handle [, tint]])` | Draw text, returns the final cursor X. Requires a default or explicit font. |
| `screen.textwidth(text [, handle])` | Measure text in pixels. |

```lua
local f = font.new("assets/font.png", 8, 16)
screen.font(f)
screen.print(20, 20, "Hello PSP")
```

## image

`image` values are surfaces. Instance functions accept either the module-call
form (`image.getw(img)`) or the method form (`img:getw()`). Both are equivalent;
the module-call form is used in older examples.

### Loading and creating

| Function | Description |
|---|---|
| `image.load(path [, mask])` | Decode PNG/JPEG/BMP/GIF with an optional transparency mask colour. Raises on failure. |
| `image.load(path, frameW, frameH [, mask])` | Decode and split the image into a `frameW` x `frameH` sprite grid, with an optional mask. Raises on failure. |
| `image.loadv(path [, ...])` | Alias for `image.load`. |
| `image.new(w, h [, color])` | Create a blank surface, optionally filled. |
| `image.copy(img)` | Clone a surface. |
| `image.save(img, path)` | Write a PNG; returns `true` or raises on failure. |

### Drawing

| Function | Description |
|---|---|
| `image.blit(img, x, y [, alpha])` | Draw the whole surface (or first frame). |
| `image.blit(img, x, y, sx, sy, w, h [, alpha])` | Draw a source rectangle. |
| `image.blitsprite(img, x, y, frame [, alpha])` | Draw one frame of a loaded sheet. |
| `image.clear(img, color)` | Fill a surface with a colour. |
| `image.pixel(img, x, y)` | Read a pixel (ABGR). |
| `image.pixel(img, x, y, color)` | Write a pixel. |

`frame` is 0-based. Sheet columns are `img.w / frameW`; rows follow in
left-to-right, top-to-bottom order.

### Transform state

Transforms are stored on the surface and apply to later blits. Scale, rotation,
flip, pivot and tint compose in one quad. Reset them between sprites.

| Function | Description |
|---|---|
| `image.fliph(img [, set])` | Set horizontal flip; omit `set` to toggle. `flipx` is an alias. |
| `image.flipv(img [, set])` | Set vertical flip; omit `set` to toggle. `flipy` is an alias. |
| `image.rotate(img, degrees)` | Rotation in degrees. |
| `image.scale(img [, uniform])` | Set uniform scale. |
| `image.scale(img, sx, sy)` | Set per-axis scale. |
| `image.pivot(img [, x, y])` | Set rotation pivot; omit coordinates to reset to the current frame centre. |
| `image.zindex(img [, z])` | Set/get the depth value passed to the GE. |
| `image.tint(img [, abgr])` | Set/get colour modulation. `0xFFFFFFFF` means no tint; the tint alpha multiplies the blit alpha. |
| `image.center(img [, ox, oy])` | Set draw offset; with no coordinates, offset by negative half the surface size. |
| `image.reset(img)` | Reset flips, angle and centre offset. It does **not** reset scale, tint, pivot or z-index. |

### Size

`image.getw(img)`, `image.geth(img)`, `image.getrealw(img)`,
`image.getrealh(img)` return the logical or power-of-two padded dimensions.

> Scale, rotation and flip composition is covered by `test/host/test_transform.c`;
> device verification is tracked in #19 and #20.

## draw

| Function | Description |
|---|---|
| `draw.line(x1, y1, x2, y2, color)` | Line. |
| `draw.rect(x, y, w, h, color)` | Outlined rectangle. |
| `draw.fillrect(x, y, w, h, color)` | Filled rectangle. |
| `draw.circle(x, y, radius, color [, sections])` | Outlined circle; `sections` defaults to 30. |

`draw.gradrect` is not implemented.

## font / sfont

| Function | Description |
|---|---|
| `font.new(path, cellW, cellH)` | Load an SFont PNG sheet. Returns a handle or nil. `font.load` is an alias. |
| `font.nchars(handle)` | Number of glyphs in the sheet. |
| `font.cell(handle)` | Returns `cellW, cellH`. |
| `font.draw(handle, x, y, code [, tint])` | Draw one ASCII code. |
| `font.print(handle, x, y, text [, tint])` | Draw a string; returns final cursor X. |
| `font.textwidth(handle, text)` or `font.textwidth(text, handle)` | Measure a string. |
| `font.free(handle)` | Release the handle and its surface. |

Glyphs start at space (`0x20`) and advance in ASCII order. Newlines move to the
next line; tabs advance four cells.

## buttons

```lua
local mask, lx, ly = buttons.read()
```

`mask` is a `sceCtrl` bitmask; `lx`/`ly` are the analog stick values.

Global constants: `__SELECT`, `__START`, `__UP`, `__DOWN`, `__LEFT`,
`__RIGHT`, `__L`, `__R`, `__TRIANGLE`, `__CIRCLE`, `__CROSS`, `__SQUARE`,
`__NOTE`, `__HOLD`.

`buttons.waitforkey` / `buttons.interval` are not implemented.

## timers

```lua
local t = timers.new(1000)  -- start with a 1-second offset
t:start()
local elapsed = t:time()    -- milliseconds
local total   = t:stoptime()
t:reset(500)                -- restart at 500 ms
```

Methods: `start`, `time`, `gettime`, `stoptime`, `reset`, `startreset`.
`time`/`gettime` return elapsed milliseconds; `stoptime` freezes the value
and returns the same result on repeated calls. `start` and `reset` restart the
timer and return its new value. There is no `timers.start(t)` function; use the
method form.

## math

OpenLua extends Lua 5.1's standard `math` table rather than replacing it, so
`math.floor`, `math.sqrt`, `math.pi` and the rest of the standard library keep
working.

| Function | Description |
|---|---|
| `math.minmax(x, min, max)` | Clamp `x` into the inclusive range `[min, max]`. |
| `math.bitnot(bit)` | Bitwise NOT of a 32-bit integer. |
| `math.bitand(a, b)` | Bitwise AND of two 32-bit integers. |
| `math.bitor(a, b)` | Bitwise OR of two 32-bit integers. |

Bitwise operands must be integral values in `[-2^31, 2^32-1]`; fractional or
out-of-range operands raise an error. Results are signed 32-bit integers, so the
high bit is represented as a negative Lua number.

```lua
math.minmax(45, 0, 30)     --> 30
math.minmax(-5, 0, 10)     --> 0
math.bitand(0xAA, 0xCC)    --> 136
math.bitor(0xAA, 0xCC)     --> 238
math.bitnot(0x0F)          --> -16
math.bitnot(0)             --> -1
```

The `math.lua` demo visualises clamping and the low eight bits of the bitwise
results; it does not require any external assets.

## battery

The `batt` table exposes read-only battery telemetry. The read-only calls are
safe to make every frame; they do not change clock, charging or power state.

| Function | Returns |
|---|---|
| `batt.exists()` | Boolean: a battery is present. |
| `batt.charging()` | Boolean: the battery is charging. |
| `batt.lifepercent()` | Number: remaining charge percentage. Negative when unavailable. |
| `batt.lifetimemin()` | Number: remaining time in minutes. Negative when unavailable. |
| `batt.lifetime()` | String: remaining time as `H:MM`, or `"-"` when unavailable. |
| `batt.temp()` | Number: battery temperature in degrees Celsius. Negative when unavailable. |
| `batt.volt()` | String: voltage in volts with two decimals (for example `"4.02"`), or `"-"`. |
| `batt.remaincap()` | String: remaining capacity in mAh as a decimal string, or `"-"`. |
| `batt.fullcap()` | String: full capacity in mAh as a decimal string, or `"-"`. |
| `batt.low()` | Boolean: the firmware reports a low battery. |
| `batt.serial()` | `"-"`; no safe user-mode PSPSDK API is available. |
| `batt.mode()` | `"-"`; no safe user-mode PSPSDK API is available. |
| `batt.tonormal()` | `0`; mode changes are not implemented. |
| `batt.topandora()` | `0`; mode changes are not implemented. |
| `batt.toautoboot()` | `0`; mode changes are not implemented. |
| `batt.backupbat(path)` | `0`; EEPROM backup is not implemented. |
| `batt.restorebat(path)` | `0`; EEPROM restore is not implemented. |

The last five calls accept the OneLua signatures and always report failure.
They never write to flash, EEPROM or NAND. The public PSPSDK user-mode API has
no battery serial/mode or EEPROM access, so returning a safe failure is
preferable to issuing undocumented syscon commands. `backupbat` and
`restorebat` require a string path for signature compatibility, but the path
is never opened.

```lua
if batt.exists() then
    local percent = batt.lifepercent()
    local time    = batt.lifetime()
    local volts   = batt.volt()
    local temp    = batt.temp()
end
```

Host tests use mocked battery values; the `battery.lua` demo shows the values
reported by the current PPSSPP build.

## power

The `power` table exposes the OneLua power-management calls. `plugged`, `lock`,
`unlock` and `tick` are safe to use in a frame loop. `suspend` and `off` change
the console's power state.

| Function | Returns | Behaviour |
|---|---|---|
| `power.plugged()` | Boolean: AC adapter or charger is connected. | `scePowerIsPowerOnline()` |
| `power.suspend()` | `nil` | Requests suspend. May not return until the console resumes. |
| `power.lock()` | Boolean: the power switch was locked. | `scePowerLock(0)` |
| `power.unlock()` | Boolean: the power switch was unlocked. | `scePowerUnlock(0)` |
| `power.tick()` | `nil` | Resets the idle timer with `scePowerTick(PSP_POWER_TICK_ALL)`. |
| `power.off()` | `nil` | Requests standby; see the shutdown limitation below. |

`power.tick()` is not called automatically by `screen.flip()`. Long-running
scripts that must stay awake should call it periodically.

The public PSPSDK has no user-mode software power-off function. `power.off()`
therefore calls `scePowerRequestStandby()`, the closest safe operation, rather
than using undocumented syscon commands. On real hardware this is a standby
request, not a guaranteed full shutdown; use the PSP's hardware power switch
for a true power-off. `power.suspend()` and `power.off()` may block until the
console wakes.

The host build substitutes inert power stubs and the `power.lua` demo never
calls `suspend()` or `off()`. PPSSPP 1.20.4 reports AC status and accepts
`tick()`, but its HLE does not implement `scePowerLock`/`scePowerUnlock`; the
demo labels those failures `HLE N/A`. PPSSPP does not model real suspend or
standby.

## sound

| Function | Description |
|---|---|
| `sound.load(path)` | Load WAV (PCM16) or VMA/VOX. Returns a handle or nil. |
| `sound.play(handle [, channel])` | Start. Channel is 1..8; omit for auto. |
| `sound.pause(handle [, mode])` | `-1` toggle, `0` resume, `1` pause. |
| `sound.stop(handle)` | Stop and mark end-of-stream. |
| `sound.loop(handle)` | Toggle looping; returns the new boolean state. |
| `sound.looping(handle)` | Query loop state. |
| `sound.playing(handle)` | True while playing and not paused. |
| `sound.endstream(handle)` | True after playback finished. |
| `sound.vol(handle [, 0..100])` | Set/get volume. |
| `sound.rate(handle)` | Output rate (always 22050). |
| `sound.free(handle)` | Release the sound and its channel. |

Audio is pumped by `screen.flip()`. Channels 1..8 are reserved explicitly and
released by `sound.stop`, `sound.free`, or natural end of playback; omit the
channel (or pass `0`) to reserve the next free channel.

## files

| Function | Description |
|---|---|
| `files.exists(path)` | Boolean. |
| `files.cdir()` / `files.cdir(path)` | Get/change working directory. |
| `files.nofile(path)` | Directory part. |
| `files.nopath(path)` | File name part. |
| `files.rename(old, new)` | Returns 1 or raises. |
| `files.copy(src, dst)` | Returns 1 or raises. |
| `files.move(src, dst)` | Returns 1 or raises. |
| `files.delete(path)` | Delete a file or directory tree; returns a boolean. |
| `files.mkdir(path)` | Create a directory and parents (best effort). |
| `files.size(path)` | Size in bytes, or -1. |
| `files.ext(path)` | Extension without the dot. |
| `files.list(path)` | Table of entries. |
| `files.listfiles(path)` | Files only. |
| `files.listdirs(path)` | Directories only. |

Each entry has `name`, `path`, `size`, `ext`, `directory` (boolean) and `mode`
(a three-character `RWE` string).

`files.scan`, `files.extract` and archive/thread variants are not implemented.

## ini

```lua
ini.read(path, key [, default])
ini.read(path, section, key [, default])
ini.write(path, key, value)
ini.write(path, section, key, value)
```

Text INI with `[section]` headers and `key = value` lines. Writes are
section-aware, sectionless keys stay before the first header, and file updates
use a temp file with an atomic move plus a backup fallback.

## data / savedata

```lua
data.save(payload, title, gameid, subtitle, description, mode)
data.load(gameid, mode)
data.delete(gameid)
```

- `mode = 0` — slot dialog semantics (`SLOT1..SLOT5`); save uses the first
  free slot and load uses the highest-numbered populated slot, then autosave.
- `mode = 1` — autosave slot (`AUTOSAVE`).
- Payload is capped at 256 bytes.
- `gameid` is 1..13 characters from `A-Z`, `a-z`, `0-9`, `-` and `_`;
  path separators and traversal sequences are rejected.
- `savedata` is an alias for `data`.
- On PSP, new saves go to the first writable root, `ms0:` (M2) then `ef0:`
  (PSP Go internal storage). An existing game stays on its current volume.
  Loads search both volumes and pick the highest-numbered slot; deletes remove
  the game directory from both.

The PSP VSH save dialog is not implemented; files are plain text under
`<root>PSP/SAVEDATA/<GAMEID>/`.

## json

```lua
local value = json.decode(text)
local text  = json.encode(value)
local value = json.load(path)
json.save(path, value)
```

- Objects decode to string-keyed tables; arrays decode to sequence tables
  (`1..n`). `null` decodes to `nil`, so a null object member is absent from
  the resulting table. Duplicate object keys use the last value.
- Decoding supports strings, numbers, booleans, `null`, nested objects/arrays,
  all JSON escapes including `\uXXXX`, and surrogate pairs. Raw UTF-8 bytes
  pass through unchanged.
- Encoding writes whole numbers without a decimal point and other numbers with
  enough precision for a round trip. Empty tables encode as `{}`. Sparse
  arrays, mixed array/object keys, non-string object keys, NaN and infinity
  raise an error.
- Malformed JSON, trailing data, excessive nesting and unsupported Lua values
  raise a `json.*` error rather than crashing.
- `json.load` reads at most 256 KB and raises if the file is missing,
  unreadable or malformed. `json.save` writes through a temp file and atomic
  move (with a backup fallback), returns `true` on success, and raises on
  failure. Parent directories must already exist.
- Object key order is not preserved.

## map

```lua
local m = map.new(tileset, mapData, tileW, tileH)
map.blit(m, x, y [, alpha])
map.sizex(m) / map.sizey(m)       -- pixels
map.cols(m) / map.rows(m)         -- tiles
map.set(m, tx, ty, value)
map.get(m, tx, ty)
map.free(m)
```

`mapData` is an array of rows; each row is an array of 1-based tile indices.
Tile `0` is empty. Tile sizes must be 8, 16, 32, 64, 128, 256 or 512 and must
divide the tileset. The tileset image is anchored by the map, so it is safe to
drop the Lua reference after `map.new`; call `map.free` when done. Map handles
are plain integers with no automatic finalizer, so explicit freeing is
required (#26).

## pmf

```lua
pmf.run(path [, x, y])   -- blocking; plays a PMF/PSMF file
pmf.play()               -- resume after pmf.pause()
pmf.pause()              -- pause at the next frame boundary
pmf.stop()               -- stop and restore the previous display buffer
```

`pmf.run` returns after playback finishes and raises on a bad header, missing
file, decoder failure or when another movie is already active. `x` and `y`
default to `0, 0` and place a smaller clip on the 480x272 screen; full-screen
480x272 clips cover it. Video is decoded with the PSP `sceMpeg` libraries and
ATRAC3+ audio is played through `sceAudioOutputBlocking`.

If the global `onPmfPlay(playing)` function exists, it is called for every
displayed frame with `true`, and on every pause update with `false`. The callback
may call `pmf.pause()`, `pmf.play()` or `pmf.stop()`:

```lua
function onPmfPlay(playing)
    if buttons.pressed(buttons.start) then pmf.stop() end
end

pmf.run("assets/splash.pmf")
```

The callback runs from inside `pmf.run`; avoid long work there because it delays
video and audio. MP4 containers are not supported; only PSMF/PMF files are.

## Planned modules

| Module | Status |
|---|---|
| `wlan` / `http` | Not implemented |

## Known API deviations

| Area | Current behaviour | Tracking |
|---|---|---|
| Transforms | Implemented and PPSSPP-verified; PSP hardware pending | #19, #20 |
| PMF audio | ATRAC3+ path implemented and verified through a PPSSPP WAV dump; no PSP hardware run yet | #5 |
| Battery mode/EEPROM | Read-only telemetry implemented; serial/mode return `"-"` and mode/EEPROM calls return `0` without hardware access | #33 |
| Power off | PSPSDK exposes no user-mode software shutdown; `power.off()` requests standby, and a full power-off needs the hardware switch | #34 |
