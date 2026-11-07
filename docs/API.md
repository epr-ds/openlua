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

### PGF system fonts (`font`)

`font.load(path)` is the OneLua PGF loader. It opens the PGF through the PSP
system library `sceLibFont`, rasterizes printable ASCII (`0x20`..`0x7E`) once
into a cached RGBA atlas, and returns a 1-based handle. No PGF file is bundled
with OpenLua; use a system font such as `flash0:/font/ltn0.pgf`. A failed load
returns `nil` rather than raising a Lua error.

| Function | Description |
|---|---|
| `font.load(path)` | Load a PGF system font. Returns a handle or nil. |
| `font.setdefault(handle)` | Select an already-loaded font for `screen.print`. |
| `font.setdefault(path)` | Load a PGF and select it. Returns nothing. |
| `font.setdefault()` | Return the current default handle, or nil. This getter is an OpenLua extension; in OneLua the no-argument form refers to its built-in default font. |
| `font.nchars(handle)` | Number of cached glyphs: 95 for printable ASCII. |
| `font.cell(handle)` | Returns the widest advance and the line height in pixels. |
| `font.draw(handle, x, y, code [, tint])` | Draw one ASCII code. Unsupported codes are skipped. |
| `font.print(handle, x, y, text [, tint])` | Draw a string; returns the final cursor X. |
| `font.textwidth(handle, text)` or `font.textwidth(text, handle)` | Measure a string in pixels. |
| `font.free(handle)` | Release the PGF library handles and atlas. |

Newlines advance by one line height; tabs advance by four space advances.
PGF advances and bearings are taken from the font metrics, so text is
proportionally spaced. `screen.print` and `screen.textwidth` use the default
selected by `font.setdefault` unless a handle is passed explicitly.

### Fixed-cell PNG fonts (`font.new`)

The fixed-cell loader remains available for existing OpenLua projects. It
loads a PNG sheet with `cellW` x `cellH` cells, glyphs starting at space
(`0x20`) in ASCII order, and returns a handle or nil.

| Function | Description |
|---|---|
| `font.new(path, cellW, cellH)` | Load a fixed-cell PNG sheet. Returns a handle or nil. |
| `font.load(path, cellW, cellH)` | Legacy alias for `font.new`; the one-argument form is PGF. |
| `font.nchars(handle)` | Number of glyphs in the sheet. |
| `font.cell(handle)` | Returns `cellW, cellH`. |
| `font.draw(handle, x, y, code [, tint])` | Draw one ASCII code. |
| `font.print(handle, x, y, text [, tint])` | Draw a string; returns final cursor X. |
| `font.textwidth(handle, text)` or `font.textwidth(text, handle)` | Measure a string. |
| `font.free(handle)` | Release the handle and its surface. |

For new code, prefer `font.load("flash0:/font/ltn0.pgf")` with the PSP system
font, or keep `font.new(path, cellW, cellH)` for a bundled PNG. Existing code
that calls `font.load(path, cellW, cellH)` keeps working unchanged. A
one-argument `font.load` now means PGF; it does not auto-detect PNG files.

### Classic variable-width SFont (`sfont`)

`sfont` is the OneLua-compatible variable-width font API. A classic SFont
sheet has magenta separator runs on its top row and one glyph per gap, starting
at ASCII `!` (`0x21`); advances come from the clear gap between separator runs.
A fixed 95-cell ASCII sheet (space through `~`) can also be loaded without
separators when its width is divisible by 95.

| Function | Description |
|---|---|
| `sfont.load(path)` | Load a classic SFont or fixed 95-cell sheet. Returns a handle or nil. |
| `sfont.print(handle, x, y, text)` | Draw a string with a white tint; returns the final cursor X. |
| `sfont.letters(handle)` | Glyph count (94 for classic `!`..`~`, 95 for a fixed sheet). |
| `sfont.width(handle, text)` | Measure a string in pixels. |

All three loaders share one handle table and the same drawing path. PNG and
SFont sheets are repacked into a texture atlas no wider than 512 px (the PSP GE
texture limit) on first draw; PGF is rasterized into the same kind of atlas
when it is loaded. This is internal and does not change handles, glyph counts
or advances. Sheet assets for this repository: `assets/font.png` (fixed 8x16
cells) and `assets/sfont.png` (classic separators). Regenerate both with
`python3 tools/gen_font.py`; PGF fonts come from the PSP firmware.

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

## os

OpenLua extends the standard Lua `os` table instead of replacing it.
`os.time`, `os.date` and `os.exit` remain the stock Lua 5.1 functions; the
entries below add the OneLua PSP surface. Host tests never call `os.exit` or
`os.restart`.

