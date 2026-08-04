# Last cycle

**Cycle:** 30  
**Time:** 2026-08-04T19:52:15+03:00  
**Focus:** G13 soft reclaim ack (panel_live)  
**Commit (gVRMod):** (pending)  
**Commit (vrmod-x64):** none  
**Tests:** `./scripts/test_all.sh` — 6/6 pass  

## What changed

1. **CubeReclaimAckPlanDecide** soft ack after 2.5s RETURN hold  
2. **WriteCubeReturnMarker** I/O; phase=panel_live  
3. Soft ack default ON; `GVRMOD_CUBE_RECLAIM` for experimental auto path  
4. xr_app poll: ack + clear RETURN banner  
5. Unit expanded reclaim soft-ack  

## Pain points

- Untouched; no XR session thrash.

## Gaps

- G13 soft path shipped offline; full XR reclaim still open  
- Next: G04 changelevel careful or G12 HMD volume  

## Notes

- cubalc_mirror dirty flood left unstaged.
