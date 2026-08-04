# Last cycle

**Cycle:** 45  
**Time:** 2026-08-04T22:40:00+03:00  
**Focus:** G27 engine blacklist never-call law (W2)  
**Commit (gVRMod):** `f02994f`  
**Commit (vrmod-x64):** `d63c6c1`  
**Tests:** `./scripts/test_all.sh --fast` — 4/4 pass  

## What changed

1. **Pure** sh_engine_blacklist_law.lua — blocked W2 names + lifecycle bans; AllowWrite/FilterMap/Decide/HmdExpect  
2. **cl_vrmod** setConvarValue/overrideConvar gate via AllowWrite; EnsurePinned stores `_engineBlacklist*`  
3. PURE_TESTED + unit test util.engine_blacklist_law.never_call_g27  
4. TESTING_FRAMEWORK §0.14 engine walk  
5. Gap G27 partial  

## Pain points

- Reinforces never fighting engine blacklists; lifecycle ban aligns with mat_queue never-write (#3).

## Gaps

- G27 partial — console walk confirm open  
- Next: HMD walk backlog or soft handoff timeout notes  

## Notes

- Left unrelated Quest FOV crop commit (`6aa0b36`) untouched.  
- cubalc_mirror dirty flood left unstaged.