| Function | Returns | Behaviour |
|---|---|---|
| `os.delay(ms)` | `nil` | Sleeps for `ms` milliseconds. Non-positive values return immediately. |
| `os.getdate()` | String `DD/MM/YYYY @ HH:MM:SS` | Local time from `sceRtcGetCurrentClockLocalTime()`. |
| `os.nick()` | String | PSP nickname through `sceUtilityGetSystemParamString()`; `""` when unavailable. |
| `os.mac()` | String | Uppercase `XX:XX:XX:XX:XX:XX` from `sceWlanGetEtherAddr()`; all-zero when unavailable. |
| `os.language()` | String | Uppercase system language name, or `"UNKNOWN"`. |
| `os.password()` | `""` | PSPSDK has no public user-mode parental-password API. |
| `os.ram()` | Number | Free memory in bytes at the time of the call. |
| `os.totalram()` | Number | Free memory cached when the module registers; this is not physical installed RAM. |
| `os.cpu()` | Number | Current CPU clock in MHz. |
| `os.cpu(mhz)` | `nil` | Requests `mhz` in the 1..333 range; invalid values are a no-op. |
| `os.bus()` | Number | Current bus clock in MHz. |
| `os.infoms0()` | Table or `nil` | `{ max, free, used }` in bytes for `ms0:/`, or `nil` when the device/capacity is unavailable. |
| `os.infoef0()` | Table or `nil` | The same fields for `ef0:/`; `nil` on non-PSP-Go models and hosts. |
| `os.cfw()` | String | `"HOST"` in host builds, otherwise `"UNK"`; no reliable public CFW detector exists. |
| `os.versiontxt()` | String | Trimmed `flash0:/etc/version.txt` contents, or `""` when unreadable. Host builds return `"HOST"`. |
| `os.restart()` | `nil` | Explicit PSP-only relaunch through `sceKernelLoadExec(cwd/EBOOT.PBP)`; a host call is a no-op. |
| `os.message(msg [, mode])` | `nil` or `-1` | Predictable stub; no dialog is created. |
| `os.messagebox(title, msg)` | `0` | Predictable stub; no dialog is created. |
| `os.initprx(path [, argv])` | `0` | Predictable stub; no module is loaded. |
| `os.stopprx(id)` | `false` | Predictable stub; no module is stopped. |
| `os.requiere(path)` | `0` | Predictable stub; no module is loaded. |

Filesystem capacities are reported in bytes. On the host, `os.infoms0()` maps
to the current directory so the table shape is testable, while `os.infoef0()`
returns `nil`. PPSSPP may return an all-zero `statvfs`; OpenLua treats a zero
capacity as unavailable and returns `nil`.

Dialog and PRX calls are intentionally scoped out of this pass. They accept the
OneLua argument shapes and return the documented failure/no-selection values
without opening a utility dialog, loading a PRX or changing module state.
`os.restart()` is destructive by design: it is never called by the host tests
or the demo, and there is no automatic restart path.

```lua
local fs = os.infoms0()
if fs then
    print(string.format("%.1f / %.1f MB used",
                        fs.used / (1024 * 1024), fs.max / (1024 * 1024)))
end
print(os.nick(), os.mac(), os.getdate())
```

## hardware

The `hw` table follows the OneLua hardware surface. Calls that need kernel-only
IDStorage or syscon access keep their OneLua signatures but return documented
fallbacks; the EBOOT never imports a kernel driver.

| Function | Returns | Behaviour |
|---|---|---|
| `hw.getmodel()` | String | `"Emulator"` for PPSSPP's default nickname, `"Go"` when `ef0:/` is available, otherwise `"UNK"`. |
| `hw.firmware()` | String | Current firmware from `sceKernelDevkitVersion()`, for example `"6.60"`; `"UNK"` when unavailable. |
| `hw.region()` | String | `"--"`; IDStorage region data is kernel-only. |
| `hw.baryon()` | String | `"UNK"`; syscon is kernel-only. |
| `hw.tachyon()` | String | `"UNK"`; syscon is kernel-only. |
| `hw.pommel()` | Number | `0`; syscon is kernel-only. |
| `hw.fuseid()` | String | `"UNK"`; fuse data is kernel-only. |
| `hw.fusecfg()` | String | `"UNK"`; fuse data is kernel-only. |
| `hw.kirk()` | String | `"UNK"`; Kirk data is kernel-only. |
| `hw.spock()` | String | `"UNK"`; Spock data is kernel-only. |
| `hw.nandseed()` | String | `"UNK"`; IDStorage is kernel-only. |
| `hw.ofwinitial()` | String | `"UNK"`; factory firmware is stored in IDStorage. |
| `hw.board()` | String | `"UNK"`; motherboard type is derived from kernel-only data. |
| `hw.gen()` | String | `"UNK"`; motherboard generation is derived from kernel-only data. |
| `hw.pandora()` | Boolean | `false`; Pandora battery detection needs syscon. |
| `hw.nand()` | String | `"UNK"`; NAND size needs kernel/IDStorage access. |
| `hw.volume()` | Number | Current `/CONFIG/SYSTEM/SOUND/main_volume` value (normally 1..30). |
| `hw.volume(level)` | Number | Clamps `level` to 1..30, writes `main_volume`, and returns the clamped level. |
| `hw.mute()` | Number | `1` when `/CONFIG/SYSTEM/SOUND/mute` is set, otherwise `0`. |
| `hw.mute(mode)` | `nil` | Clamps `mode` to 0/1 and writes `mute`. |
| `hw.headphone()` | Number | `1` when the headphone is connected, otherwise `0`, from `sceHprmIsHeadphoneExist()`. |
| `hw.audioout()` | Number | Current `sceAudioRoutingGetMode()`: `0` headphones, `1` speakers, or `-1` on error. |
| `hw.audioout(mode)` | Number | Selects `0` headphones or `1` speakers; returns `0` on success, `<0` on error. |
| `hw.led(led, state)` | Number | `0` on success, `<0` on error. PSP builds return `-1` because `sceSysconCtrlLED` is kernel-only; host mocks accept LED indices 0..3. |

Volume and mute use the firmware sound registry through `libpspreg`. PPSSPP
1.20.4 does not expose the `/CONFIG/SYSTEM/SOUND` category, so OpenLua keeps a
session-local `27`/`0` fallback there; writes update that session value but do
not change PPSSPP's mixer. On a real PSP the registry write is attempted and
persisted when the category is available. Volume changes are registry settings,
not per-channel `sound.vol()` mixer levels.

