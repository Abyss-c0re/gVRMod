# Last cycle

**Cycle:** 41  
**Time:** 2026-08-04T21:52:00+03:00  
**Focus:** G18 framed window chrome law  
**Commit (gVRMod):** `e2dd4e3`  
**Commit (vrmod-x64):** (unchanged)  
**Tests:** `./scripts/test_all.sh` — 6/6 pass  

## What changed

1. **Pure** `window_chrome.hpp` — Cube pin windowed+framed; SanitizeNoborder; BuildArgs; Decide; HmdExpect  
2. **gmod_spawn** uses pure BuildArgs (no invent `-noborder`)  
3. last_play / ui_panel comments clarify G18 (missing key stays framed)  
4. TESTING_FRAMEWORK §0.10 chrome walk  
5. Gap G18 partial  

## Pain points

- Reinforces #2: never force GMod borderless; keep framed title chrome.

## Gaps

- G18 partial — desktop walk confirm open  
- Next: HMD walk backlog (G05/G12/G15–G17) or G03 stage height  

## Notes

- cubalc_mirror dirty flood left unstaged.
- vrmod-x64 not touched this cycle.
