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
| `font.lua` | SFont loading, `screen.print`, tint, text width | `assets/font.png` |
| `math.lua` | `math.minmax` clamping and `bitand`/`bitor`/`bitnot` bit patterns | none (built-in 5x7 font) |
| `battery.lua` | Read-only `batt.*` telemetry: level, time, temperature, voltage, capacity | none (built-in 5x7 font) |
| `power.lua` | `power.*`: AC status, switch lock/unlock and tick; suspend/off shown but not invoked | none (built-in 5x7 font) |
| `os.lua` | `os.*` system info: nickname, language, MAC, CFW/version, date, CPU/bus, RAM and storage; destructive/dialog calls marked not run | none (built-in 5x7 font) |
| `sound.lua` | WAV load/loop/volume, audio pumped by `screen.flip` | `assets/beep.wav` |
| `transforms.lua` | Rotation, scale, tint, pivot | `assets/test_sheet.png` |

## Assets

| File | Size | Regenerate with |
|---|---|---|
| `assets/font.png` | ASCII 0x20..0x7E, 8x16 cells | `python3 tools/gen_font.py` |
| `assets/beep.wav` | 2 s, 440 Hz mono tone | `python3 tools/gen_tone.py` |
| `assets/test_sheet.png` | 4-frame 64x64 sprite sheet | checked in |

## Known caveats

- Image instance functions support both module-call (`image.getw(img)`) and
  method (`img:getw()`) forms.
- Transforms are per-surface state. Reset flips/angle/scale/tint between draws.
- Frame lifecycle, scaled blits and transform composition were reworked in
  #19/#20; visual confirmation on PSP/PPSSPP is still pending.
- `screen.print` requires a font; load one with `font.new` and select it with
  `screen.font`.
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
