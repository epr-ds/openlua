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
| OS password/CFW | `os.password()` returns `""` because PSPSDK has no public password API; `os.cfw()` returns `"UNK"` because no reliable CFW detector exists | #35 |
| OS dialogs/PRX | `os.message`/`messagebox`/`initprx`/`stopprx`/`requiere` are predictable no-side-effect stubs; no utility dialog or PRX is loaded | #35 |
| OS restart | `os.restart()` is explicit and PSP-only via `sceKernelLoadExec`; host calls are no-ops and the demo/tests never call it | #35 |
| Hardware identity | Motherboard/fuse/NAND/board/gen/region and Pandora data need kernel-mode IDStorage/syscon; OpenLua returns `"UNK"`, `0` or `false` | #36 |
| Hardware LED | `hw.led()` is present but returns `-1` because `sceSysconCtrlLED` is kernel-only | #36 |
| Hardware volume/mute | Uses `/CONFIG/SYSTEM/SOUND`; PPSSPP 1.20.4 lacks that registry category, so the session fallback `27`/`0` is reported and writes are not applied to the emulator mixer | #36 |
| Hardware audio route | `hw.audioout(mode)` uses `sceAudioRoutingSetMode`; PPSSPP 1.20.4's `GetMode` HLE always returns `0` | #36 |
