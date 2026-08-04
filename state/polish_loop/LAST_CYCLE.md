# Last cycle

**Cycle:** 47  
**Time:** 2026-08-04T23:04:00+03:00  
**Focus:** G29 supersample cold-start cap law  
**Commit (gVRMod):** `c5e593c`  
**Commit (vrmod-x64):** (unchanged)  
**Tests:** `./scripts/test_all.sh` — 6/6 pass  

## What changed

1. **Pure** CubeSs_ColdStartCap/ClampColdStart/LadderFromIdx/Decide/HmdExpect  
2. **gmod_spawn** cfg uses ClampColdStart; **launch_fill** uses ladder helpers  
3. Launcher unit test launcher_supersample_cold_cap_g29  
4. TESTING_FRAMEWORK §0.16 SS walk  
5. Gap G29 partial  

## Pain points

- Soft care: supersample at Start — cap for bring-up; don’t crank SS in cold start.

## Gaps

- G29 partial — HMD hitch walk open  
- Next: HMD walk backlog (G05/G12) or FOV archive write-only-when-touched notes  

## Notes

- cubalc_mirror dirty flood left unstaged.
- vrmod-x64 not touched this cycle.
