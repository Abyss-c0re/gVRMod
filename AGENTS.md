# Agent notes — gVRMod

## Version control (mandatory)

Do **not** wait for the user to ask. After any meaningful code/docs/config change:

1. `git status` / `git diff` on **gVRMod** and **addon/vrmod-x64**
2. Commit with a clear message (what + why)
3. `git push` to `origin` on the current branch
4. Or run `./scripts/sync_repos.sh` (fetch/ff-pull when clean + push if ahead)

Never leave product work only in the working tree. Screenshots/temp go in `.scratch/` (gitignored).

Repos: `Abyss-c0re/gVRMod`, `Abyss-c0re/vrmod-x64`. No force-push / hard-reset of shared history unless the user explicitly asks.

## Testing (mandatory before push of product code)

Offline gate (no HMD / no GMod):

```bash
./scripts/test_all.sh          # contracts + Lua + C++ module + launcher
./scripts/test_all.sh --fast   # contracts + Lua only
./test.sh --no-clean           # rebuild release + full suite
```

- Design: `docs/TESTING_FRAMEWORK.md`
- Contracts: `tests/contracts/` (regen: `python3 scripts/gen_contracts.py`)
- Lua unit: `luajit tests/lua/run.lua`
- Scenarios: `luajit tests/scenarios/run.lua`
- In-game (optional): `./quick_test.sh`

New `vrmod.*` / `vrmod.utils.*` symbols must appear in contracts after `gen_contracts.py`. Prefer tests on **public** APIs.

## Cube native launcher (default desktop entry)

- **Default product entry:** desktop **gVRMod Cube** → `scripts/CubeUI.sh` → `install/native/CubeUI`
- Installer (`install.sh`) writes `~/.local/share/applications/gvrmod.desktop` to that script
- `scripts/gvrmod_launcher.sh` is **GMod-only helper** (after Start), not the menu
- Source: `native_launcher/`
- Defaults: `native_launcher/CubeUI.conf` (shipped; user overrides in `~/.config/gvrmod/`)

## Cube polish loop (every 11 min)

Durable self-tracking lives in **`state/polish_loop/`** (survives context prune).

- Brief: `state/polish_loop/AGENT_PROMPT.md`
- Pain points: `state/polish_loop/PAIN_POINTS.md` (climbing thrash, borderless, mat_queue_mode=2, dual pose, …)
- Gaps backlog: `state/polish_loop/GLOGIC_GAPS.md`
- Machine status: `state/polish_loop/LOOP_STATE.json`

Cadence: recover → one safe polish vs Cube standard → offline tests if code → **meaningful** commit+push (never empty) → update journal/state. Play safe; avoid regression.
