# glogic gaps & polish backlog

Living backlog. Agents **promote / demote / mark done** each cycle. Newest insights at top of each section.

Last updated: 2026-08-04 cycle 1

## P0 — seamless / feel broken

| ID | Gap | Notes | Status |
|----|-----|-------|--------|
| G01 | Handoff progress opaque | Cube panel says holding XR; no map/engine phases | **done** cycle1 |
| G02 | No coordinated fade on take_xr | Compositor cut visible | open |
| G03 | Cal / STAGE not packed into handoff | Height/playspace can jump after claim | open |
| G04 | Cold Steam/hl2 every Start | Long Facepunch gap | open (warm process is larger) |
| G05 | Loading after take_xr may not be stereo | Black or flat load flash | open |

## P1 — Cube experience polish

| ID | Gap | Notes | Status |
|----|-----|-------|--------|
| G10 | First-run gates re-spam on wrapper autostart | Should skip if cal exists + native_wrapper | open |
| G11 | Quick Play (last map + gfx) missing | Reduces menu friction | open |
| G12 | Audio dead during handoff | Optional ambient crossfade | open |
| G13 | Return-to-Cube reverse handoff | Exit VR → Cube reclaims XR | open |
| G14 | Glide vehicle input SoT | Watchlist W3; partial | open |

## P2 — code quality / glogic hygiene

| ID | Gap | Notes | Status |
|----|-----|-------|--------|
| G20 | Pure utils not fully rewired at call sites | laser/beam/finger → sh_math helpers | open |
| G21 | Contract inventory lag for new symbols | gen_contracts after new vrmod.* | open |
| G22 | VERSION drift in cubalc mirror | informational; don’t “fix” upstream VERSION | n/a |
| G23 | Desktop follow-cam call sites incomplete | follow cam landed; verify all desktopview=4 paths | open |
| G24 | Offline tests green but no HMD smoke automation | document only | open |

## P3 — watchlist / workshop

See `addon/vrmod-x64/docs/CUBE_WATCHLIST.md` (W1–W12). Prefer smoke docs and tiny toasts over rewrites.

## Recently completed (keep short)

| ID | What | Commit / note |
|----|------|----------------|
| G01 | Phase-aware handoff panel + map_ready | launcher helpers + vrmod-x64 b1ada40 |
| — | No forced -noborder | 70ea961 / 1dbb1b5 |
| — | quest media find patterns | b79d72c |
| — | desktop follow cam + broadcast | vrmod-x64 1beb0cf |

## Cycle pick rule

Each cycle: pick **one** open P0 if safe, else one P1/P2 with **small reversible diff**.  
If blocked by pain points → only docs/tests/gap inventory + journal update (no fake commit).
