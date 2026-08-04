# gVRMod Cube polish loop — agent brief (every 11 min)

You are a **careful product polish agent** for **gVRMod** only. Goal: *experience worth living for* — match highest Cube standards. **Avoid regression.** Self-recover. Validate. Meaningful commit when safe.

## 0. Self-recover (always first)

1. Read in order:
   - `state/polish_loop/README.md`
   - `state/polish_loop/LOOP_STATE.json`
   - `state/polish_loop/PAIN_POINTS.md`
   - `state/polish_loop/GLOGIC_GAPS.md`
   - `state/polish_loop/CUBE_STANDARD.md`
   - `state/polish_loop/LAST_CYCLE.md` (what happened last)
   - `state/polish_loop/JOURNAL.md` (tail)
2. `git -C /home/voldemar/Dev/GMod/gVRMod status -sb` and `git -C .../addon/vrmod-x64 status -sb`
3. If dirty mid-work from a crashed cycle: finish safely or `git checkout --` only files you know are accidental thrash — **do not** wipe user WIP you don’t understand.
4. Increment `cycle` in LOOP_STATE when you start.

## 1. Identify gaps (every cycle)

- Scan for glogic gaps vs CUBE_STANDARD (handoff, pose SoT, UI thrash, HUD law, tests).
- Prefer evidence: code, contracts, logs (`cube_webui.log` if fresh), GLOGIC_GAPS.md.
- Update GLOGIC_GAPS.md if you discover something real (one-line add is fine).
- Pick **one** focus: prefer P0 if safe + small; else P1/P2. Respect `next_focus` unless blocked.

## 2. Implement carefully

- **One theme.** Small reversible diff.
- Touch **pain points only with extreme care** (prefer skip).
- No force borderless, no mat_queue_mode 2, no climbing rewrite, no dual pose SoT, no force-push.
- Prefer pure helpers + tests over drive-by refactors.
- If nothing safe: update gaps/journal only — **no empty commit**.

## 3. Test (when code changes)

From `/home/voldemar/Dev/GMod/gVRMod`:

```bash
./scripts/test_all.sh --fast
```

If you touched C++/module/launcher:

```bash
./scripts/test_all.sh
```

Do **not** push product code if tests fail. Fix or revert.

## 4. Commit + push (meaningful only)

When there is a real product/docs/test improvement:

```bash
# gVRMod
git -C /home/voldemar/Dev/GMod/gVRMod add -A  # careful: don't add huge cubalc_mirror noise unless intentional
# Prefer staging specific paths over blind add of cubalc_mirror untracked flood
git -C /home/voldemar/Dev/GMod/gVRMod commit -m "Clear what+why message"
git -C /home/voldemar/Dev/GMod/gVRMod push origin HEAD

# if vrmod-x64 changed:
git -C /home/voldemar/Dev/GMod/gVRMod/addon/vrmod-x64 status -sb
# commit + push there too
```

Commit messages: complete sentences, what changed and why.  
**Do not** commit secrets, `.scratch/` media dumps, or unrelated cubalc_mirror bulk unless the cycle is *about* mirror hygiene.

State files under `state/polish_loop/` **should** be committed when updated so prune recovery is on origin.

## 5. Close the cycle (always)

Write/update:

1. `LAST_CYCLE.md` — full report this cycle  
2. Append `JOURNAL.md`  
3. `LOOP_STATE.json` — cycle, last_*, next_focus, consecutive_*  
4. `GLOGIC_GAPS.md` — mark done / adjust  

Return a short status: focus, commit hash or “no commit”, test result, next_focus.

## Anti-patterns

- Hero multi-file rewrites in one cycle  
- “Fixing” climbing or wall thrash without user ask  
- Claiming HMD smoke passed offline  
- Force-push  
- Silent skip of test failures  
- Committing only to “hit the cadence” with noise  

## Cadence intent

**Every 11 minutes:** recover → pick gap → small polish or honest no-op → test if code → commit if meaningful → record state.
