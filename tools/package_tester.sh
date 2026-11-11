#!/usr/bin/env bash
#
# Package the on-device hardware test menu for PSP deployment.
#
# Usage:
#   bash tools/package_tester.sh [OUTPUT_DIR]
#
# Output defaults to out/tester-package.  The staged tree is:
#
#   PSP/GAME/OpenLua/EBOOT.PBP
#   PSP/GAME/OpenLua/script.lua     (copy of demos/tester.lua)
#   PSP/GAME/OpenLua/assets/        (fonts, WAV, 3D, splash, sample PBP)
#   PSP/GAME/OpenLuaProbe/EBOOT.PBP (optional 8002013C boot probe)
#   README.txt                      (deployment and control notes)
#   VERSION                         (OPENLUA_VERSION, or "dev")
#
# Set OPENLUA_VERSION to stamp a build version (the release workflow sets it
# to the published tag).  OPENLUA_EBOOT overrides the EBOOT.PBP path.
# OPENLUA_PROBE_EBOOT overrides the boot-probe EBOOT path (default:
# tools/bootprobe/EBOOT.PBP); when missing, the probe is omitted.
#
# Copy PSP/ to the root of a Memory Stick or the PSP Go internal storage.  The
# script needs EBOOT.PBP in the repository root; build it first with
# `bash docker/build.sh`.
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-$ROOT/out/tester-package}"
EBOOT="${OPENLUA_EBOOT:-$ROOT/EBOOT.PBP}"
PROBE="${OPENLUA_PROBE_EBOOT:-$ROOT/tools/bootprobe/EBOOT.PBP}"
VERSION="${OPENLUA_VERSION:-dev}"
APP="$OUT/PSP/GAME/OpenLua"
PROBE_APP="$OUT/PSP/GAME/OpenLuaProbe"

if [ ! -f "$EBOOT" ]; then
    echo "error: $EBOOT not found" >&2
    echo "       build it first with: bash docker/build.sh" >&2
    exit 1
fi

if [ ! -f "$ROOT/demos/tester.lua" ]; then
    echo "error: demos/tester.lua not found" >&2
    exit 1
fi

rm -rf "$OUT"
mkdir -p "$APP"

cp "$EBOOT" "$APP/EBOOT.PBP"
cp "$ROOT/demos/tester.lua" "$APP/script.lua"
cp -R "$ROOT/assets" "$APP/assets"

if [ -f "$PROBE" ]; then
    mkdir -p "$PROBE_APP"
    cp "$PROBE" "$PROBE_APP/EBOOT.PBP"
fi

# Drop caches and editor leftovers if the assets directory ever gains them.
find "$APP/assets" -name '__pycache__' -type d -prune -exec rm -rf {} +
find "$APP/assets" -name '*.pyc' -delete

cat > "$OUT/README.txt" <<'EOF'
OpenLua hardware test menu
==========================

Deploy
------
Copy the PSP/ directory to the root of a Memory Stick (ms0:) or to the PSP
Go internal storage (ef0:).  The result is:

    PSP/GAME/OpenLua/EBOOT.PBP
    PSP/GAME/OpenLua/script.lua
    PSP/GAME/OpenLua/assets/

Start "OpenLua" from the Game menu.  No installation or flash write is
performed.  This menu is a test build: on a real PSP it can open the OSK,
the camera, USB mass storage, the network and PMF playback, but only after
you press the documented key on the relevant page.  Destructive calls
(os.exit, os.restart, power.suspend/off/lock, flash writes, FTP server) are
never made.

Controls
--------
UP / DOWN     move or scroll
X or O        open the selected test page
O             go back
X             run / re-run the page or trigger its documented action
SELECT        cycle the declared model (Fat / Slim / 3000 / Street / Go)
START         run all non-interactive checks and write test_report.txt

The declared model is stored in tester.cfg next to the EBOOT and is used
because hw.getmodel() can only reliably identify the PSP Go.

Report
------
START writes test_report.txt in the app directory and, where available, to
ms0:/PSP/OPENLUA_TEST/ and ef0:/PSP/OPENLUA_TEST/.  The report lists PASS,
FAIL, INFO and SKIP for every check and ends with totals.  SKIP means the
check needs an interactive key press, missing hardware or a destructive
action; it is not a failure.

Notes
-----
- FAT / Slim / 3000 / Street / Go are user-declared profiles.  hw.getmodel()
  only distinguishes the Go; use SELECT to match the physical unit.
- The standard build does not link sceLibFont.  On real hardware sceLibFont is
  provided by each game's libfont.prx, so font.load() returns nil and the menu
  uses assets/font.png or its built-in 5x7 font instead.  PNG and SFont loaders
  are unaffected.
- USB mass storage and the GO! CAM cannot be active at the same time.
- WLAN tests need the wireless switch on and a configured access point.
- HTTP is plaintext; the PSPSDK build has no TLS.
- A PPSSPP run is emulator verification only.  Run this menu on real
  hardware before claiming hardware support.

Boot probe (8002013C)
---------------------
If the main EBOOT refuses to start with error 8002013C, launch
PSP/GAME/OpenLuaProbe instead.  It is a minimal homebrew that imports only core
libraries and then tries to load the optional modules (net, avcodec, camera,
irda).  It writes openlua_probe.txt next to itself and to ms0:/ and ef0:/.

- Probe runs, main EBOOT does not: the console can run homebrew, but the main
  EBOOT imports a library the firmware cannot resolve.  Send the probe log.
- Probe also fails to start: the issue is the CFW/HEN or the launcher, not the
  OpenLua build.
EOF

printf '%s\n' "$VERSION" > "$OUT/VERSION"
cat >> "$OUT/README.txt" <<EOF

Build version
-------------
$VERSION
EOF

echo "packaged: $OUT"
find "$OUT" -type f -printf '  %P\n' | sort
