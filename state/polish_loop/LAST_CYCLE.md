# Last cycle

**Cycle:** 54  
**Time:** 2026-08-05T00:16:00+03:00  
**Focus:** G36 FOV/Z soft-refresh law (W5)  
**Commit (gVRMod):** (pending)  
**Commit (vrmod-x64):** (pending)  
**Tests:** `./scripts/test_all.sh --fast` — 4/4 pass (pure pending=0; 48 Lua)  

## What changed

1. **Pure** FovZLaw_RefreshKind/ClampFov/ClampZnear/Decide/HmdExpect  
2. **cl_vrmod** BindBorder/BindRenderProfile + FOV clamp in ComputeDisplayParams  
3. Unit test util.fovz_law.soft_refresh_g36 + PURE_TESTED  
4. TESTING_FRAMEWORK §0.23 FOV/Z walk  
5. Gap G36 partial  

## Pain points

- Soft care: no mid-frame UV+FOV fight; prefer Border guide over Z spam.

## Gaps

- G36 partial — HMD one-eye jitter walk open  
- Next: HMD walk backlog (G05/G12) or W9 hand bullet filter notes  

## Notes

- cubalc_mirror dirty flood left unstaged.
