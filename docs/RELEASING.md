# Releasing

Releases are built and published by GitHub Actions. The tag is the source of
truth for the version; there is no checked-in version file to edit.

## What runs

On every push to `main`, the [`Build`](../.github/workflows/build.yml)
workflow:

1. runs the full host suite and repository hygiene check,
2. builds `EBOOT.PBP` and the optional `8002013C` boot probe in the PSPSDK container,
3. resolves the next version, and
4. publishes a GitHub Release with:

| Asset | Contents |
|---|---|
| `openlua-tester-<version>.zip` | `PSP/GAME/OpenLua/{EBOOT.PBP,script.lua,assets/}`, `PSP/GAME/OpenLuaProbe/EBOOT.PBP`, `README.txt`, `VERSION` |
| `openlua-<version>.EBOOT.PBP` | standalone tester EBOOT |
| `openlua-probe-<version>.EBOOT.PBP` | standalone minimal boot probe |
| `SHA256SUMS.txt` | checksums for the files above |

The release is skipped for pull requests and non-`main` branches, and it only
runs after the host tests and PSP build succeed. GitHub also attaches the
automatic source archives.

The release tag and the package `VERSION` file identify the project build.
The `version 1.0` line at the top of `test_report.txt` is the tester's report
format version, not the release version.

## Version scheme

Versions are `vMAJOR.MINOR.PATCH`:

- With no previous tag, the first release is `v0.1.0`.
- Otherwise each push to `main` increments `PATCH` (`v0.1.0` → `v0.1.1`).
- Use a manual dispatch to choose `MINOR`, `MAJOR` or a full version.

## Automatic patch release

Merge or push to `main`; no extra step is needed. The workflow creates the tag
and release itself with the repository `GITHUB_TOKEN`, so no personal access
token is required.

## Manual release

Use **Actions → Build → Run workflow** and enter one of:

| Input | Result |
|---|---|
| `version` empty | bumps `PATCH` like a normal main push |
| `version: 0.2.0` | releases `v0.2.0` |
| `version: 1.0.0` | releases `v1.0.0` |
| `version: 0.3.0-rc.1` | releases `v0.3.0-rc.1` |
| `prerelease: true` | marks the GitHub Release as a pre-release |

The dispatch must run on `main`; the release job checks the ref before
publishing.

```bash
# Same thing from the command line
gh workflow run Build -f version=0.2.0
gh run watch
```

## Re-running

The release job is concurrency-limited to one `main` release at a time. If the
resolved tag already exists, it reuses the tag and re-uploads the assets with
`--clobber` instead of failing. Re-running the workflow on the same commit
therefore refreshes the assets of the newest tag rather than creating a
duplicate version.

## Local equivalent

To reproduce the release zip locally:

```bash
bash docker/build.sh all probe
OPENLUA_VERSION=v0.1.0 bash tools/package_tester.sh
cd out/tester-package && zip -r ../openlua-tester-v0.1.0.zip PSP README.txt VERSION
```

`OPENLUA_VERSION` defaults to `dev` for local packaging. `OPENLUA_PROBE_EBOOT`
can point at a probe built elsewhere; if `tools/bootprobe/EBOOT.PBP` is absent,
the probe is simply omitted from the package.

## Verification scope

A release means the host suite and PSP build passed. It does **not** mean the
tester was run on real hardware. The release notes and
[TESTER.md](TESTER.md) state that verification is PPSSPP-only until real
PSP-1000/2000/3000/E1000/Go reports are collected. Attach `test_report.txt`
from a real unit when opening a hardware result issue.