PPSSPP 1.20.4's `sceAudioRoutingGetMode` HLE always reports `0`, even though
the default route is speakers; `hw.audioout(mode)` still selects and restores
the route. The PSP value is passed through directly.

LED indices follow OneLua: `0` Memory Stick, `1` WLAN, `2` power, `3` Bluetooth.
No LED call crashes or changes hardware from the user-mode build.

```lua
local model = hw.getmodel()
local level = hw.volume()
local muted = hw.mute() == 1
local phones = hw.headphone() == 1
```

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
| `files.copy(src, dst)` | Returns 1 or raises; works across volumes. |
| `files.move(src, dst)` | Returns 1 or raises; cross-volume moves use copy + delete. |
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
- On PSP, new saves go to the first writable root, `ms0:/` (M2) then
  `ef0:/` (PSP Go internal storage). An existing game stays on its current
  volume. Loads search both volumes and pick the highest-numbered slot; deletes
  remove the game directory from both.

The PSP VSH save dialog is not implemented; files are plain text under
`<root>/PSP/SAVEDATA/<GAMEID>/` (for example
`ef0:/PSP/SAVEDATA/OPENLUA1/AUTOSAVE`).

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

## splash

```lua
splash.show([path|table])     -- default: assets/splash.png
splash.old([path|table])      -- default: assets/splash_old.png
splash.oldschool()            -- default: assets/splash_oldschool.png
```

`show` and `old` accept a single PNG path or a table of paths. Each image is
shown for about two seconds as a slideshow; a fresh Cross, Circle or Start
press advances to the next one. Images are centred on black and downscaled
(never upscaled) to fit 480x272 while preserving their aspect ratio. Each side
must be 512 pixels or less, the GE texture limit used by the other image
modules.

Missing, unreadable and oversized images are skipped rather than raising. If
none of the requested images can be loaded, a built-in geometric fallback is
drawn, so a bad asset path never leaves the screen blank or breaks the GE.
Tables are limited to 32 entries. Argument type errors, non-string table
entries, too many entries and overlong paths raise a `splash` error.

The calls are blocking and are intended for start-up, before the main loop.
They pump audio, restore the 2D GE state (`gfx3d_finish`) before drawing, and
restore the previous clear colour afterwards, so normal `screen.*`/`draw.*`
calls work immediately after the splash. If the caller wants 3D again, call
`amg.begin()` as usual. `splash.oldschool()` is an original OpenLua tribute
screen, not the original OldSchool Library artwork.

On the host build the images are decoded and validated but no window or timing
loop runs; `test/host/test_splash.lua` covers the paths and fallback logic.
PPSSPP verification covers the actual rendering.

## osk

```lua
local text = osk.init(desc, text [, type [, textmax [, lines [, mode]]]])
```

Opens the PSP system keyboard and blocks until the user confirms or cancels.
`desc` is the description shown by the dialog and `text` is the initial UTF-8
string. Confirming returns the edited string, including `""` when the field is
empty. Cancelling, or a failure to start/update the dialog, returns `nil`.

Optional arguments follow the OneLua defaults and limits:

| Argument | Meaning | Default | Range |
|---|---|---|---|
| `type` | `1` normal keyboard, `2` numeric keypad | `1` | non-`2` values use normal |
| `textmax` | maximum characters | `128` | clamped to `1..512` |
| `lines` | visible input lines | `1` | clamped to `1..8` |
| `mode` | keyboard language (see below) | `-1` (firmware) | clamped to `-1..10` |

`mode` uses the OneLua language list: `0` Japanese, `1` English, `2` Spanish,
`3` German, `4` Italian, `5` Dutch, `6` Portuguese, `7` Russian, `8` Korean,
`9` Traditional Chinese, `10` Simplified Chinese and `-1` for the firmware
language. PSPSDK has no Chinese enum, so modes `9` and `10` fall back to the
firmware default; other out-of-range values also use the firmware default.
The normal keyboard uses `PSP_UTILITY_OSK_INPUTTYPE_ALL`; numeric mode uses
`PSP_UTILITY_OSK_INPUTTYPE_LATIN_DIGIT`.

Invalid argument types raise an `osk` error. The dialog is presented through
the normal GE/display path. Before returning, the module restores the previous
clear colour, finishes any 3D scene, clears both display buffers and leaves
the documented 2D GE state, so a normal `screen.*`/`draw.*` loop can resume
immediately. The call must not be made from inside a render pass.

On the host build there is no dialog: `osk.init` validates and echoes the
(truncated) text, and `osk._last()` exposes the resolved fields for tests. The
host hook is not part of the public PSP API. PPSSPP verification covers the
actual open, edit and confirm path; its utility HLE always reports
`PSP_UTILITY_OSK_RESULT_CHANGED` and has no cancel button, so the `nil`
(cancel) branch is covered by host tests and the PSP implementation only.

## game

```lua
local info = game.info(path)        -- PARAM.SFO fields, or nil
local icon = game.geticon0(path)    -- ICON0 image handle, or nil
local pic0 = game.getpic0(path)     -- PIC0 image handle, or nil
local pic1 = game.getpic1(path)     -- PIC1 image handle, or nil
```

Reads a PBP file and exposes only its metadata and artwork. `info` parses
PARAM.SFO and returns a table containing every valid entry: integer entries
(`0x0404`) become Lua numbers and UTF-8 strings (`0x0004`/`0x0204`) become Lua
strings. Common keys are `TITLE`, `TITLE_ID`, `CATEGORY`, `APP_VER`,
`PARENTAL_LEVEL` and `BOOTABLE`.

