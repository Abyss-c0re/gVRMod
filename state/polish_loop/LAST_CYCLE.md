# Last cycle

**Cycle:** 28  
**Time:** 2026-08-04T19:28:24+03:00  
**Focus:** G12 ambient default-on (careful)  
**Commit (gVRMod):** `027a008`  
**Commit (vrmod-x64):** none  
**Tests:** `./scripts/test_all.sh` — 6/6 pass  

## What changed

1. **CubeAmbientPlayerEnabledFromEnv** default ON; opt-out 0/false/off  
2. **CubeAmbient_ComfortMaster** 0.55 soft volume  
3. PlayerDecide applies comfort master to volume  
4. TESTING_FRAMEWORK ambient checklist default-on  
5. Unit: env policy + comfort volume  

## Pain points

- Untouched.

## Gaps

- G12 nearly done offline; HMD volume taste open  
- Next: G03 plan executor careful or G13 reclaim  

## Notes

- cubalc_mirror dirty flood left unstaged.
