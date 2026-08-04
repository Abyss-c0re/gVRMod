# Last cycle

**Cycle:** 5  
**Time:** 2026-08-04T15:16:02+03:00  
**Focus:** G23 desktopview=4 follow-cam call sites  
**Commit (gVRMod):** `d10d4b2`  
**Commit (vrmod-x64):** `8dc402c`  
**Tests:** `./scripts/test_all.sh` — 6/6 pass  

## What changed

1. **cl_vrmod** — mode 4 never falls into stereo eye crop; uses IsFollowMode/IsEyeCropMode  
2. **cl_desktop_cam** — Clamp/Cycle/Label/IsEyeCropMode pure helpers  
3. **ComputeDesktopCrop** — mode 1 and 4 return unused 0,0  
4. **Launcher** — RIGHT label explicit; last_play clamps desktopview  
5. **docs/DESKTOP_BROADCAST.md** — call-site law table  

## Pain points

- Untouched: climbing, -noborder, mat_queue, dual pose, HUD, force-push.

## Gaps

- G23 → **done**  
- Next: G20 residual color parse or G21 contracts  

## Notes

- cubalc_mirror dirty flood left unstaged.