The image functions extract ICON0, PIC0 or PIC1, decode the image data in
memory and return a normal `image` handle (`image.blit`, `image.scale`,
`image.getw`, ...). Each call creates an independent handle that the normal
garbage collector frees.

Missing, unreadable or non-PBP files, truncated headers, out-of-range sections,
an oversized SFO (128 KB cap) or image (4 MB cap) and undecodable images return
`nil` instead of raising, so a bad path cannot break a menu script. Invalid Lua
argument types still raise an error. Paths are resolved like any other relative
file path, from the script's working directory.

Only PBP files are parsed; ISO/CSO images are not supported yet. The
destructive OneLua operations `game.pack`, `game.add`, `game.remove`,
`game.unpack` and `game.launch` are intentionally not registered.

On the host build the parser and Lua surface are covered by
`test/host/test_pbp.c` (53 checks) and `test/host/test_game.lua` (46
assertions) using the generated `assets/sample.pbp` fixture. PPSSPP
verification covers the extracted images and the rendered table. No PSP
hardware was available.

## cam

```lua
cam.init()                       -- 1/0: load the USB camera driver
cam.begin()                      -- 1/0: configure 480x272 video
cam.state()                      -- true/false: driver loaded
cam.available()                  -- true/false: camera connected
cam.render(x, y)                 -- draw the current frame
cam.render(x, y, w, h)           -- draw it scaled
local img = cam.toimage()        -- ordinary image handle, or nil
local ok = cam.shot(path)        -- 1/0: write a PNG snapshot

cam.direction()                  -- 1 = lens towards the user, 0 otherwise
cam.brightness([value])          -- 0..255, or -1; setter clamps
cam.contrast([value])            -- 0..255, or -1; setter clamps
cam.sharpness([value])           -- 0..255, or -1; setter clamps
cam.saturation([value])          -- 0..255, or -1; setter clamps
cam.zoom([value])                -- 1..5, or -1; setter clamps
cam.evlevel([value])             -- 1..17, or -1; setter clamps
cam.effect([value])              -- 1..7, or -1; setter clamps
cam.autoreverse([mode])          -- 1/0; mode 1 enables, 0 disables
cam.mirror([value])              -- 1/0, or -1; value 1 enables
cam.term()                       -- 1/0: stop video and unload the driver
```

`cam` drives the PSP GO! CAM USB accessory through `sceUsb`/`sceUsbCam`.
Call `cam.init()` before `cam.begin()`, and call `cam.term()` when finished.
The camera driver and USB storage mode cannot be active at the same time.
`render` displays the current JPEG frame, decoding it in memory; `toimage`
returns the same kind of handle as `image.load`, so `image.blit`, `image.scale`
and the normal garbage collector apply. `shot` decodes the current frame and
writes a PNG. Each render/snapshot call reads one camera frame.

All calls are safe when the camera is missing or unsupported. `init`/`begin`
return `0`, `state`/`available` return `false`, getters return `-1`,
`direction`/`autoreverse` return `0`, `render` is a no-op, and
`shot`/`toimage` return `0`/`nil`. Setters clamp their values and do nothing
without an active camera; invalid Lua argument types still raise an error.
`term` is idempotent.

Effect values follow OneLua's 1-based enum (`1` normal, `2` negative,
`3` black-and-white, `4` sepia, `5` blue, `6` red, `7` green). Zoom `1..5`
maps to the driver's 1x..5x range, and EV `1..17` maps to +2.0..-2.0 with
`8` as 0.0.

Host tests cover the absent-hardware contract, argument handling and the
OneLua/PSPSDK value converters (`test/host/test_cam.c`, 23 checks, and
`test/host/test_cam.lua`, 90 assertions). PPSSPP supplies a dummy camera
frame, so the active init/begin/render/toimage/shot path is PPSSPP-verified;
its control getters are not implemented in PPSSPP (`n/a`), and no real GO! CAM
hardware was available.

## umd

```lua
umd.init()      -- true/false: mount disc0: and wait for READY
umd.present()   -- true/false: disc in the drive
umd.type()      -- "GAME", "VIDEO", "AUDIO" or "-"
umd.gameid()    -- DISC_ID string, or "" when unavailable
umd.info()      -- PARAM.SFO table, or nil
umd.size()      -- mounted size in GB, 4 decimals, as a string
umd.term()      -- unmount disc0: (idempotent)
```

`umd.init` checks for a disc, activates `disc0:` and waits up to five seconds
for the drive to become ready. `umd.gameid` and `umd.info` read
`disc0:/PSP_GAME/PARAM.SFO` through the same bounds-checked parser as
`game.info`; a video/audio disc or an unreadable filesystem returns `""`/`nil`.

`umd.size` walks the mounted ISO9660 view and returns gigabytes with four
decimals (`"1.2345"`). It is an estimate of the visible files, not the raw
disc capacity: hidden sectors and anything beyond the walk budget are not
counted. Without a disc it returns `"0.0000"`.

OneLua's `umd.dumpiso` and `umd.launch` are intentionally **not registered**:
they write a disc image or hand control to another application and need a
separate safety review.

Absent hardware never raises: `init` is `false`, `present` is `false`, `type`
is `"-"`, `gameid` is `""`, `info` is `nil`, `size` is `"0.0000"` and `term`
does nothing. Argument errors still raise.

## usb

