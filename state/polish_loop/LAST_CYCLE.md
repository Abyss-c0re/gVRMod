# Last cycle

**Cycle:** 43  
**Time:** 2026-08-04T22:16:00+03:00  
**Focus:** G25 pose SoT single-path law  
**Commit (gVRMod):** `7547b22`  
**Commit (vrmod-x64):** `fbf50f8`  
**Tests:** `./scripts/test_all.sh --fast` — 4/4 pass  

## What changed

1. **Pure** sh_pose_sot_law.lua — PipelineSteps; Public/Raw source; forbid second angvel/dual public; Decide/HmdExpect  
2. **cl_vrmod** stores `_poseSoT*` snapshot after UpdateTracking+ApplyPoseModifiers  
3. PURE_TESTED + unit test util.pose_sot_law.single_path_g25  
4. TESTING_FRAMEWORK §0.12 pose walk  
5. Gap G25 partial  

## Pain points

- Reinforces #4: one path raw→tracking→modifiers; no second angvel/pose SoT.

## Gaps

- G25 partial — HMD hands/gun lock walk open  
- Next: HMD walk backlog (G05/G12) or remaining partials  

## Notes

- cubalc_mirror dirty flood left unstaged.
