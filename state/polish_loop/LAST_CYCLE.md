# Last cycle

**Cycle:** 22  
**Time:** 2026-08-04T18:23:39+03:00  
**Focus:** G12 ambient asset+player (partial — player hard-off)  
**Commit (gVRMod):** (pending)  
**Commit (vrmod-x64):** none  
**Tests:** `./scripts/test_all.sh` — 6/6 pass  

## What changed

1. **cube_hold.ogg** soft hold tone under `native_launcher/assets/ambient/` (install copy next to binary)  
2. **ambient_clip.hpp** PlayerDecide / AssetsDirCandidates / clip_present format + StatusLabelEx  
3. **CubeAmbientPlayerEnabled()** hard-off — no paplay/OpenAL spawn yet  
4. **ResolveCubeAmbientAssetsDir** + FillCubeAmbientClipPaths; handoff panel AUDIO uses real presence  
5. Unit: `launcher_ambient_player_decide`  

## Pain points

- Untouched.

## Gaps

- G12 still **partial** — real backend play path open (feature gate)  
- Next: G04 map attach, or G03 apply design, or ambient paplay careful  

## Notes

- cubalc_mirror dirty flood left unstaged.
