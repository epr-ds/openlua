# Roadmap

Delivery is tracked on the [OpenLua project board](https://github.com/users/epr-ds/projects/4).
This document explains the phases and maps the remaining work to issues.

## Where we are

| Phase | Scope | State |
|---|---|---|
| **P1: Core Engine** | Lua host, GE pipeline, sprites, input, timers | Implemented; #19/#20 fixed and PPSSPP-verified (PSP hardware pending) |
| **S2: Font + Sound** | PGF, SFont, `sfont`, `screen.print`, WAV/VMA audio, 3D models | Implemented; host-tested and PPSSPP-verified (#25, #37, #38, #39) |
| **S3: Maps + Splash + PMF** | Tilemaps, OS stubs, splash, PMF video | Maps host-tested (#4, #26); splash implemented and PPSSPP-verified (#41); PMF implemented and PPSSPP-verified (#5) |
| **S4: Files + INI + Savedata** | `files`, `ini`, save slots | Implemented; hardening #24 |
| **S5: Network + Adhoc** | WiFi, HTTP, IRC, adhoc | Not started |
| **S6: Testing + Polish** | Boot/verify, renderer, tests, docs | **Current focus** |
| **3.5: Data + Platform** | JSON module, PSP Go support | JSON host-tested (#30); PSP Go paths implemented, verification pending (#31) |
| **OneLua API parity** | Missing module families from the OneLua index | Math (#32), battery (#33), power (#34), OS (#35), hardware (#36), `sfont` (#37), PGF fonts (#38), the 3D modules (#39) and `splash` (#41) complete; peripheral and networking work tracked in #40–#45 |

“Implemented” means the API exists and basic smoke testing passes. It does not
mean every edge case is correct; the linked issues are the source of truth.

## Now — S6: Testing + Polish

| Issue | Work | Area |
|---|---|---|
| #19 | Frame lifecycle + 2D depth fix; PPSSPP-verified | GFX |
| #20 | Transform/blit fix; PPSSPP-verified | GFX |
| #6 | PPSSPP black-screen / silent halt | Core |
| #7 | Verify EBOOT in native PPSSPP | Core |

Definition of done for S6: `EBOOT.PBP` boots reliably, the smoke demo renders
correctly, host tests cover the milestone regressions, and CI is green.

## Next — Phase 3.5: Platform

| Issue | Work | Notes |
|---|---|---|
| #31 | PSP Go compatibility | Internal storage/M2 paths implemented; device/PPSSPP verification pending |

## Backlog by theme

### Storage and data
- #24 savedata root probing and game-id validation

### Platform and tooling
- #6, #7 PSP/PPSSPP boot verification
- #8 Docker build speed
- #29 repository hygiene
- #31 PSP Go verification

### OneLua API parity backlog
The OneLua module index was audited in October 2026. Gaps now have scoped
issues and project-board items; `math` (#32), `battery` (#33), `power` (#34),
`os` (#35), `hardware` (#36), `sfont` (#37), PGF `font` (#38), the 3D
modules (#39) and `splash` (#41) are complete.

- **Font and graphics:** none remaining
- **Applications:** #40 `game`, #42 `osk`, #43 `cam`
- **Peripherals:** #44 `umd`, `usb`, `sio`, `ir`
- **Networking:** #45 `wlan`, `socket`, `http`, `ftp`, `adhoc`

### Not started
- `wlan` / HTTP / IRC / adhoc (S5, #45)
- Archive extraction (`files.extract`, zip/rar)
- VSH save dialog
- MP4 container support (PMF/PSMF playback is implemented)

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

- **Phase 3.5: Data & Platform** — PSP Go
- **Phase 4: Codec (PMF/MP4/Audio) + Archive**
- **Bug Fixes & Polish (Final)** — current S6 bugs
- **Phase 5: Networking (+ WiFi/HTTP)**

## How work is prioritised

1. Boot/render blockers (#6, #19, #20).
2. Safety and data-loss bugs (#22–#26).
3. Device verification (#7).
4. New capabilities (#31, #5).
5. Polish and documentation (#29, #8).
