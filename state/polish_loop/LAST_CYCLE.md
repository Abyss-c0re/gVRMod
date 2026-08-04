# Last cycle

**Cycle:** 20  
**Time:** 2026-08-04T18:00:30+03:00  
**Focus:** G04 warm reuse design (partial — feature hard-off)  
**Commit (gVRMod):** `ed814eb`  
**Commit (vrmod-x64):** none  
**Tests:** `./scripts/test_all.sh` — 6/6 pass  

## What changed

1. **warm_reuse.hpp** pure CubeWarmReuseDecide / Format / Parse / Detail  
2. **CubeWarmReuseEnabled()** hard-off — never skip steam spawn in product  
3. **cube_warm.txt** written when process already up (intent marker)  
4. Panel boot kind **WARM DETECTED · REQUEST FILED**; still cold-spawns  

## Pain points

- Untouched; no risky skip-spawn.

## Gaps

- G04 still **partial** — map-change/attach warm_reuse path open  
- Next: G13 Cube reclaim poll, or ambient asset+player careful  

## Notes

- cubalc_mirror dirty flood left unstaged.