```lua
usb.mstick()       -- expose the memory stick
usb.umd()          -- expose the UMD as read-only ISO9660
usb.flash0()       -- expose flash0 read-only
usb.flash1()       -- expose flash1 read-only
usb.flash2()       -- expose flash2 read-only
usb.flash3()       -- expose flash3 read-only
usb.stop()         -- stop the connection
usb.isconnected()  -- true when a PC has taken the connection
usb.isactive()     -- true when a mode was activated
```

All modes return nothing. `isactive` reports whether activation was attempted
successfully, even without a USB cable; `isconnected` reports whether the PC
has actually established the link.

`mstick` uses the standard `USBBusDriver` + `USBStor_Driver` pair. The
`umd`/`flash*` modes additionally select a device through
`pspUsbDeviceSetDevice`, which needs Sony's optional `_usbdevice.prx` (usually
provided by custom firmware). When that module is absent the mode fails
safely: nothing is exposed and `isactive` stays `false`. Flash modes are
always requested read-only; the API never enables USB writes to flash.

USB storage and the GO! CAM camera share the USB port and cannot be active at
the same time. Call `cam.term()` before `usb.mstick()`, or `usb.stop()` before
`cam.init()`.

On the host every mode is inactive and every query is `false`. PPSSPP 1.20.4
implements the `sceUsb` state flags, so `usb.mstick`, `isactive` and
`isconnected` are PPSSPP-verified; it does not implement `_usbdevice`, so the
UMD/flash modes are documented but not PPSSPP-verified.

## sio

```lua
sio.init()              -- 1/0
sio.baud(baud)          -- nothing
sio.available()         -- bytes waiting, or 0
sio.read()              -- 0..255, or -1 when empty
sio.write(data)         -- nothing
sio.print(msg)          -- nothing
sio.putstring(data,len) -- nothing
sio.term()              -- 1/0
```

PSPSDK exposes no user-mode SIO/UART system call; the extra `pspuart`
library performs kernel-only register and interrupt-handler access. This
user-mode EBOOT therefore registers the documented signatures but stays
inactive: `init` is `0`, `available` is `0`, `read` is `-1`, sending is a
no-op and `term` is `1`. Scripts can probe `sio.init()` and fall back. Argument
types are still validated.

## ir

```lua
ir.init()      -- 1/0
ir.baud(baud)  -- nothing
ir.state()     -- true/false
ir.get()       -- 0..255, or -1 when empty
ir.put(data)   -- 1/0
ir.read()      -- string + length, or nil
ir.write(msg)  -- bytes sent, or -1
ir.term()      -- 1/0
```

Raw IR byte streams need a kernel-mode SIRC/IrDA driver and only PSP-1000
models have the IR port. PSPSDK's `sceSircsSend` sends complete remote-control
frames, not the byte-stream interface OneLua exposes. The module is therefore
registered for compatibility but inactive: `init` is `0`, `state` is `false`,
`get` is `-1`, `read` is `nil`, `put` is `0`, `write` is `-1` and `term` is
`1`. Argument types are still validated.

## wlan

```lua
wlan.init()                 -- true/false: acquire the shared network stack
wlan.term()                 -- release this module's reference
wlan.connect([profile])     -- 1/0: connect and wait up to 30 s
wlan.disconnect()           -- disconnect and release this module's reference
wlan.isconnected()          -- true/false
wlan.status()               -- apctl state, or -13 when networking is unavailable
wlan.getip()                -- dotted IPv4 string, or nil
wlan.strength()             -- signal strength, or 0
wlan.over()                 -- active profile name, or ""
wlan.getconfigs()           -- empty table
wlan.scan()                 -- empty table
wlan.autoconnect(mode[, arg]) -- validated no-op
wlan.autostatus()           -- false
```

`wlan` exposes connection state and an explicit connect/disconnect path. The
default profile is `1`; `status` returns the `sceNetApctl` state (notably `4`
means connected) or `-13` when the stack cannot be initialised. `getip` returns
an address only when the state is connected; on the host it always returns
`nil` so scripts follow the same not-connected path as an unconfigured PSP.

Sony's network-configuration dialog and its profile add/copy/delete/backup/
restore operations are intentionally **not registered**. PSPSDK has no
user-mode API that enumerates profiles or access points, so `getconfigs` and
`scan` return empty tables, and `autoconnect` validates its integer arguments
and returns without starting a background loop. All calls are safe when the
network stack or WiFi hardware is absent. `term` and `disconnect` release only
this module's reference to the shared stack.

## socket

```lua
local client, err = socket.connect(host[, port]) -- port defaults to 80
local server, err = socket.server([port])        -- port defaults to 80
local peer, err = server:accept()                -- blocking
local sent      = sock:send(data[, length])      -- bytes sent, or -1
local data, len = sock:recv([max])               -- string + length, or nil + 0
sock:close()
socket.term()                                    -- close all live sockets
```

Sockets are TCP-only Lua userdata. `connect` uses a 10-second connect timeout;
`accept` waits until a peer arrives or `socket.term()` closes the listener.
`send` sends all requested bytes and returns the count (or `-1`), while `recv`
returns a string and its length, or `nil` and `0` on error/EOF. The optional
`recv` limit defaults to 1024 bytes and is capped at 1 MiB. Module-call forms
such as `socket.send(sock, data)` and `socket.close(sock)` are also accepted.

At most 64 sockets may be live at once, and the userdata `__gc` metamethod
closes a handle that the script forgets. `socket.term()` closes every tracked
socket and is the deterministic shutdown path. Ports outside `1..65535` raise;
a failed connect/listen returns `nil, "connection failed"` or
`nil, "could not listen"`. UDP, TLS, hostname resolution diagnostics and
non-blocking mode are not exposed. This is a general TCP primitive: only run
trusted scripts, because a script can connect to any reachable host and port.

