# Last cycle

**Cycle:** 12  
**Time:** 2026-08-04T16:32:44+03:00  
**Focus:** G03 STAGE pack Lua parse + toast (apply still deferred)  
**Commit (gVRMod):** `0b545f9`  
**Commit (vrmod-x64):** `d052c35`  
**Tests:** `./scripts/test_all.sh --fast` — 4/4 pass  

## What changed

1. **sh_stage_pack.lua** pure Parse / Normalize / IsUsable / ToastHint  
2. **cl_openxr_launch** reads `cube_stage_pack.txt` after VR live → `g_VR._cubeStagePack` + toast  
3. Unit test `util.stage_pack.parse_and_hint`; PURE_TESTED map updated  
4. **No** origin / scale / seatedoffset mutation (height apply deferred)

## Pain points

- Untouched; FOV archives and soft handoff timeouts unchanged.

## Gaps

- G03 still **partial** — continuity data end-to-end (write + read + toast); apply path open  
- Next: G12 audio design-only, or careful G03 apply only with pure decision + HMD note  

## Notes

- cubalc_mirror dirty flood left unstaged.
