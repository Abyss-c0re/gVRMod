# Last cycle

**Cycle:** 19  
**Time:** 2026-08-04T17:49:44+03:00  
**Focus:** G12 ambient clip contract (partial — no OpenAL player)  
**Commit (gVRMod):** (pending close)  
**Commit (vrmod-x64):** none  
**Tests:** `./scripts/test_all.sh` — 6/6 pass  

## What changed

1. **ambient_clip.hpp** pure ShouldPlay / Format / Parse / Resolve / StatusLabel  
2. **WriteCubeAmbientStatus** → `cube_ambient.txt` during handoff (throttled)  
3. Panel AUDIO line uses clip contract labels  
4. Default clip path `ambient/cube_hold.ogg` (asset still optional — no forced playback)

## Pain points

- Untouched.

## Gaps

- G12 still **partial** — player/OpenAL + real ogg asset open  
- Next: G04 warm reuse design, or drop real ambient asset + soft player  

## Notes

- cubalc_mirror dirty flood left unstaged.
