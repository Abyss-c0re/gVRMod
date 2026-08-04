# Last cycle

**Cycle:** 37  
**Time:** 2026-08-04T21:08:28+03:00  
**Focus:** G04 warm HmdExpect + §0.6 smoke  
**Commit (gVRMod):** `13ffbf1`  
**Commit (vrmod-x64):** (set after push)  
**Tests:** `./scripts/test_all.sh` — 6/6 pass  

## What changed

1. **Pure Lua** WarmAttach_HmdExpect  
2. **Pure C++** CubeWarm_HmdExpect  
3. **openxr_launch** logs G04 HMD checklist  
4. TESTING_FRAMEWORK §0.6 warm walk  

## Pain points

- Untouched; default still cold + no auto changelevel.

## Gaps

- G04 partial — HMD warm walk still open  
- Next: HMD walk backlog (G05/G12/G03) or HUD additive law notes  

## Notes

- cubalc_mirror dirty flood left unstaged.
