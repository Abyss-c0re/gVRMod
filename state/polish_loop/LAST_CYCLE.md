# Last cycle

**Cycle:** 9  
**Time:** 2026-08-04T16:01:50+03:00  
**Focus:** G14 Glide stick-primary SoT + seat toast  
**Commit (gVRMod):** `ca30d8c`  
**Commit (vrmod-x64):** `e8318a9`  
**Tests:** `./scripts/test_all.sh --fast` — 4/4 pass  

## What changed

1. **sh_glide_sot.lua** — pure GlideSeatIsDriver + GlidePreferStickSteer  
2. **cl_input** — use helpers; toast “Glide seat — use thumbstick; wheel is optional”  
3. **CUBE_WATCHLIST W3** — HMD smoke steps documented  
4. Unit test + PURE_TESTED map  

## Pain points

- Untouched (no climb/noborder/mat_queue/pose thrash).

## Gaps

- G14 → **partial** (offline SoT/toast; HMD walkthrough still required)  
- Next: G02 fade carefully  

## Notes

- cubalc_mirror dirty flood left unstaged.
