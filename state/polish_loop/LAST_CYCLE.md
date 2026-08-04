# Last cycle

**Cycle:** 21  
**Time:** 2026-08-04T18:12:00+03:00  
**Focus:** G13 Cube reclaim poll (partial — feature hard-off)  
**Commit (gVRMod):** `ed05fff`  
**Commit (vrmod-x64):** none  
**Tests:** `./scripts/test_all.sh` — 6/6 pass  

## What changed

1. **cube_return.hpp** pure Format/Parse + CubeReclaimDecide / PanelLabel / Detail  
2. **CubeReclaimEnabled()** hard-off — never auto reverse reclaim XR  
3. **ReadCubeReturnMarker** I/O; labels moved from gmod_spawn into pure header  
4. **xr_app** ~1 Hz poll when not in Start handoff; panel RETURN banner on New Game  
5. Unit: `launcher_cube_return_reclaim_poll`  

## Pain points

- Untouched; no auto reclaim, no climb/border/mq thrash.

## Gaps

- G13 still **partial** — auto reclaim path empty even if feature flipped  
- Next: G12 ambient asset+player careful, or G04 map attach  

## Notes

- cubalc_mirror dirty flood left unstaged.