## http

```lua
local ok          = http.getfile(url, path[, background])
local ok          = http.postfile(url, path, body[, length])
local response, err = http.request(method, url[, body])
http.term()
```

`http.request` performs a synchronous HTTP/1.x request and returns a table with
`status` (number), `reason` (string), `url` (the original URL), `body` (string),
`length` (number) and `headers` (a name-to-value table). `GET`, `POST` and other
HTTP method tokens are sent as given; a non-`nil` body adds
`Content-Type: application/x-www-form-urlencoded` and `Content-Length`.
`getfile`/`postfile` write a successful (2xx) response body to the given path
and return `true`/`false`; existing files are overwritten. OneLua's third
`getfile` argument (background download) is validated but the request still
runs synchronously.

The client follows up to four `301`/`302`/`303`/`307`/`308` redirects, keeps
up to eight `Set-Cookie` values in memory for the next request until
`http.term()`, and decodes chunked and `Content-Length` bodies. Limits are a
15-second connect timeout, 16 KB of response headers, 16 MB of body and a
2048-byte URL. `https://` is rejected because PSPSDK has no TLS client; there
is no proxy, authentication, certificate validation or background thread.
Traffic is plaintext, so do not send credentials over untrusted networks.
`http.getfile` can write any path the application can write.

## ftp

```lua
ftp.init()   -- true/false: start the server
ftp.state()  -- true/false: server thread running
ftp.term()   -- stop the server and join/delete its thread
```

`ftp.init()` starts a small anonymous, read-only FTP server rooted at the
current working directory. Only passive mode is supported; `LIST`, `NLST`,
`SIZE`, `RETR` and `CWD` are available, while `STOR`, `APPE`, `DELE`, `MKD`,
`RMD`, `RNFR`, `RNTO` and `SITE` are rejected with `550 Read-only server`.
Path resolution refuses `..` traversal above the root and limits each path to
16 components. Directory listings are capped at 64 entries per command.

The server listens on port 21 by default; set `OPENLUA_FTP_PORT` to a valid
port before `ftp.init()` to override it (useful for host tests or a non-root
process). There is **no authentication**: while running, any device that can
reach the PSP can read the exposed directory tree. Start it only on a trusted
LAN, do not run it from a directory containing secrets, and always call
`ftp.term()` before leaving the app.

## adhoc

```lua
adhoc.init([gameid])  -- 1/0: join the adhoc group (up to 8 chars used)
adhoc.term()          -- leave and release the network stack
adhoc.getmac()        -- "AA:BB:CC:DD:EE:FF", or ""
adhoc.scan()          -- array of { name = ..., mac = ..., state = 1 }
adhoc.gettotal()      -- number of peers
```

`adhoc.init` normalises the optional game id to uppercase letters, digits,
`_` and `-` (default `OPENLUA`), requires the WLAN switch to be on, initialises
`sceNetAdhoc`/`sceNetAdhocctl`, joins the group and waits up to five seconds for
the control state to report connected. `scan` reads the current peer list;
`state` is always `1` (`__JOINED`) for peers returned by this call.

The matching-request and PDP data APIs are registered for compatibility but
remain inactive in this build: `sendrequest` returns `false`, `getrequest`
returns an empty table, `accept`/`reject` are validated no-ops, `send` returns
`0` and `recv` returns `nil`. Without a WLAN switch or adhoc-capable hardware,
`init` returns `0`, `getmac` is `""` and the peer queries are empty. Adhoc was
not verified on real hardware; see `docs/verification/README.md`.

## bluetooth

```lua
bluetooth.init()       -- false
bluetooth.term()       -- false
bluetooth.state()      -- false
bluetooth.available()  -- false
```

The PSP has no built-in Bluetooth radio and PSPSDK exposes no Bluetooth stack.
This table is a compatibility stub for scripts that probe for the module: all
four calls return `false` without changing system state. There is no pairing,
device discovery or data-transfer API.

## 3D modules (`amg`, `cam3d`, `texture3d`, `model3d`)

The 3D subset is original code: a small OBJ/MTL parser (`engine/3d/obj.c`),
portable camera/model/texture state, and a PSP GE renderer
(`engine/gfx/gfx3d.c`). Models are drawn with depth testing through the GE;
`amg` also owns the 2D/3D state switch.

### Scene control: `amg`

```lua
amg.init()
amg.begin()
amg.mode2d(0)                 -- back to 3D state
amg.mode2d(1)                 -- disable depth, restore 2D state
amg.update()                  -- present the 3D frame and pump audio
amg.finish()                  -- restore the 2D defaults
amg.perspective(fov)          -- 10..170 degrees
amg.renderstyle(__TRIANGLES)
amg.fog(near, far, color)     -- ABGR colour
amg.fog()                     -- disable fog
amg.quality(q)                -- stored; no visual effect yet
```

Typical frame:

```lua
screen.clear(color.new(8, 10, 24))
amg.begin()
model3d.render(model, __ALL)
amg.mode2d(1)                 -- required before screen.print / draw / image
screen.print(8, 8, "HUD")
amg.update()
```

`amg.begin()` enables depth testing, clamps depth to the far plane, clears the
depth buffer and applies the current camera. Colour is cleared by the frame
flip, so `screen.clear` still selects the next background colour. `amg.update`
is the 3D equivalent of `screen.flip()`; do not call both for the same frame.

Constants registered globally:

