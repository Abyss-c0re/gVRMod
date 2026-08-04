# Last cycle

**Cycle:** 18  
**Time:** 2026-08-04T17:38:00+03:00  
**Focus:** G13 reverse handoff protocol (partial — no auto reclaim)  
**Commit (gVRMod):** (pending close)  
**Commit (vrmod-x64):** `b1dc55f`  
**Tests:** `./scripts/test_all.sh` — 6/6 pass  

## What changed

1. **sh_cube_return.lua** pure Format/Parse/PhaseLabel/ShouldNotifyCube/Detail  
2. **VRUtilClientExit** writes `cube_return.txt` (vr_exit → xr_released) for Cube sessions  
3. **CubeReverse*** pure labels in launcher; clear return marker on Start  
4. Toast: relaunch Cube shell — **no** auto OpenXR reclaim  

## Pain points

- Untouched; soft handoff timeouts unchanged.

## Gaps

- G13 → **partial** (protocol + marker; Cube reclaim process open)  
- Next: G12 ambient clip, or G04 warm reuse, or G13 Cube poll reclaim  

## Notes

- cubalc_mirror dirty flood left unstaged.
