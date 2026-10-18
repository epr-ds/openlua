# OpenLua - Build Instructions

## Prerequisites
- Docker
- pspdev toolchain (via Docker image)

## Build
```bash
# Single build
bash docker/build.sh

# Clean rebuild
bash docker/build.sh clean   # then separately:
bash docker/build.sh
```
> Note: do NOT run `bash docker/build.sh clean all` together — `-j2` runs them in parallel and causes link errors.

## Output
- `EBOOT.PBP` — PSP game executable (deploy to `PSP/GAME/OpenLua/`)
- `openlua.elf` — intermediate ELF

## Directory Structure
```
engine/
  core/      main.c, olreg.c/h, stb_impl.c
  gfx/       gfx.h/c (surface+GU), draw.h/c (primitives)
  img/       image.h/c (Lua image module + stb loading)
  input/     buttons.h/c
  util/      timers.h/c
third_party/
  lua/src    Lua 5.1.5 sources
  stb        stb_image.h, stb_image_write.h
```

## Module API (Current)
- **color.new(r,g,b[,a])** → integer ABGR
- **screen.clear(color)** / **screen.flip()**
- **image.load(path[,w,h][,mask])** — PNG/JPEG/BMP/GIF + transparency mask
- **image.blit(img,x,y)** and **image.blit(img,x,y,sx,sy,w,h,alpha)**
- **image.blitsprite(img,x,y,frame[,alpha])**
- **image.fliph/flipv** — draw-time UV flip (not pixel copy!)
- **image.center/rotate/reset/copy/clear/pixel/...**
- **draw.line/rect/fillrect/circle**
- **buttons.read() → mask, lx, ly**
- **timers.new() → t; t:start(), :stoptime(), :time(), :reset()**
