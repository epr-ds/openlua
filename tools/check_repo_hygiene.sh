#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

tracked="$(git ls-files | grep -E '(\.o|\.o_elf|\.elf|\.pbp|\.AppImage)$|(^|/)EBOOT\.PBP$|(^|/)PARAM\.SFO$' || true)"

if [ -n "$tracked" ]; then
    printf 'error: build artifacts are tracked:\n%s\n' "$tracked" >&2
    exit 1
fi

echo "repo hygiene: no tracked build artifacts"
