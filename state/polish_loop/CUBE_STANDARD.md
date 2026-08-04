# Highest Cube standard — gVRMod bar

Derived from `addon/vrmod-x64/docs/CUBE_SYNTHESIS.md`, `CUBE_EXPERIENCE.md`, `CUBE_WATCHLIST.md`, launcher handoff design.

## One energy direction

```
WaitGetPoses → tracking SoT + modifiers + input
  → stereo RealRenderView → eng RT (IN)
  → blit eng IN → dual OUT (RGBA8)
  → OpenXR Submit(L/R)
```

Never submit eng texture. Never dual pose truth.

## Experience

- First-run: Welcome → Vision (border) → Posture → Complete
- After first-run: panel2vr + Glorious Crimson Cube; desktop stays Derma
- Seamless launcher: hold XR → STARTING GMOD → take_xr → release (no early void)
- Handoff should feel intentional: progress, cal continuity, fade — not “stuck?”

## UI / interaction

- Laser + trigger click is sacred and must stay responsive
- No menu thrash (register once; dedupe by id)
- No grab_end storms / left-trigger silence regressions
- World panel: world-locked by default; grip repositions

## Compatibility

- Module &lt; 20 refuse; 20–22 degrade; ≥23 crisp SS path
- Optional args / version gates for new features
- Engine blacklists: never call blocked convars

## Ship bar (offline proxy)

```bash
./scripts/test_all.sh          # or --fast when only Lua
```

In-headset smoke remains open; do not claim “shipped” without it.

## Polish taste

- Short clear commits (what + why)
- Prefer pure helpers + tests over spaghetti
- Toast honest errors; no silent death
- Small reversible diffs over hero rewrites
