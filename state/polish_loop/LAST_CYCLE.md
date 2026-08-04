# Last cycle

**Cycle:** 17  
**Time:** 2026-08-04T17:26:58+03:00  
**Focus:** G03 StagePack apply gate (design — still no auto height)  
**Commit (gVRMod):** (pending close)  
**Commit (vrmod-x64):** `82a0a6d`  
**Tests:** `./scripts/test_all.sh --fast` — 4/4 pass  

## What changed

1. **StagePack_ApplyDecision / ApplyToast** pure band gate (close / far / deferred)  
2. **cl_openxr_launch** stores `g_VR._cubeStagePackApply`; allow_apply=false hard  
3. Toast from decision reason; **no** scale/seatedoffset/origin mutation  
4. Unit coverage inside util.stage_pack.parse_and_hint  

## Pain points

- Untouched; FOV archives and dual pose law preserved.

## Gaps

- G03 still **partial** — full HMD-proven apply path open (gate ready)  
- Next: G12 ambient clip, G13 reverse handoff note, or G04 warm reuse design  

## Notes

- cubalc_mirror dirty flood left unstaged.
