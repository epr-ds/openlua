#!/usr/bin/env bash
set -euo pipefail

BASE_IMAGE="${PSPDEV_IMAGE:-pspdev/pspdev:latest}"
BUILDER_IMAGE="${OPENLUA_BUILDER_IMAGE:-openlua-pspdev:local}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CACHE="$ROOT/docker/.cache/ccache"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"

mkdir -p "$CACHE"

docker build \
    --build-arg "PSPDEV_IMAGE=$BASE_IMAGE" \
    -t "$BUILDER_IMAGE" \
    "$ROOT/docker"

RUN_ARGS=(
    --rm
    -e HOME=/tmp
    -e CCACHE_DIR=/ccache
    -e CCACHE_BASEDIR=/build
    -v "$ROOT":/build
    -v "$CACHE":/ccache
    -w /build
)

if [ "$(id -u)" -ne 0 ]; then
    RUN_ARGS+=(--user "$(id -u):$(id -g)")
fi

docker run "${RUN_ARGS[@]}" "$BUILDER_IMAGE" \
    make -j"$JOBS" CC="ccache psp-gcc" "$@"
