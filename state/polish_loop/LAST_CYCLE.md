# Last cycle

**Cycle:** 31  
**Time:** 2026-08-04T20:03:17+03:00  
**Focus:** G04 careful changelevel plan executor  
**Commit (gVRMod):** `65e66a9`  
**Commit (vrmod-x64):** `c3fefcd`  
**Tests:** `./scripts/test_all.sh` — 6/6 pass  

## What changed

1. **Pure Lua** WarmAttach_MapTokenOk / AllowChangelevelFromFlags / ChangelevelPlan / Cmd / ShouldExecute / ExecuteChangelevel / ExecuteToast  
2. **Pure C++** CubeWarmChangelevel* plan + token gate + allow flags (env GVRMOD_WARM_CHANGELEVEL)  
3. **openxr_launch** opt-in: `vrmod_warm_changelevel` or `warm_changelevel_enable.txt`; RCC only when armed  
4. Unit + launcher tests; gen_contracts PURE_TESTED for new symbols  

## Pain points

- Untouched; default still no auto changelevel.

## Gaps

- G04 partial — HMD-proven warm reuse + changelevel still open  
- Next: G05 HMD load-flash notes or G12 HMD volume taste  

## Notes

- cubalc_mirror dirty flood left unstaged.
