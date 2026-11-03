#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

# assets/sample.pbp is a generated test fixture (tools/gen_pbp.py), not a build
# artifact, so it is the one tracked .pbp file.
tracked="$(git ls-files | grep -E '(\.o|\.o_elf|\.elf|\.pbp|\.AppImage)$|(^|/)EBOOT\.PBP$|(^|/)PARAM\.SFO$' | grep -v '^assets/sample\.pbp$' || true)"

if [ -n "$tracked" ]; then
    printf 'error: build artifacts are tracked:\n%s\n' "$tracked" >&2
    exit 1
fi

echo "repo hygiene: no tracked build artifacts"
