# OpenLua — Module Design Reference (Based on OneLua API)

This file documents the core modules used by OpenLua, organized as a design
reference. The public Lua API in `engine/core/main.c` should follow these
patterns: one global table per subsystem, camelCase method names, consistent
parameter ordering.

---

## 1. Core Conventions

| Convention | Detail |
|------------|--------|
| Global table per module | `screen`, `image`, `draw`, `font`, `buttons`, `timers` |
| Method style | `module.method(arg1, arg2, ...)` — C functions registered via `lua_pushcfunction` |
| Color type | Single integer: `0xAARRGGBB` (ARGB) — one helper: `color.new(r, g, b)` returns ABGR |
| Return convention | `nil` on success; return int/bool only when caller needs the value |
| Constants (if any) | `__NAME` with two leading underscores + uppercase (OneLua convention) |

---

## 2. Module Map (Target API Surface)

```
color.new(r, g, b [, a=255])      → int (ABGR)   ✅ DONE
screen.clear(color)                → nil          ✅ DONE
screen.flip()                      → nil          ✅ DONE

image.load(path)                   → surface      ✅ DONE (internal)
image.new(w, h [, color])          → surface
image.blit(surface, x, y)          → nil
image.blitsprite(surface, x, y, frame) → nil   — grid math: cols = realW / cellW
image.flipx(surface, set?)         → nil          ✅ DONE (toggle|set)
image.flipy(surface, set?)         → nil          ✅ DONE (toggle|set)
image.center(surface [, xi, yi])   → nil

draw.line(x1,y1,x2,y2,color)       → nil
draw.rect(x,y,w,h,color)           → nil
draw.fillrect(x,y,w,h,color)       → nil
draw.gradrect(x,y,w,h,{c1,c2,c3,c4}) → nil

font.load(path, cellW, cellH)      → font         ✅ DONE (phase 2 draft)
font.blt(font, x, y, code)         → int/nil
screen.print(x, y, text [, font])  → int          — see note * below

buttons.read()                     → int bitmask  ✅ DONE
buttons.waitforkey(key)            → nil
buttons.interval(delay, interval)  → nil          ✅ DONE (timers wrapper)
buttons.homepopup(state)           → nil

timer.new([initial_ms])            → timer        ✅ DONE
timer.start(t)                     → int(ms)      ✅ DONE
timer.stop(t)                      → int(ms)      ✅ DONE
timer.time(t)                      → int(ms)      ✅ DONE

sound.load(path)                   → sound (future Phase 2.5)
sound.play(sound, [channel])       → nil          PAudio: sceCtrl_
sound.loop(sound)                  → nil           Toggle loop on/off
sound.vol(sound, [%])              → int|nil       Set or get volume 0-100
```

---

## 3. OneLua-Compatible Design Decisions |

*One Lua used these patterns that we preserve for compatibility:*

### image module
- **`image.load(path [, w, h])`** — Returns an image/surface object. Width/height
  are optional hints for pre-allocation. Internally calls STBI to decode PNG, BMP.
- **`image.blit(src, x, y)`** — Draws `src` at position `(x,y)` on the current
  back buffer (not the source surface).
- **`image.blitsprite(src, x, y, frame)`** — Extracts a single frame from a
  spritesheet. Layout: left-to-right, top-to-bottom grid. The sheet's `realW`
  (power-of-2 GPU width) defines columns: `cols = realW / cellW`. Frame index
  maps to `(frame % cols, frame / cols)`.
- **`image.flipx(img [, set])`** — When called with one argument toggles flipX
  state on the surface. Called with a boolean argument the state is set to that
  value (`flipx(img, true)` = force-flip-on; `flipx(img, false)` = force-flip-off).
  The same pattern applies to `image.flipy()`.
- **`image.new(w, h [, color])`** — Allocates a blank surface. Optional `color`
  fills it at creation (0x00000000 = transparent).

### draw module  
- **`draw.line(x1,y1,x2,y2,color)`** — Draws a line between two points on the
  back buffer. Uses GE Gouraud line.
- **`draw.rect(x,y,w,h,color)`** — Outlined rectangle (stroke only).
- **`draw.fillrect(x,y,w,h,color)`** — Filled rectangle (solid fill).
- **`draw.gradrect(x,y,w,h,{c1,c2,c3,c4} [, mode])`** — Gradient rectangle with
  four corner colors. Mode: `"h"` horizontal, `"v"` vertical, `0` diagonal (default).

### font module (OneLua reference)
One Lua shipped **two** font APIs:
1. **`font.load(path)`** — Loads `.pgf` (PSP binary glyph font) for hardware-rendered text via PSP SDK's VIF/GME.
2. **`sfont.load(path)`** — Loads a SFont PNG spritesheet (pixel font).

OpenLua takes the **SFont approach** with additional features:
- `font = font.load("png", cellW, cellH)` — Parses glyph grid from the PNG.
  Each `cellW × cellH` pixel block maps to one character starting at `' '`
  (space=0). Characters are stored in a flat buffer or GPU texture. Returns an
  opaque `font` object usable with `image.blit`.