| Constant | Value | Constant | Value |
|---|---|---|---|
| `__8888` | 0 | `__NEAR` | 0 |
| `__4444` | 1 | `__LINEAR` | 1 |
| `__5551` | 2 | `__ENV` | 0 |
| `__5650` | 3 | `__COORDS` | 1 |
| `__RAM` | 0 | `__DIRECTIONAL` | 0 |
| `__VRAM` | 1 | `__POINT` | 1 |
| `__POINTS` | 0 | `__SPOT` | 2 |
| `__LINES` | 1 | `__BOX` | 0 |
| `__TRIANGLES` | 2 | `__SPHERE` | 1 |
| `__FRONT` | 0 | `__CONE` | 2 |
| `__TRANS` | 1 | `__CYLINDER` | 3 |
| `__ALL` | 2 | `__CONVEX` | 4 |

Lighting, raytracing, collision and screenshot constants exist for source
compatibility but the corresponding functions are not registered.

### Camera: `cam3d`

```lua
local cam = cam3d.new()       -- position (0, 0, 6), eye (0, 0, 0), up (0, 1, 0)
cam3d.position(cam, {x, y, z})
cam3d.eye(cam, {x, y, z})     -- look-at target
cam3d.up(cam, {x, y, z})
cam3d.rotation(cam, {x, y, z})-- scene rotation in degrees
cam3d.set(cam)                -- copy the camera into the GE renderer
cam3d.free(cam)
```

Vectors accept `{x, y, z}` arrays or `{x=..., y=..., z=...}` tables. `set` must
be called before rendering; later edits do not apply until the next `set`.
Cameras are plain 1-based integer handles (up to 16).

### Textures: `texture3d`

```lua
local tex = texture3d.load("assets/3d/checker.png" [, location])
local tex = texture3d.new(width, height [, location])
texture3d.setcustom(model, object, slot, tex)
texture3d.filter(model [, object], __NEAR)
texture3d.quality(tex, __8888)      -- stored; pixels stay 8888
texture3d.position(tex, x, y)
texture3d.scaling(tex, sx, sy)
texture3d.trans(tex, u, v)          -- stored; not applied by draw() yet
texture3d.setframes(tex, frames)
texture3d.frame(tex, index)
texture3d.draw(tex)                 -- 2D blit; call in mode2d(1)
texture3d.free(tex)
texture3d.finish()                  -- free every 3D texture
```

PNG textures are decoded to ABGR `OL_Surface` buffers and must be 512x512 or
smaller (power-of-two dimensions are not required). `location`/`quality` are
recorded for OneLua parity; both RAM and VRAM requests are uploaded from the
same RAM surface. `setcustom` replaces the material texture for the whole
object; the `slot` argument is accepted but ignored. `filter` is the same
operation as `model3d`'s internal filter setting (`__NEAR` or `__LINEAR`).
`draw` uses `position`, `scaling` and the current frame row; it is a normal 2D
sprite and is subject to the usual 2D state.

### Models: `model3d`

```lua
local m = model3d.load(path [, css])
model3d.countobj(m)
model3d.render(m, draw_type [, object])
model3d.rotation(m, object, {x, y, z})
model3d.getrotation(m, object)
model3d.position(m, object, {x, y, z})
model3d.getposition(m, object)
model3d.scaling(m, object, {x, y, z})
model3d.getscaling(m, object)
model3d.free(m)
```

`load` parses Wavefront OBJ and its `mtllib` MTL. Polygons are triangulated as
fans; positions, `vt` texture coordinates, `usemtl`, `Kd`, `d` and `map_Kd`
are supported. `map_Kd` is resolved relative to the MTL file and loaded as a
`texture3d` texture. Faces are batched per material, and objects come from the
OBJ `o`/`g` grouping (a single implicit object is created when absent).

`render` submits every batch with depth testing. `draw_type` accepts
`__FRONT`, `__TRANS` and `__ALL`; this pass draws all three identically (no
separate transparent pass or back-face sorting). `object` is 0-based: a
non-negative value renders only that object, a negative value (`-1`, `-2`, ...)
skips that object, and `nil` renders all. Rotations are in degrees and applied
X, then Y, then Z before scaling; positions are in world units. Each model
animate (`css`) argument is accepted but ignored; OBJ/MTL animation is not
implemented yet.

### Not implemented

| OneLua/AMGLib area | Status |
|---|---|
| Lighting (`amg.light`, materials/lights) | Not registered |
| Raytracing, collision tests, culling helpers | Not registered |
| Model animation (`css`), skinning, morph targets | Argument ignored |
| Transparent face sorting and `__FRONT`/`__TRANS` passes | All faces use one depth-tested pass |
| Texture `__4444`/`__5551`/`__5650` storage | Quality is stored; pixels remain ABGR 8888 |
| VRAM placement | Accepted; textures stay in RAM |
| Texture `trans` scrolling | Stored; not applied by `texture3d.draw` |
| OBJ normals, multiple UV sets, smoothing groups | Parsed as needed for geometry; normals/smoothing unused |
| Real PSP hardware verification | Only PPSSPP was available; see `docs/verification/README.md` |

## Planned modules

| Module | Status |
|---|---|
| Archive extraction (`files.extract`, zip/rar) | Not implemented |
| VSH save dialog | Not implemented |
| IRC client | Not implemented |
| MP4 container support | Not implemented (PMF/PSMF playback works) |

## Known API deviations

