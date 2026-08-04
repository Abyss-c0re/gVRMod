# Last cycle

**Cycle:** 1  
**Time:** 2026-08-04T14:41:00+03:00  
**Focus:** G01 handoff progress phases on Cube panel  
**Commit (gVRMod):** `c53f0e7`  
**Commit (vrmod-x64):** `b1ada40`  
**Tests:** `./scripts/test_all.sh` — 6/6 suites pass (incl. 3 new launcher handoff unit tests)  

## What changed

1. **Pure helpers** in `native_launcher/src/gmod_spawn.hpp`:
   - `CubeHandoffDetailForPhase` — status-file phase → human detail copy
   - `CubeHandoffProgressForPhase` — known phases → 0..1 bar fraction
   - `CubeHandoffPhaseLabel` — PHASE line display labels
2. **`xr_app.cpp`** — uses phase-aware detail instead of binary “holding XR / process up”.
3. **`ui_panel.cpp`** — friendly PHASE label + phase-based progress bar (time fallback when unknown).
4. **Lua** `cl_openxr_launch.lua` — writes `map_ready` on InitPostEntity for launch sessions.
5. **Launcher tests** — offline coverage for detail / progress monotone / labels.

## Pain points

- Untouched: climbing, -noborder, mat_queue_mode, dual pose, HUD, force-push.
- Handoff soft/timeout 90s/180s unchanged.

## Gaps

- G01 → **done** (phase surface intentional; remaining feel is G02 fade / G03 cal).
- Next focus: G10 first-run gates re-spam (safe Lua) or G20 pure utils if G10 blocked.

## Notes

- cubalc_mirror dirty flood left unstaged.
- Submodule vrmod-x64 advanced to `b1ada40`.
