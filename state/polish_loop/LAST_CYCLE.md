# Last cycle

**Cycle:** 4  
**Time:** 2026-08-04T15:05:48+03:00  
**Focus:** G11 Quick Play last map + gfx  
**Commit (gVRMod):** `5408886`  
**Commit (vrmod-x64):** none  
**Tests:** `./scripts/test_all.sh` — 6/6 pass (+2 last_play unit tests); launcher builds  

## What changed

1. **`last_play.hpp`** — pure Format/Parse for map + server + gfx + XR SS snapshot  
2. **Save** on successful StartGame → `garrysmod/data/vrmod/cube_last_play.txt`  
3. **Load** on WebUI_Init — restore selection + gfx  
4. **QUICK PLAY** button on New Game when snapshot exists  
5. Offline tests for round-trip + reject-empty  

## Pain points

- Untouched: climbing, -noborder (snapshot may store user noborder=0 default), mat_queue, dual pose, HUD, force-push.

## Gaps

- G11 → **done**  
- Next: G23 follow-cam call sites  

## Notes

- cubalc_mirror dirty flood left unstaged.
