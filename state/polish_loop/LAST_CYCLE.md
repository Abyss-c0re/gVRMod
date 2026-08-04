# Last cycle

**Cycle:** 62  
**Time:** 2026-08-05T02:28:00+03:00  
**Focus:** G46 realign with b1a5e9e mid-frame restore  
**Commit (gVRMod):** `9367aec`  
**Commit (vrmod-x64):** `dec3dc6` (`cube-stereo-g45`)  
**Tests:** full `./scripts/test_all.sh` — 6/6 pass (pure pending=0; 57 Lua)  

## What changed

1. Realign pure DesktopMirror_* to mid-frame legacy + post-submit ban  
2. Mid-frame snapshot wire; ComputeDesktopCrop NaN guards  
3. submit_bounds unit test accepts unclamped left U under H offset  
4. Document branch policy: product tip = cube-stereo-g45; no force main  
5. Did not undo user b1a5e9e desktop restore  

## Pain points

- Soft care: mid-frame live RT is risk-flagged not hard-blocked (user path).

## Gaps

- G46 partial — HMD walk desktopview 1 vs 2 still open  
- **Attention:** merge cube-stereo-g45 → main needs user OK (main has mq2 mono revert)  

## Notes

- cubalc_mirror left unstaged.
