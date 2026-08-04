# Last cycle

**Cycle:** 2  
**Time:** 2026-08-04T14:43:54+03:00  
**Focus:** G10 first-run gates re-spam when cal + native_wrapper  
**Commit (gVRMod):** `e2a0847`  
**Commit (vrmod-x64):** `70ff998`  
**Tests:** `./scripts/test_all.sh --fast` — 4/4 pass (+6 experience unit tests)  

## What changed

1. **`utils/sh_experience.lua`** — pure `Experience_ShouldRunFromState` (force / disabled / complete / G10 wrapper+cal).
2. **`cl_onboarding.lua`** — `Experience_ShouldRun` uses pure helper; prior cal = `border_profile.txt`.
3. **`cl_openxr_launch.lua`** — store `native_wrapper` from marker; `IsNativeWrapperLaunch()`; cfg path marks wrapper.
4. **Tests** — `tests/lua/unit/experience_test.lua` + runner wire-up.

## Pain points

- Untouched: climbing, -noborder, mat_queue_mode, dual pose, HUD, force-push.
- First-run without border profile still runs Welcome → Vision → Posture.
- `vrmod_experience_force` still overrides skip.

## Gaps

- G10 → **done**
- Next: G20 pure utils rewire (or G11 Quick Play)

## Notes

- cubalc_mirror dirty flood left unstaged.
- Submodule vrmod-x64 → `70ff998`.
