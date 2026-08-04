# Last cycle

**Cycle:** 53  
**Time:** 2026-08-05T00:05:00+03:00  
**Focus:** G35 viewscale fisheye law (W8)  
**Commit (gVRMod):** `81e19df`  
**Commit (vrmod-x64):** `c126533`  
**Tests:** `./scripts/test_all.sh` — 6/6 pass (launcher 32; 47 Lua)  

## What changed

1. **Pure Lua** ViewScaleLaw_Clamp/IsFisheyeRisk/Decide/HmdExpect  
2. **Pure launcher** CubeViewScale_* + gmod_spawn/launch_fill clamp  
3. **cl_vrmod** ComputeDisplayParams uses clamp + snapshot  
4. Unit tests Lua + launcher; PURE_TESTED; §0.22  
5. Gap G35 partial  
6. Pushed stereo tilt settings `423a178` (user; tests green)  

## Pain points

- Soft care: fisheye via extreme viewscale; prefer Vision defaults / 1.0.

## Gaps

- G35 partial — HMD fisheye walk open  
- Next: HMD walk backlog (G05/G12) or W5 FOV/Z jitter notes  

## Notes

- cubalc_mirror dirty flood left unstaged.
