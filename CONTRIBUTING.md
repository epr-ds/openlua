# Contributing to OpenLua

Thanks for helping. OpenLua targets the OneLua API on PSP, with a portable
storage layer and host tests.

## Before you start

1. Read [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) and
   [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
2. Check the [project board](https://github.com/users/epr-ds/projects/4) and
   existing issues. Work that is already tracked should be claimed there.
3. For a new bug or feature, open an issue first (or ask on an existing one).

## Setup

```bash
git clone https://github.com/epr-ds/openlua.git
cd openlua

# Host tests (no PSP toolchain needed)
bash test/host/run.sh

# Check that no build artifacts are tracked
bash tools/check_repo_hygiene.sh

# PSP build (Docker)
bash docker/build.sh
```

## Workflow

- Create a topic branch from `main`:
  `git checkout -b fix/timer-elapsed` or `feat/json-module`.
- Keep commits focused and imperative, for example:
  `fix(timers): return non-negative elapsed milliseconds`.
- Do not commit generated or tracked build artifacts (`*.o`, `*.elf`,
  `EBOOT.PBP`, `out/`, `*.AppImage`). See #29 for the existing cleanup.
- Do not mix refactors with behaviour changes in one commit.
- Update docs and tests in the same change as the code.

## Project board fields

When you open an issue or PR, set:

| Field | Guidance |
|---|---|
| Status | Todo → In Progress → Done |
| Phase | P1 / S2 / S3 / S4 / S5 / S6 |
| Sprint | The sprint you are working in |
| Track | Core / GFX / Audio / NWK / TUI |
| Points | 2 (small), 3 (medium), 5 (large) |
| Priority | `priority:high`, `priority:medium`, `priority:low` label |

Link the issue in the PR description so the board shows progress.

## Pull request checklist

- [ ] The problem and approach are explained.
- [ ] Host tests pass (`bash test/host/run.sh`).
- [ ] Repository hygiene check passes (`bash tools/check_repo_hygiene.sh`).
- [ ] PSP build succeeds if PSP code changed.
- [ ] New/changed public API is documented in `docs/API.md`.
- [ ] Demos still run or are updated.
- [ ] No new tracked binaries or secrets.
- [ ] Related issue is linked (`Closes #NN`).

## Definition of done

- Acceptance criteria in the issue are met.
- Tests cover the fix where practical.
- A PSP/PPSSPP manual check is recorded for rendering, audio or boot changes.
- Documentation matches the implementation.
- The issue is closed with a short verification note.

## Reporting bugs

Include:

- PSP model/firmware or PPSSPP version;
- the Lua script (minimal reproduction);
- `openlua.log` and the `flash0:/openlua_boot.txt` trace if boot fails;
- whether the bug reproduces on host tests.

Rendering bugs should state which of #19/#20 they may be related to.

## Coding style

Match the surrounding C: C99, 4-space indentation, no unrelated reformatting.
Keep PSP-specific code behind `#if defined(__PSP__)`. Pixel buffers are ABGR
and must use the `realW` stride.
