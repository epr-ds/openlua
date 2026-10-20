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

`image` values are surfaces. Use the module-call form: `image.getw(img)`.
There are currently no `img:method()` methods (#27).

### Loading and creating

| Function | Description |
|---|---|
| `image.load(path [, frameW, frameH [, mask]])` | Decode PNG/JPEG/BMP/GIF. With `frameW`/`frameH` the sheet is split into a grid. With a 2nd argument only, that value is a transparency mask colour. Raises on failure. |
| `image.loadv(path [, ...])` | Alias for `image.load`. |
| `image.new(w, h [, color])` | Create a blank surface, optionally filled. |
| `image.copy(img)` | Clone a surface. |
| `image.save(img, path)` | Write a PNG. Return value is currently ignored. |

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
| `image.fliph(img [, set])` | Set horizontal flip; omit `set` to toggle. |
| `image.flipv(img [, set])` | Set vertical flip; omit `set` to toggle. |
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
| `font.new(path, cellW, cellH)` | Load an SFont PNG sheet. Returns a handle or nil. |
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

Audio is pumped by `screen.flip()`. Explicit channel reservation is currently
buggy (#25).

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
section-aware; the non-final-section bug is tracked in #23.

## data / savedata

```lua
data.save(payload, title, gameid, subtitle, description, mode)
data.load(gameid, mode)
data.delete(gameid)
```

- `mode = 0` — slot dialog semantics (`SLOT1..SLOT5`), first free slot on save.
- `mode = 1` — autosave slot (`AUTOSAVE`).
- Payload is capped at 256 bytes.
- `savedata` is an alias for `data`.

The PSP VSH save dialog is not implemented; files are plain text under
`PSP/SAVEDATA/<GAMEID>/`.

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
divide the tileset. The tileset image must stay alive while the map is used
(#26).

## Planned modules

| Module | Status |
|---|---|
| `json` | Planned (#30) |
| `pmf` / MP4 playback | Planned (#5) |
| `wlan` / `http` | Not implemented |

## Known API deviations

| Area | Current behaviour | Tracking |
|---|---|---|
| Image methods | Module-call only, no `img:method()` | #27 |
| Flip names | `fliph`/`flipv` (docs elsewhere say `flipx`/`flipy`) | #27 |
| Timers | Negative elapsed time; initial ms ignored | #21 |
| Transforms | Fixed in code; pending PSP/PPSSPP device verification | #19, #20 |
| Sound channels | Explicit channel is not reserved | #25 |
| Map lifetime | Tileset is borrowed, not anchored | #26 |
