# Last cycle

**Cycle:** 27  
**Time:** 2026-08-04T19:18:23+03:00  
**Focus:** G04 skip-spawn plan (env opt-in)  
**Commit (gVRMod):** `118120f`  
**Commit (vrmod-x64):** none  
**Tests:** `./scripts/test_all.sh` — 6/6 pass  

## What changed

1. **CubeWarmSkipSpawnPlanDecide** pure skip + markers/stage flags  
2. **GVRMOD_WARM_REUSE** env opt-in (default off)  
3. **WriteWarmAttachMarkers** openxr_launch + handoff phase=warm_attach  
4. **xr_app** skip branch files markers + stage pack (no Steam)  
5. Handoff labels for warm_attach / warm_wait_map / warm_ready  
6. Unit: launcher_warm_skip_spawn_plan  

## Pain points

- Untouched; no force skip without env.

## Gaps

- G04 still **partial** — GMod changelevel on map mismatch open  
- Next: ambient default-on or G03 executor careful  

## Notes

- cubalc_mirror dirty flood left unstaged.
