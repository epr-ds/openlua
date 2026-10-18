# OpenLua — PSP Lua Engine

**A clean-room, OneLua-API-compatible Lua interpreter engine for the PlayStation Portable (PSP), built with PSPSDK & Lua 5.1.**

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Build](https://github.com/epr-ds/openlua/actions/workflows/build.yml/badge.svg)](https://github.com/epr-ds/openlua/actions)
[![Project Board](https://img.shields.io/badge/Project-Board-orange)](https://github.com/users/epr-ds/projects/4)

## Overview

OpenLua is a clean-room implementation of a OneLua-compatible engine for PSP homebrew development. It provides:

- **Fully compatible Lua 5.1** interpreter (clean-room rewrite, no OneLua source used)
- **OneLua-style modules**: `screen`, `image`, `draw`, `font`, `buttons`, `timers`, plus PAudio/Sound, file ops, WiFi/HTTP stubs
- **Sprite-based rendering** via PSP GE graphics pipeline: image loading (PNG → ABGR), blitting, flip, scale, rotation
- **SFont font rendering**: load PNG tilemaps for bitmap characters, draw text on screen
- **Build via Docker/PSPSDK**: one-command build, works on ARM64 Linux or native x86_64

## Quick Start

### Build

```bash
# One docker call (or two if preferred):
bash docker/build.sh

# Or separate steps:
bash docker/build.sh clean      # first step only
bash docker/build.sh            # second step only
```

Output: `EBOOT.PBP` (ready for `PSP/GAME/OpenLua/`)

### Install & Run

1. Copy the contents of this repo into `PSP/GAME/OpenLua/`:
2. Place your `script.lua` in the same dir
3. Launch from PSP memory stick or PPSSPP emulator  

In PPSSPP:
- Disable "software rendering" ✔️ 
- Enable "fast memory (unstable)" ✔️

## OneLua API Compatibility |

| OneLua Module | OpenLua Equivalent | Status |
|---------------|-------------------|--------|
| `screen` | `screen.clear(color)`, `screen.flip()` | ✅ Phase 1 |
| `image` | `image.load()`, `image.blit()`, `image.fliph()/flipv()` | ✅ Phase 1 | 
| `draw` | `draw.line()`, `rect()`, `fillrect()`, `gradrect()` | ✅ Phase 1 |
| `buttons` | `buttons.read()`, `waitforkey()`, `interval()` | ✅ Phase 1 |
| `timers` | `timer.new()`, `start()`, `stop()`, `time()` | ✅ Phase 1 |
| `font` (sfont) | `font.load(path, cellW, cellH)`, `blit_font()` | 🟡 Phase 2 | 
| `screen.print()` | Built-in with SFont support | 🟡 Phase 2 |
| `sound`       | `sound.load()`, `play()`, `loop()`, `vol()` | ⏳ Phase 3 |
| `files`       | `files.copy()`, `extract()` | ⏳ Phase 4 |
| HTTP / WiFi   | `wlan.*`, `http.get()` | ⏳ Milestone 5+ |

### Lua API Surface (Current)

```lua
-- Color helper
local color = require("lib").color
local red = color.new(255, 0, 0, 255)

-- Screen
screen.clear(color.rgb(0, 0, 0))    -- clear background
screen.flip()                        -- present frame

-- Image
local img = image.load("sprite.png")  -- PNG → surface
image.blit(img, 100, 80)             -- draw at x/y
image.fliph(img, true)               -- force-flip horizontally (toggle if omitted)
image.fipv(img, false)               -- force-flip vertically

-- Draw primitives
draw.line(10, 20, 30, 40, color.rgb(255, 255, 255))
draw.rect(50, 60, 128, 64, color.rgb(255, 0, 0))
draw.fillrect(50, 60, 128, 64, col

-- Font (SFont)
local font = font.load("font.png", 8, 16)    -- cellW=8, ceilH=16 pixels
image.font_blt(x, y, code, font)              -- draw one glyph
screen.print(50, 400, "Hello PSP!", font)     -- draw full text

-- Input
local buttons = require("buttons")
buttons.interval(10)   -- poll every 10ms instead of every frame
while true do
   local state = buttons.read()
   -- ... your game logic
end

-- Timers
local t = timer.new()       -- create new timer object
t:start()                   -- start counting
local elapsed = t:time()    -- return milliseconds since start
t:stop()                    -- stop, returns ms when stopped
```

### Transform Pipeline (Phase 1)

OpenLua supports **draw-time per-blit transforms**:
- `scale` — zoom before upload (nearest neighbor, for scale-down or upscale)
- `pivotX, pivotY` — center of rotation (default: w/2, h/)
- `angle` — rotation angle in degrees (counter-clockwise)
- `flipX`, `flipY` — boolean state; optional arg sets explicitly
- `zindex` — depth order per-blit (higher z = drawn later, on top)
- `tint` — ABGR color modulation (0xFFFFFFFF = no tint)

Example: layered transforms with rotation + scale + flip:

```lua
local img = image.load("sheet.png")
for i = 1, 360, 90 do
    image.flipx(img, i % 180 == 90)   -- flip every other frame 
    screen.print(100, 100, "Testing!", font)
end
```

## Project Structure / Engine Modules

| Module | Location | Description |
|--------|-----------|
| `core` | `engine/core/main.c` | Entry point, lua init |
| `gfx` | `engine/gfx/` | GE pipeline + surface struct (OL_Surface) |
| `draw` | `engine/gfx/draw.c` | Primitives: line, rect, fillrect, gradrect |
| `img` | `engine/img/image.c` | image load/flifp/blit via STB_image |
| `font` | `engine/font/` | SFont tilemap loader + blitter |
| `input` | `engine/input/buttons.c` | PSP controller input polling |
| `util` | `engine/util/timers.c` | Elapsed-millisecond timer |

```
openlua/
├── EBOOT.PBP        — built output (place in PSP/GAME/OpenLua/)
├── Makefile         — PSPSDK build rules
├── README.md        
├── docs/
│   └── onelua-reference/          — OneLua module docs (for API compat)
├── engine/                — one file per module
│   ├── core/
│   │   ├── main.c         — Entry point, Lua init
│   │   ├── olreg.h       — Header only  
│   │   └── main.o
│   ├── gfx/              — GFX pipeline header  
│   │   ├── draw.c       — graphics primitives (draw.line, rect, etc.)
│   │   ├── draw.h     
│   │   ├── gfx.c           — GE buffer setup + surface struct
│   │   └── gfx.h       
├── third_party/           — No-code dependencies: Lua 5.1.5 + stb_image/write  
│   ├── lua/src/
│   └── stb/

```

## Build Environment

| Component | Detail |
|---|---|
| `docker/p` | PSPSDK build environment (`pspdev/pspdk:latest`) |
| `bash docker/build.sh [clean]` | Build script (two-step recommended for Docker context) |
| Toolchain | psp-gcc, psp-ld, psp-strip, psp-fixup-imports |

## OneLua-Compatible Design Decisions

OpenLua follows the OneLua module naming and method signature conventions as a **compatibility target**, not as a strict dependency. Key design decisions:

- **Color** is always an integer `0xAARRGGBB` (ABGR) — matches OneLua's single-color value approach
- **Screens** is one global table (`screen.clear()`, `screen.flip()`)
- **Images** are opaque handles returned by `image.load()`, drawn via `image.blit()`
- **Fonts** use sprite-sheet tilemaps: load one PNG, extract glyphs by frame index
- **Buttons** returns a bitmask (like OneLua's `buttons.read()`)
- Timers return elapsed milliseconds; no "pause/resume" state in early versions

## Development Phases

See the [GitHub project board](https://github.com/users/epr-ds/projects/4) for issue tracking.

| Phase | Milestone | Status |
|-------|-----------|--------|
| Phase 1: Core Engine + Sprite Renderer | #1 (closed) | ✅ Complete |
| Phase 2: Font + Sound APIs | **#6** (open) | 🟢 In Progress | 
| Phase 3: Maps/Ini/Savedata | #3 | ⏳ Planned |
| Phase 4: PMF/MP4 Decoder | #4 | ⏳ Planned |
| Bug Fixes & Polish | #5 | 👁️ Ongoing |

## Contributing

- Issues welcome via [GitHub Issues](https://github.com/epr-ds/openlua/issues)
- For OneLua-compatible modules, see `docs/modules.md` for API surface reference
- Build environment requires Docker (x86_64) or native PSPSDK toolchain (`psp-`, `pspsdk`)

## License

MIT — see [LICENSE](LICENSE) for details. OpenLua is clean-room code; no OneLua source was used or modified.
