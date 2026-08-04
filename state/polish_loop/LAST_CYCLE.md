# Last cycle

**Cycle:** 24  
**Time:** 2026-08-04T18:45:49+03:00  
**Focus:** G12 paplay/ffplay ambient backend (env opt-in)  
**Commit (gVRMod):** `a80a832`  
**Commit (vrmod-x64):** none  
**Tests:** `./scripts/test_all.sh` — 6/6 pass  

## What changed

1. **ambient_backend.cpp** fork/exec ffplay (loop+volume) / paplay fallback  
2. Pure **PlayArgv / VolumePercent / ShouldRestartForGain / WantEnv**  
3. **GVRMOD_AMBIENT_PLAY=1** opt-in (default still silent)  
4. xr_app Apply on decide; stop when env off  
5. Unit: `launcher_ambient_backend_argv`  

## Pain points

- Untouched.

## Gaps

- G12 still **partial** — default silent; HMD smoke with env; OpenAL future  
- Next: G05 stereo-load notes, or default-on ambient after HMD proof  

## Notes

- cubalc_mirror dirty flood left unstaged.
