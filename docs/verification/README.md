# PPSSPP verification

These captures were produced with PPSSPP v1.20.4 using the **software**
renderer on a headless Xvfb display. No PSP hardware was available, so this is
emulator verification only.

The EBOOT was built with `bash docker/build.sh` and the test script was copied
next to it as `script.lua` (with `assets/` alongside). Each image is downscaled
from the 960x544 PPSSPP window to the native 480x272 PSP resolution. The
`Log to file` banner is PPSSPP's own OSD; it is not part of the game.

| Capture | What it shows | Source |
|---|---|---|
| ![smoke](smoke.png) | Dark background, green fill, white outline and red line | `demos/smoke.lua` |
| ![transforms](transforms.png) | Triangle at 0/30/60/90 degrees, 45-degree scaled + tinted triangle, 2x circle, vertically flipped triangle | `transform_check.lua` |
| ![flip](flip.png) | Normal, horizontal, vertical and combined flips; explicit source rects and whole-sheet blit | `demos/flip.lua` |
| ![depth](depth.png) | Red (z=10) drawn before green (z=5); red remains on top in the overlap. Blue (z=20) is separate | `depth_check.lua` |
| ![battery](battery.png) | `batt` telemetry: 100%, ON BATTERY, 0:00 remaining, 0 C; PPSSPP reports no voltage/capacity, so those fields show `-` | `demos/battery.lua` |
| ![power](power.png) | `power` demo: PLUGGED YES, TICK YES; PPSSPP 1.20.4 does not implement lock/unlock, so those show NO with HLE N/A; suspend/off are marked NOT RUN | `demos/power.lua` |
| ![os](os.png) | `os` demo: PPSSPP nickname/language/MAC, RTC date, 222/111 MHz clocks and free/total memory; PPSSPP reports no flash0 version or MS/ef0 capacity, so those show N/A; CFW is UNKNOWN | `demos/os.lua` |
| ![hardware](hardware.png) | `hw` demo: EMULATOR, firmware 6.60, region `--`, session volume 27/mute OFF, no headphone; board/gen/NAND and LED show documented kernel-only fallbacks; audio route reports mode 0 because of PPSSPP's `GetMode` HLE | `demos/hardware.lua` |

Before these fixes the smoke test showed only the clear colour, sprites sampled
the transparent corner of the sheet, and window capture of the engine's GUI was
garbage. See the issue comments for [#19] and [#20].

[#19]: https://github.com/epr-ds/openlua/issues/19
[#20]: https://github.com/epr-ds/openlua/issues/20
[#36]: https://github.com/epr-ds/openlua/issues/36
