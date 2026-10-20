#!/usr/bin/env bash
# Build + run the host tests (POSIX backend via olfs, vendored Lua 5.1).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="$ROOT/out"
mkdir -p "$BUILD"

LUASRC="$ROOT/third_party/lua/src"
HOST_CFLAGS=(-O2 -Wall -Wextra -Werror)
LOG="$BUILD/build.log"
: > "$LOG"

run() {
    if ! "$@" >>"$LOG" 2>&1; then
        echo "build failed; compiler log follows:" >&2
        cat "$LOG" >&2
        exit 1
    fi
}

run gcc "${HOST_CFLAGS[@]}" -I"$ROOT" \
    "$ROOT/engine/fs/olfs.c" \
    "$ROOT/test/host/test_olfs.c" \
    -o "$BUILD/olfs_test"
"$BUILD/olfs_test"

run gcc "${HOST_CFLAGS[@]}" -I"$ROOT" \
    "$ROOT/engine/gfx/transform.c" \
    "$ROOT/test/host/test_transform.c" \
    -lm -o "$BUILD/transform_test"
"$BUILD/transform_test"

CORE="lapi lcode ldebug ldo ldump lfunc lgc llex lmem lobject
      lopcodes loslib lparser lstate lstring ltable ltm lvm lzio
      lauxlib lbaselib ldblib liolib lmathlib lstrlib ltablib
      loadlib linit lundump"

OBJS=()
for f in $CORE; do
    run gcc -O2 -I"$LUASRC" -c "$LUASRC/$f.c" -o "$BUILD/$f.o"
    OBJS+=("$BUILD/$f.o")
done

run gcc "${HOST_CFLAGS[@]}" -I"$ROOT/test/host/stub" -I"$LUASRC" -I"$ROOT" \
    "$ROOT/test/host/test_maps.c" "$ROOT/engine/core/olreg.c" \
    "${OBJS[@]}" -lm -o "$BUILD/maps_test"
"$BUILD/maps_test"

run gcc "${HOST_CFLAGS[@]}" -I"$LUASRC" -I"$ROOT" \
    "$ROOT/test/host/test_sound.c" \
    "$ROOT/engine/snd/sound.c" "$ROOT/engine/core/olreg.c" \
    "${OBJS[@]}" \
    -lm -o "$BUILD/sound_test"
"$BUILD/sound_test"

run gcc "${HOST_CFLAGS[@]}" -I"$LUASRC" -I"$ROOT" \
    "$ROOT/test/host/host_main.c" \
    "$ROOT/engine/core/olreg.c" "$ROOT/engine/fs/olfs.c" \
    "$ROOT/engine/fs/files.c" \
    "$ROOT/engine/fs/ini.c" \
    "$ROOT/engine/data/json.c" \
    "$ROOT/engine/snd/sound.c" \
    "$ROOT/engine/sys/savedata.c" \
    "$ROOT/engine/util/timers.c" \
    "${OBJS[@]}" \
    -lm -o "$BUILD/host_test"

rm -rf "$BUILD"/t3work "$BUILD"/PSP "$BUILD"/iniwork "$BUILD"/soundwork "$BUILD"/jsonwork "$BUILD"/data
cd "$BUILD"
./host_test "$ROOT/test/host/test_phase3.lua"
./host_test "$ROOT/test/host/test_timers.lua"
./host_test "$ROOT/test/host/test_ini.lua"
./host_test "$ROOT/test/host/test_json.lua"
./host_test "$ROOT/test/host/test_savedata.lua"
./host_test "$ROOT/test/host/test_sound.lua"
