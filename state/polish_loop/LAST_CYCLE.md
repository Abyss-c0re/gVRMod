# Last cycle

**Cycle:** 23  
**Time:** 2026-08-04T18:34:25+03:00  
**Focus:** G04 map attach design (partial — changelevel hard-off)  
**Commit (gVRMod):** `ac1775d`  
**Commit (vrmod-x64):** `b299125`  
**Tests:** `./scripts/test_all.sh` — 6/6 pass  

## What changed

1. **warm_reuse.hpp** CubeWarmAttach_NormalizeMap / Decide / Toast (pure)  
2. **sh_warm_attach.lua** Parse + Decide + Toast (allow_changelevel=false)  
3. **cl_openxr_launch** noteWarmAttachOnce toast after VR live (no changelevel)  
4. Units: launcher_warm_attach_decide + util.warm_attach.decide_g04  
5. PURE_TESTED map for WarmAttach_*  

## Pain points

- Untouched; no auto changelevel / skip-spawn.

## Gaps

- G04 still **partial** — real skip-spawn + changelevel path open  
- Next: G12 paplay backend careful, or G05 HMD notes  

## Notes

- cubalc_mirror dirty flood left unstaged.
