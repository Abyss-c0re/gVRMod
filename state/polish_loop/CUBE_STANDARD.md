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
- G05 partial: `StereoLoadPolicy` keeps dual-eye paint through load when mat_queue&lt;2; never dual under mq≥2
- G04 partial: every Start still cold-spawns; `cube_warm.txt` records warm intent when process up (`CubeWarmReuseEnabled` hard-off)
- G13 partial: VR exit writes `cube_return.txt`; Cube does **not** auto-reclaim XR yet (relaunch shell)
- G03 partial: Cube writes `cube_stage_pack.txt`; GMod parses + `StagePack_ApplyDecision` (allow_apply=false); **must not** auto-jump origin/height without HMD-proven apply
- G12 partial: `CubeHandoffAudioGain` + `cube_ambient.txt` clip contract (`ambient/cube_hold.ogg`); OpenAL player optional future

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
