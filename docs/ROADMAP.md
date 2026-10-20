# Roadmap

Delivery is tracked on the [OpenLua project board](https://github.com/users/epr-ds/projects/4).
This document explains the phases and maps the remaining work to issues.

## Where we are

| Phase | Scope | State |
|---|---|---|
| **P1: Core Engine** | Lua host, GE pipeline, sprites, input, timers | Implemented; renderer bugs #19, #20 |
| **S2: Font + Sound** | SFont, `screen.print`, WAV/VMA audio | Implemented; sound bug #25, API polish #27 |
| **S3: Maps + Splash + PMF** | Tilemaps, OS stubs, PMF video | Maps done (#4, #26); PMF pending (#5) |
| **S4: Files + INI + Savedata** | `files`, `ini`, save slots | Implemented; hardening #22–#24 |
| **S5: Network + Adhoc** | WiFi, HTTP, IRC, adhoc | Not started |
| **S6: Testing + Polish** | Boot/verify, renderer, tests, docs | **Current focus** |
| **3.5: Data + Platform** | JSON module, PSP Go support | Planned (#30, #31) |

“Implemented” means the API exists and basic smoke testing passes. It does not
mean every edge case is correct; the linked issues are the source of truth.

## Now — S6: Testing + Polish

| Issue | Work | Area |
|---|---|---|
| #19 | GE pipeline state corruption / frame lifecycle | GFX |
| #20 | Scaled, tinted and rotated blit correctness + leak | GFX |
| #6 | PPSSPP black-screen / silent halt | Core |
| #7 | Verify EBOOT in native PPSSPP | Core |
| #28 | Expand host tests and add CI | Tooling |
| #29 | Remove tracked binaries, add LICENSE, refresh docs/demos | Tooling |

Definition of done for S6: `EBOOT.PBP` boots reliably, the smoke demo renders
correctly, host tests cover the storage and timer fixes, and CI is green.

## Next — Phase 3.5: Data + Platform

| Issue | Work | Notes |
|---|---|---|
| #30 | JSON module | `json.decode/encode/load/save`, host-testable |
| #31 | PSP Go compatibility | Internal storage, M2, boot and savedata paths |

## Backlog by theme

### Renderer and assets
- #19 frame clear/swap/depth ordering
- #20 scaled/tinted/rotated blits and buffer lifetime
- #27 image userdata methods + naming consistency

### Audio
- #25 explicit channel reservation, short-file parsing
- #5 PMF/MP4 decode and playback

### Storage and data
- #22 portable FS hardening (`olfs_cwd`, listing)
- #23 INI middle-section insertion
- #24 savedata root probing and game-id validation
- #26 map tileset lifetime and finalizer
- #30 JSON module

### Platform and tooling
- #6, #7 PSP/PPSSPP boot verification
- #8 Docker build speed
- #28 tests + CI
- #29 repository hygiene
- #31 PSP Go support

### Not started
- `wlan` / HTTP / IRC / adhoc (S5)
- Archive extraction (`files.extract`, zip/rar)
- VSH save dialog
- PMF/MP4 pipeline

## Project fields

| Field | Values | Meaning |
|---|---|---|
| Status | Todo / In Progress / Done | Board columns |
| Phase | P1 … S6 | Scope |
| Sprint | S1 … S6 | Capacity |
| Track | Core / GFX / Audio / NWK / TUI | Discipline |
| Points | 2 / 3 / 5 | Fibonacci estimate |

Priority is tracked with `priority:high`, `priority:medium`, `priority:low`
labels. The board README lists the provisioned views (`Board by Status`,
`Remaining Work`, `Done`, `By Phase`, `By Track`, `Priority Triage`,
`Sprint Plan`).

## Milestones

- **Phase 3.5: Data & Platform** — JSON, PSP Go
- **Phase 4: Codec (PMF/MP4/Audio) + Archive**
- **Bug Fixes & Polish (Final)** — current S6 bugs
- **Phase 5: Networking (+ WiFi/HTTP)**

## How work is prioritised

1. Boot/render blockers (#6, #19, #20).
2. Safety and data-loss bugs (#22–#26).
3. Verification and CI (#7, #28).
4. New capabilities (#30, #31, #5).
5. Polish and documentation (#27, #29, #8).
