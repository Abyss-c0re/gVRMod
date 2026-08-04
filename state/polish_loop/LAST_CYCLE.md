# Last cycle

**Cycle:** 46  
**Time:** 2026-08-04T22:52:00+03:00  
**Focus:** G28 soft handoff timeout law  
**Commit (gVRMod):** `67b2826`  
**Commit (vrmod-x64):** (unchanged)  
**Tests:** `./scripts/test_all.sh` — 6/6 pass  

## What changed

1. **Pure** CubeHandoffTimeout_Decide/HmdExpect — soft 90s (GMod up), hard 180s, refuse racey  
2. **xr_app** release gate uses pure Decide + reason log  
3. Launcher unit test launcher_handoff_timeout_law_g28  
4. TESTING_FRAMEWORK §0.15 handoff timeout walk  
5. Gap G28 partial  

## Pain points

- Soft care reinforced: OpenXR handoff 90s/180s; never racey early release.

## Gaps

- G28 partial — HMD seamless hold walk open  
- Next: HMD walk backlog (G05/G12) or supersample cold-start cap notes  

## Notes

- cubalc_mirror dirty flood left unstaged.
- vrmod-x64 not touched this cycle.
