#!/usr/bin/env bash
set -euo pipefail

IMAGE="${PSPDEV_IMAGE:-pspdev/pspdev:latest}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

docker run --rm \
    -v "$ROOT":/build \
    -w /build \
    "$IMAGE" \
    make -j2 "$@"