- `image.blit_font(font, x, y, code)` — Extracts frame `code` from the font's
  sheet and draws it at `(x,y)`. The glyph index is 0-based from space (0 → ' ',
  1 → '!', etc.).
- `screen.print(x, y, text [, font])` — Convenience wrapper: renders `text` by
  calling `blit_font()` for each character, advancing X by `cellW` per glyph.

### screen module
- **`screen.clear(color)`** — Fills back buffer with solid `color`. Uses GE fill.
- **`screen.flip()`** — Calls `sceGuFinish()`, `sceGuSync()`, then flips front
  and back buffers by swapping the display list pointer. Any modifications made are not visible until this function is called.

### buttons / timed modules
- **`buttons.read()`** — Returns a bitmask of currently-pressed buttons using PSP SDK controller reads via `sceCtrlReadBufferPositive`.
- **`buttons.waitforkey(key)`** — Polls buttons until `key` is pressed then returns control block.
- **`timers.new([initial_ms])`** → `TIMER` object. Tracks elapsed time since start.
  **`timer.start(t)` starts the timer, **`timer.stop(t)` stops it and returns the last measured delta. **`timer.time(t)` returns elapsed milliseconds so far.

### buttons module (OneLua reference)
- **`buttons.interval(delay, interval)`** — Sets global input rate: delays `delay` ms before first read, then reads every `interval` ms. Useful to reduce CPU usage vs. polling every frame.
- **`buttons.waitforkey(key)`** — Blocks until a specific button is pressed. Returns the current buttons value.

---

## 4. Architecture & Module Registration |

### Entry Point (`engine/core/main.c`)
```c
int main(int argc, char **argv) {
    flash_write("boot:main", 9);
    debug_note("boot: main");
    setup_callbacks();               // register exit callback
    chdir_to_appdir(...);            // find game dir from argv[0]
    
    debug_note("boot: gfx init...");
    gfx_init();                      // power-on GE, allocate buffers
    debug_note("boot: gfx ok");

    lua_State *L = luaL_newstate();
    if (!L) fatal_show("lua state");

    luaL_openlibs(L);                // core Lua libs (string, table, math..)
    register_api(L);                 // register our engine API (see below)

    const char *script = find_script(argc, argv);
    if (!script) fatal_show("no script.lua found in app dir");

    debug_note(script);
    if (luaL_dofile(L, script) != 0) {
        const char *err = lua_tostring(L, -1);
        debug_note("boot: lua error");
        // log it and show fatal screen
        lua_close(L);
        fatal_show(err ? err : "(unknown Lua error)");
    }

    debug_note("boot: script returned");
    lua_close(L);
    sceKernelExitGame();
    return 0;
}
```

### API Registration Pattern (`register_api(L)`)
```c
static void register_api(lua_State *L) {
    // color module — single helper
    lua_createtable(L, 0, 1);
    lua_pushcfunction(L, l_color_new);
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "color");

    // screen module — clear + flip
    lua_createtable(L, 0, 2);
    lua_pushcfunction(L, l_screen_clear);
    lua_setfield(L, -2, "clear");
    lua_pushcfunction(L, l_screen_flip);
    lua_setfield(L, -2, "flip");
    lua_setglobal(L, "screen");

    // sub-modules (each in its own engine/ directory):
    image_api_register(L);           // engine/img/image.c
    draw_api_register(L);            // engine/gfx/draw.c
    font_api_register(L);            // engine/font/font.c  (Phase 2)
    buttons_api_register(L);         // engine/input/buttons.c
    timers_api_register(L);          // engine/util/timers.c
}
```

---

## 5. Module File Layout

Each module lives in `engine/<category>/`:

```
engine/
├── core/           — main.c, olreg.h (Lua registration, PSP kernel glue)
├── gfx/            — draw.c/draw.h (primitives: lines, rects, gradients)
│                   — gfx.c/gfx.h    (surface struct, GE pipeline, blit)
├── img/            — image.c/image.h (load PNG via STB, flip, scale, tint)
├── font/           — font.c/font.h   (SFont sprite fonts + screen.print)
├── input/          — buttons.c/buttons.h (Ctrl ReadBuffer+ polling)
└── util/           — timers.c/timers.h (elapsed-milli counter)
third_party/        — lua/, stb/   (third-party no-modification code)
```

---

## 6. Notes for Future Expansion / Phase 2+ modules:

### sound module (PAudio integration, future Phase 2.5)
- `sound.load(path)` loads MP3/S3M/BGM/WAV via PAudio PSP SDK library.
- Returns opaque `SOUND` type (a struct with handle + metadata).
- Channel support: 8 stereo channels (`sound.play(snd, channel1..8)`).
- Loop state toggled via `sound.loop(snd)` and queried with `sound.looping(snd)`.

### image.loadv / load from memory — optional future addition for embedded bitmaps

### 3D module — deferred to Phase 4+; PSP SDK's `guMtxOrtho` etc.

---

*OneLua docs reference URL: http://onelua.x10.mx/psp/docs/en/index.html*
*Retrieved: 2026-10-18 — used for API compatibility mapping in this file.*
