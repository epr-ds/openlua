#!/usr/bin/env bash
# Build + run the Phase-3 host tests (POSIX backend via olfs, vendored Lua 5.1).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="$ROOT/out"
mkdir -p "$BUILD"

LUASRC="$ROOT/third_party/lua/src"

gcc -O2 -I"$ROOT" \
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
    gcc -O2 -I"$LUASRC" -c "$LUASRC/$f.c" -o "$BUILD/$f.o"
    OBJS+=("$BUILD/$f.o")
done

gcc -O2 -I"$LUASRC" -I"$ROOT" \
    "$ROOT/test/host/host_main.c" \
    "$ROOT/engine/core/olreg.c" "$ROOT/engine/fs/olfs.c" \
    "$ROOT/engine/fs/files.c" \
    "$ROOT/engine/fs/ini.c" \
    "$ROOT/engine/sys/savedata.c" \
    "${OBJS[@]}" \
    -lm -o "$BUILD/host_test"

rm -rf "$BUILD"/t3work "$BUILD"/PSP; cd "$BUILD"
exec ./host_test "$ROOT/test/host/test_phase3.lua"
