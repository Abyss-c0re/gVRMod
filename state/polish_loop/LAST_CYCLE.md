# Last cycle

**Cycle:** 44  
**Time:** 2026-08-04T22:28:00+03:00  
**Focus:** G26 menu thrash / QM dedupe law  
**Commit (gVRMod):** `74057ce`  
**Commit (vrmod-x64):** `95edf81`  
**Tests:** `./scripts/test_all.sh --fast` — 4/4 pass  

## What changed

1. **Pure** sh_menu_law.lua — NormalizeName/StableKey/ItemsMatch/climb aliases/DedupList/Decide/HmdExpect  
2. **cl_api** wires MenuLaw for Match/StableKey; DedupInGameMenuItems stores `_menuLaw*`  
3. PURE_TESTED + unit test util.menu_law.dedupe_g26  
4. TESTING_FRAMEWORK §0.13 menu walk  
5. Gap G26 partial  

## Pain points

- Reinforces #9: QM VRClimb menu dupes — dedupe by id/name.

## Gaps

- G26 partial — HMD one-tile walk open  
- Next: engine blacklist never-call law (W2) or HMD walk backlog  

## Notes

- cubalc_mirror dirty flood left unstaged.
