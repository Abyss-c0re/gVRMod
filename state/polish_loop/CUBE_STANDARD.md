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
- G05 partial: `StereoLoad_IsLoading` + dual-hold toast + `StereoLoad_HmdExpect` checklist; paint through load when mat_queue&lt;2; never dual under mq≥2; HMD walk still open (TESTING_FRAMEWORK §0.1)
- G04 partial: default cold-spawns; skip-spawn + attach decide; changelevel opt-in; `WarmAttach_HmdExpect` + §0.6 walk; HMD warm proof open
- G13 partial: soft ack `panel_live` default; env `GVRMOD_CUBE_RECLAIM` → pure XR plan **panel refresh only** (never restart session); action rebind deferred; HMD walk §0.3 open
- G03 partial: pack + plan + opt-in executor (`vrmod_stage_apply` / `stage_apply_enable.txt`); **default off**; `StagePack_HmdExpect` + §0.4 walk; HMD height proof open
- G12 partial: gain law + `cube_hold.ogg` + ffplay; **default ON**; comfort master 0.55 or `GVRMOD_AMBIENT_MASTER`; silence `GVRMOD_AMBIENT_PLAY=0`; HMD taste walk §0.2 open
- G14 partial: stick SoT + wheel assist; `Glide_HmdExpect` enter checklist; HMD drive walk §0.5 open

## UI / interaction

- Laser + trigger click is sacred and must stay responsive
- No menu thrash (register once; dedupe by id)
- No grab_end storms / left-trigger silence regressions
- World panel: world-locked by default; grip repositions
- G15 partial: HUD composite law — clear plate translucent; dim plate **additive** (PROPHECY); never opaque black slab; HMD walk §0.7 open

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
