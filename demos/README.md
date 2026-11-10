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
| `splash.lua` | `splash.show`/`old`/`oldschool`, single images and a slideshow, then the normal 2D frame loop | `assets/splash.png`, `assets/splash_old.png`, `assets/splash_oldschool.png` |
| `osk.lua` | `osk.init` with the normal keyboard, a 20-character limit and English mode; shows the returned string or the `nil` cancel branch | `assets/font.png` |
| `game.lua` | Read-only PBP metadata: `game.info` PARAM.SFO fields, plus ICON0/PIC0/PIC1 extracted as ordinary image handles | `assets/sample.pbp`, `assets/font.png` |
| `cam.lua` | GO! CAM lifecycle, live `cam.render`, `cam.toimage`, PNG `cam.shot` and the image controls; shows the safe absent-hardware path | `assets/font.png` (camera is optional) |
| `peripherals.lua` | UMD present/init/type/gameid/info/size, USB mass-storage flags, and the documented SIO/IR availability results | `assets/font.png` |
| `network.lua` | `wlan` state/queries, loopback socket and HTTP failure paths, `ftp` state, `adhoc` lifecycle/peer queries and the Bluetooth stub; shows the safe no-network result | `assets/font.png` |
| `storage.lua` | `files.cdir`, `os.infoms0`/`infoef0`, savedata save/load/delete and `files.mkdir/list/copy/move/delete` across `ms0:`/`ef0:`; cleans up its probe directory | `assets/font.png` |
| `tester.lua` | On-device test menu for every module: per-page checks, interactive hardware actions, model profiles and a START batch report. See [Hardware test menu](#hardware-test-menu) | all of `assets/` (falls back to a built-in font) |

## Hardware test menu

`tester.lua` is the self-contained menu used to exercise the whole engine on a
PSP-1000, PSP-2000/3000, PSP-E1000 and PSP Go. Copy it as `script.lua` next to
`EBOOT.PBP` and `assets/`, or stage a ready tree with:

```bash
bash tools/package_tester.sh
```

Controls: UP/DOWN move, X or O open, O back, X run or trigger the page action,
SELECT cycles the declared model (saved to `tester.cfg`) and START runs every
non-interactive check and writes `test_report.txt`. Destructive calls
(`os.exit`, `os.restart`, `power.suspend/off/lock`, flash writes, FTP server)
are never made automatically; USB, network, camera, OSK, PMF and splash
actions only start on the documented key press. The full reference, safety
notes and current PPSSPP result are in [../docs/TESTER.md](../docs/TESTER.md).

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
| `assets/splash.png` | Current OpenLua introduction, 480x272 | `python3 tools/gen_splash.py` |
| `assets/splash_old.png` | Retro old-splash variant, 480x272 | `python3 tools/gen_splash.py` |
| `assets/splash_oldschool.png` | Original OSLib-style tribute, 480x272 | `python3 tools/gen_splash.py` |
| `assets/sample.pbp` | 27 KB synthetic PBP: PARAM.SFO, 144x80 ICON0, 310x180 PIC0, 480x272 PIC1 | `python3 tools/gen_pbp.py` |

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
- `splash.*` is blocking: each image is shown for about 2 s and a fresh
  Cross/Circle/Start press advances the slideshow. Missing assets are skipped;
  if nothing loads, an original geometric fallback is drawn. Splash restores
  the 2D GE state and the previous clear colour, so the frame loop works
  normally afterwards. `splash.oldschool()` uses an original tribute screen,
  not the historical OSLib artwork; see [../docs/API.md](../docs/API.md) and
  #41.
- `osk.init` is blocking and uses the PSP system keyboard. Confirm returns the
  edited string (possibly `""`); cancel returns `nil`. Arguments are clamped
  as documented; `mode` follows the OneLua language list, and Chinese modes
  fall back to the firmware default because PSPSDK has no Chinese enum. PPSSPP
  1.20.4 has no cancel control (Circle deletes) and always reports
  `RESULT_CHANGED`, so the `nil` branch is host-tested and PSP-only; see
  [../docs/API.md](../docs/API.md) and #42.
- `game` is read-only. `game.info` returns PARAM.SFO fields and
  `geticon0`/`getpic0`/`getpic1` return normal `image` handles; missing,
  non-PBP or malformed files return `nil`. ISO/CSO images are not parsed, and
  `game.pack`/`add`/`remove`/`unpack`/`launch` are intentionally not registered.
  The demo uses the synthetic `assets/sample.pbp`; see
  [../docs/API.md](../docs/API.md) and #40.
- `cam.*` drives the optional GO! CAM USB accessory. Without a camera,
  `init`/`begin` return `0`, getters return `-1`, state/available are false and
  render/toimage do nothing. PPSSPP provides dummy frames, so `demos/cam.lua`
  verifies the active path there; its control getters report `n/a` because
  PPSSPP does not implement that HLE. Call `cam.term()` before switching to
  USB storage mode. See [../docs/API.md](../docs/API.md) and #43.
- `peripherals.lua` only reads state, then activates USB mass storage with
  `usb.mstick()`; it never exposes flash or dumps a disc. `umd.dumpiso` and
  `umd.launch` are not registered. UMD metadata (`gameid`/`info`) needs a game
  disc, so PPSSPP shows `-` for the homebrew EBOOT; UMD present/type and the
  USB flags are verified there. `usb.umd`/`flash*` need Sony's
  `_usbdevice.prx`; the standard build omits that import so the EBOOT boots on
  a normal CFW, and those calls are no-ops. `sio`/`ir` report unavailable
  because raw access needs a kernel driver. USB storage and the camera cannot
  be active together. See [../docs/API.md](../docs/API.md) and #44.
- `network.lua` never starts the FTP server and only attempts refused loopback
  connections, so it is safe without WiFi. `wlan` profiles and access points
  report empty tables, adhoc needs the WLAN switch on, and Bluetooth is always
  unavailable. HTTP is plaintext and synchronous; there is no TLS. If you test
  `ftp.init()` yourself, remember it exposes the working directory anonymously
  on the selected port until `ftp.term()`. See [../docs/API.md](../docs/API.md)
  and #45.
- `storage.lua` only creates `PSP/OPENLUA_COMPAT` for its probe and removes it
  afterwards; savedata uses the game id `OPENLUA1` and deletes it at the end.
  A PSP Go shows `ef0: YES` and both cross-volume copy and move; PPSSPP 1.20.4
  does not emulate `ef0:`, so it shows `ef0: NO` while `app0:`/`ms0:` boot,
  savedata and file operations still pass. See [../docs/DEVELOPMENT.md](../docs/DEVELOPMENT.md)
  and #31.
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
