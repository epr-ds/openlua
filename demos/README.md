# Demos

Small scripts that exercise the public Lua API. They are deliberately
standalone: the engine loads a single `script.lua` from its working directory.

## Running

1. Build/deploy the engine (see [../docs/DEVELOPMENT.md](../docs/DEVELOPMENT.md)).
2. Copy a demo to the app directory as `script.lua`:

   ```bash
   cp demos/smoke.lua /path/to/PSP/GAME/OpenLua/script.lua
   ```

3. Copy the `assets/` directory next to `script.lua`, or run from the
   repository root where `assets/` already exists.

All paths in the demos are relative to the working directory, not to the demo
file itself.

## Index

| Demo | What it shows | Assets |
|---|---|---|
| `smoke.lua` | Minimal boot check: clear, fill, outline, line | none |
| `flip.lua` | `image.fliph` / `image.flipv`, sprite frames, source rects | `assets/test_sheet.png` |
| `pgf.lua` | PGF system font through `font.load`/`font.setdefault`: proportional print, metrics, cell and glyph count | `flash0:/font/ltn0.pgf` (system) |
| `font.lua` | Fixed-cell font loading, `screen.print`, tint, text width | `assets/font.png` |
| `sfont.lua` | Classic variable-width SFont: `sfont.load`, `print`, `width`, `letters`; fixed-font header | `assets/sfont.png`, `assets/font.png` |
| `math.lua` | `math.minmax` clamping and `bitand`/`bitor`/`bitnot` bit patterns | none (built-in 5x7 font) |
| `battery.lua` | Read-only `batt.*` telemetry: level, time, temperature, voltage, capacity | none (built-in 5x7 font) |
| `power.lua` | `power.*`: AC status, switch lock/unlock and tick; suspend/off shown but not invoked | none (built-in 5x7 font) |
| `os.lua` | `os.*` system info: nickname, language, MAC, CFW/version, date, CPU/bus, RAM and storage; destructive/dialog calls marked not run | none (built-in 5x7 font) |
| `hardware.lua` | `hw.*` identity, firmware, volume/mute, headphone and audio route; kernel-only ID/LED fields marked as fallbacks, setters not run | none (built-in 5x7 font) |
| `sound.lua` | WAV load/loop/volume, audio pumped by `screen.flip` | `assets/beep.wav` |
| `transforms.lua` | Rotation, scale, tint, pivot | `assets/test_sheet.png` |
| `3d.lua` | Textured rotating OBJ cube: `amg` scene, `cam3d` camera, `model3d` OBJ/MTL load, `texture3d` PNG, 3D→2D HUD switch | `assets/3d/cube.obj`, `assets/3d/cube.mtl`, `assets/3d/checker.png` |

## Assets

| File | Size | Regenerate with |
|---|---|---|
| `assets/font.png` | ASCII 0x20..0x7E, 8x16 cells | `python3 tools/gen_font.py` |
| `assets/sfont.png` | Classic SFont, magenta top-row separators, glyphs 0x21..0x7E | `python3 tools/gen_font.py` |
| `assets/beep.wav` | 2 s, 440 Hz mono tone | `python3 tools/gen_tone.py` |
| `assets/test_sheet.png` | 4-frame 64x64 sprite sheet | checked in |
| `assets/3d/cube.obj` | Original 8-vertex textured cube | `python3 tools/gen_3d_assets.py` |
| `assets/3d/cube.mtl` | Cube material (`Kd`, `map_Kd`) | `python3 tools/gen_3d_assets.py` |
| `assets/3d/checker.png` | Original 128x128 checker texture | `python3 tools/gen_3d_assets.py` |

## Known caveats

- Image instance functions support both module-call (`image.getw(img)`) and
  method (`img:getw()`) forms.
- Transforms are per-surface state. Reset flips/angle/scale/tint between draws.
- Frame lifecycle, scaled blits and transform composition were reworked in
  #19/#20; visual confirmation on PSP/PPSSPP is still pending.
- `screen.print` requires a font. Load a PGF system font with
  `font.load("flash0:/font/ltn0.pgf")` and select it with `font.setdefault`
  (or `screen.font`), or load a bundled fixed-cell PNG with
  `font.new(path, cellW, cellH)`. Classic variable-width sheets use
  `sfont.load` and the `sfont.*` functions instead.
- Font sheets wider than 512 px are repacked into a GE-safe atlas internally;
  the generated 8x16 sheet maps lowercase glyphs to uppercase shapes. PGF
  uses printable ASCII (`0x20`..`0x7E`) and system metrics; the demo does not
  redistribute a Sony PGF file.
- 3D demos must keep scene state explicit: `amg.begin()` for 3D,
  `amg.mode2d(1)` before any `screen.print`/`draw.*`/`image.*`, then
  `amg.update()` instead of `screen.flip()`. 3D texture UVs are normalised;
  `texture3d` quality/location are stored for parity but pixels stay ABGR 8888
  in RAM. Lighting, animation and transparent face sorting are not implemented
  yet; see [../docs/API.md](../docs/API.md) and #39.
- `batt.mode()`/`batt.serial()` return `"-"`; mode changes and EEPROM
  backup/restore are safe no-ops. See [../docs/API.md](../docs/API.md).
- `power.suspend()`/`power.off()` can sleep a real PSP, so the power demo never
  calls them. `power.off()` requests standby, not a true shutdown; use the
  hardware power switch for that (#34).
- PPSSPP 1.20.4 does not implement `scePowerLock`/`scePowerUnlock`; the power
  demo shows `LOCK`/`UNLOCK` as `NO` with `HLE N/A` there. They call the real
  PSPSDK functions on a PSP.
- `os.exit()` and `os.restart()` can close or relaunch the app, so the OS demo
  never calls them. Dialog/PRX calls are stubs in this pass; `os.password()`
  returns `""` and `os.cfw()` returns `"HOST"`/`"UNK"` because PSPSDK has no
  public password or reliable CFW query (#35).
- The hardware demo only reads values. Motherboard/fuse/NAND/region and LED
  control need kernel mode, so they show `UNK`/`N/A`; `hw.volume()`,
  `hw.mute()`, `hw.audioout()` and `hw.led()` setters are not called (#36).
- PPSSPP 1.20.4 does not expose `/CONFIG/SYSTEM/SOUND`, so it shows the
  session fallback `27`/`0`; `sceAudioRoutingGetMode` also always returns `0`
  there. On a PSP the registry is written when available and the routing value
  is passed through (#36).