| Area | Current behaviour | Tracking |
|---|---|---|
| WLAN profiles | The Sony profile dialog and add/copy/delete/backup/restore calls are not registered; `getconfigs`/`scan` return empty tables and `autoconnect` is a validated no-op | #45 |
| HTTP transport | Synchronous plaintext HTTP/1.x only; no TLS, proxy, authentication, certificate validation or background downloads. `getfile`'s background flag is accepted but ignored | #45 |
| HTTP limits | 16 MB response body, 16 KB headers, 4 redirects, 8 in-memory cookies and a 15 s connect timeout | #45 |
| FTP | Anonymous read-only passive server rooted at the working directory; no authentication, writes, renames or deletes | #45 |
| Sockets | Blocking TCP only; no UDP, TLS or non-blocking mode. 64 live handles maximum and 1 MB per `recv` | #45 |
| Adhoc matching/PDP | `sendrequest` returns `false`, `getrequest` `{}`, `accept`/`reject` do nothing, `send` returns `0` and `recv` returns `nil`; peer-list scanning is implemented | #45 |
| Bluetooth | Compatibility stub; all calls return `false` because the PSP has no built-in radio and PSPSDK has no stack | #45 |
| Network verification | Loopback socket/HTTP/FTP paths are host-tested; PPSSPP verified the registered modules and safe absence only. No WiFi, adhoc, Bluetooth or real-LAN hardware was available | #45 |
| Splash assets | `splash.show`/`old` share the same renderer and differ only in their default asset; `oldschool` is an original tribute rather than the historical OSLib artwork | #41 |
| Splash timing | Fixed at about 2 s per image with a 250 ms fade-in and 300 ms fade-out; skip advances one image at a time | #41 |
| OSK cancel verification | PPSSPP 1.20.4 has no OSK cancel control and its HLE always returns `RESULT_CHANGED`; confirm/edit is PPSSPP-verified, the `nil` cancel branch is host-tested and PSP-only | #42 |
| OSK Chinese modes | PSPSDK has no Chinese input-language value, so OneLua modes `9`/`10` use the firmware default | #42 |
| Camera controls in PPSSPP | PPSSPP 1.20.4 provides dummy video frames but its `sceUsbCam` get/set control HLE is unimplemented, so getters show `n/a` there; the setters still run on real hardware | #43 |
| Camera hardware verification | The active path was checked with PPSSPP dummy frames only; no GO! CAM accessory or PSP hardware was available | #43 |
| UMD size | `umd.size` sums the mounted ISO9660 tree with a depth/entry budget, so it is an estimate of visible files rather than raw disc capacity | #44 |
| UMD dump/launch | `umd.dumpiso` and `umd.launch` are not registered; they write disc images or start other applications | #44 |
| USB device modes | `usb.umd`/`flash*` need Sony's `_usbdevice.prx`, which is not part of PSPSDK; without it they fail safely and `isactive` stays `false` | #44 |
| USB flash writes | Flash modes are always requested read-only; there is no way to expose writable flash through this API | #44 |
| USB and camera | Both use the USB port and cannot be active together; call `cam.term` or `usb.stop` before switching | #43, #44 |
| SIO | User-mode PSPSDK has no safe UART syscall, so `sio.init` is `0`, `read` is `-1` and sends are no-ops | #44 |
| IR | Raw IR needs a kernel SIRC/IrDA driver and only PSP-1000 has the port; `ir.init` is `0`, `read` is `nil` and `write` is `-1` | #44 |
| Peripheral verification | UMD present/init/type and USB state flags were checked in PPSSPP only; game-disc metadata, flash/UMD USB modes and SIO/IR need real hardware or CFW | #44 |
| 3D modules | `amg`/`cam3d`/`texture3d`/`model3d` support the documented subset (textured static OBJ models, camera, fog, points/lines/triangles); lighting, animation and transparent passes are not implemented | #39 |
| 3D verification | Textured rotating cube and the 2D sfont regression were checked in PPSSPP only; no PSP hardware was available | #39 |
| Transforms | Implemented and PPSSPP-verified; PSP hardware pending | #19, #20 |
| PMF audio | ATRAC3+ path implemented and verified through a PPSSPP WAV dump; no PSP hardware run yet | #5 |
| Battery mode/EEPROM | Read-only telemetry implemented; serial/mode return `"-"` and mode/EEPROM calls return `0` without hardware access | #33 |
| Power off | PSPSDK exposes no user-mode software shutdown; `power.off()` requests standby, and a full power-off needs the hardware switch | #34 |
| OS password/CFW | `os.password()` returns `""` because PSPSDK has no public password API; `os.cfw()` returns `"UNK"` because no reliable CFW detector exists | #35 |
| OS dialogs/PRX | `os.message`/`messagebox`/`initprx`/`stopprx`/`requiere` are predictable no-side-effect stubs; no utility dialog or PRX is loaded | #35 |
| OS restart | `os.restart()` is explicit and PSP-only via `sceKernelLoadExec`; host calls are no-ops and the demo/tests never call it | #35 |
| Hardware identity | Motherboard/fuse/NAND/board/gen/region and Pandora data need kernel-mode IDStorage/syscon; OpenLua returns `"UNK"`, `0` or `false` | #36 |
| Hardware LED | `hw.led()` is present but returns `-1` because `sceSysconCtrlLED` is kernel-only | #36 |
| Hardware volume/mute | Uses `/CONFIG/SYSTEM/SOUND`; PPSSPP 1.20.4 lacks that registry category, so the session fallback `27`/`0` is reported and writes are not applied to the emulator mixer | #36 |
| Hardware audio route | `hw.audioout(mode)` uses `sceAudioRoutingSetMode`; PPSSPP 1.20.4's `GetMode` HLE always returns `0` | #36 |
