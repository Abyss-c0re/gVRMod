# Last cycle

**Cycle:** 40  
**Time:** 2026-08-04T21:41:08+03:00  
**Focus:** G17 mat_queue pin law  
**Commit (gVRMod):** `593f34e`  
**Commit (vrmod-x64):** (set after push)  
**Tests:** `./scripts/test_all.sh --fast` — 4/4 pass  

## What changed

1. **Pure** sh_mat_queue_law.lua pin/clamp/never-write/dual_ok/HmdExpect  
2. **cl_vrmod** WantedMatQueueMode uses ClampRead + stores decision  
3. TESTING_FRAMEWORK §0.9 mq walk  
4. Gap G17 partial  

## Pain points

- Reinforces #3: never optimize to mat_queue_mode 2; VR never writes mq.

## Gaps

- G17 partial — console/HMD confirm open  
- Next: HMD walk backlog (G05/G12) or framed window / -noborder law notes  

## Notes

- cubalc_mirror dirty flood left unstaged.
