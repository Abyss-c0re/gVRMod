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
- G02 done (Cube shell): panel dim + both-eye black overlay (`GlFadeEyeBufferTowardBlack`) on take_xr
- G05 partial: `StereoLoad_IsLoading` + dual-hold toast; paint through load when mat_queue&lt;2; never dual under mq≥2; HMD proof open
- G04 partial: default cold-spawns; skip-spawn plan + `WriteWarmAttachMarkers` when `GVRMOD_WARM_REUSE=1`; careful changelevel plan executor opt-in (`vrmod_warm_changelevel` / `warm_changelevel_enable.txt` / `GVRMOD_WARM_CHANGELEVEL`) — **default off**
- G13 partial: VR exit writes `cube_return.txt`; Cube soft-acks to `panel_live` after RETURN banner; XR rebind still env `GVRMOD_CUBE_RECLAIM` (off)
- G03 partial: pack + plan + opt-in executor (`vrmod_stage_apply` 1 or `stage_apply_enable.txt`); **default off** — no auto height jump
- G12 partial: gain law + `cube_hold.ogg` + ffplay backend; **default ON** during handoff (comfort master 0.55); silence with `GVRMOD_AMBIENT_PLAY=0`

## UI / interaction

- Laser + trigger click is sacred and must stay responsive
- No menu thrash (register once; dedupe by id)
- No grab_end storms / left-trigger silence regressions
- World panel: world-locked by default; grip repositions

## Compatibility

- Module &lt; 20 refuse; 20–22 degrade; ≥23 crisp SS path
- Optional args / version gates for new features
- Engine blacklists: never call blocked convars

## Ship bar (two layers — G24)

**Offline (required to push product):**

```bash
./scripts/test_all.sh          # or --fast when only Lua
```

Proves contracts + pure utils + module/launcher unit. **Not** headset-proven.

**HMD smoke (manual, no automation yet):** boot → handoff phases → stereo both eyes → laser UI → desktopview modes → cal/skip → pain-point checks.  
Tools: `./quick_test.sh`, Cube webui / `gvrmod_launcher.sh`. Details: [`docs/TESTING_FRAMEWORK.md`](../../docs/TESTING_FRAMEWORK.md) §0.

Never claim “shipped Ideal VR” or “HMD smoke passed” from offline green alone.

## Polish taste

- Short clear commits (what + why)
- Prefer pure helpers + tests over spaghetti
- Toast honest errors; no silent death
- Small reversible diffs over hero rewrites
