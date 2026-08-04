# gVRMod Cube polish loop

Durable self-tracking for the **11-minute** agent loop. Survives context prune — always read this directory first.

## Files

| File | Role |
|------|------|
| `LOOP_STATE.json` | Machine status: cycle #, last commit, next focus, test result |
| `PAIN_POINTS.md` | Do-not-thrash zones (play safe) |
| `GLOGIC_GAPS.md` | Living backlog of glogic gaps + polish opportunities |
| `CUBE_STANDARD.md` | What “highest Cube standard” means for this product |
| `JOURNAL.md` | Append-only cycle log (human readable) |
| `LAST_CYCLE.md` | Full report of the most recent cycle (overwrite) |

## Cadence

- Interval: **11 minutes**
- Goal: **one meaningful commit per successful cycle** when safe
- If nothing safe to change: update journal + gaps only (no empty commit)
- Always: offline tests before product push when code changed

## Repos

- `gVRMod` (this tree) — launcher, scripts, module, docs, state
- `addon/vrmod-x64` — Lua product (may be nested git)

Push both when both change. No force-push.
