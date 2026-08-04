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
- G28 partial: soft handoff timeouts **90s** (GMod up) / **180s** hard; never racey early release; `CubeHandoffTimeout_*`; §0.15 open
- G29 partial: supersample cold Start **cap 1.4**; don’t crank SS at bring-up; `CubeSs_*`; §0.16 open
- G30 partial: FOV archive **write only when SETTINGS touched**; omit fovscale to preserve Vision cal; `CubeFov_*`; §0.17 open
- G31 partial: action-manifest **force-rewrite self-heal** + one retry; toast on fail; never abort VR; `BindingsLaw_*`; §0.18 open
- G32 partial: ShareTexture/HMD self-test **honest toast** (W7); delayed no-HMD; never silent black; `StereoSelfTest_*`; §0.19 open
- G33 partial: swap-eyes **SBS content only** (W4); IPD/FOV/pose single path; `SwapEyesLaw_*`; §0.20 open
- G34 partial: fly-away **one-shot origin snap** + `/actions/main` before input (W12); `FlyAwayLaw_*`; §0.21 open
- G35 partial: viewscale **default 1.0** clamp 0.1..2.0; comfort 0.75..1.25 fisheye risk (W8); `ViewScaleLaw_*` / `CubeViewScale_*`; §0.22 open
- G36 partial: FOV/Z **soft-refresh only** (W5); no mid-frame UV+FOV fight; `FovZLaw_*`; §0.23 open
- G37 partial: hands **Real for grabs**; bullets filtered/redirected (hand 0.45 / head 10×) (W9); `HandBulletLaw_*`; §0.24 open
- G38 partial: worldmodel **single path** (W10); floating hands OR worldmodel, no dual ghost; `WorldModelLaw_*`; §0.25 open
- Handoff should feel intentional: progress, cal continuity, fade — not “stuck?”
- G02 done (Cube shell): panel dim + both-eye black overlay (`GlFadeEyeBufferTowardBlack`) on take_xr
- G05 partial: `StereoLoad_IsLoading` + dual-hold toast + `StereoLoad_HmdExpect` checklist; paint through load when mat_queue&lt;2; never dual under mq≥2; HMD walk still open (TESTING_FRAMEWORK §0.1)
- G04 partial: default cold-spawns; skip-spawn + attach decide; changelevel opt-in; `WarmAttach_HmdExpect` + §0.6 walk; HMD warm proof open
- G13 partial: soft ack `panel_live` default; env `GVRMOD_CUBE_RECLAIM` → pure XR plan **panel refresh only** (never restart session); action rebind deferred; HMD walk §0.3 open
- G03 partial: pack + plan + opt-in executor (`vrmod_stage_apply` / `stage_apply_enable.txt`); **default off**; `StagePack_HmdExpect` + §0.4 walk; HMD height proof open
- G12 partial: gain law + `cube_hold.ogg` + ffplay; **default ON**; comfort master 0.55 or `GVRMOD_AMBIENT_MASTER`; silence `GVRMOD_AMBIENT_PLAY=0`; HMD taste walk §0.2 open
- G14 partial: stick SoT + wheel assist; `Glide_HmdExpect` enter checklist; HMD drive walk §0.5 open

## UI / interaction

- Laser + trigger click is sacred and must stay responsive (G16: pure primary-hand SoT + first-eye focus; §0.8 walk open)
- No menu thrash (register once; dedupe by id) — G26 partial: `MenuLaw_*` stable key + VRClimb collapse; HmdExpect §0.13 open
- No grab_end storms / left-trigger silence regressions
- World panel: world-locked by default; grip repositions
- G15 partial: HUD composite law — clear plate translucent; dim plate **additive** (PROPHECY); never opaque black slab; HMD walk §0.7 open
- G17 partial: `mat_queue_mode` Cube pin prefer **1**; VR never SetInt mq; dual only if mq&lt;2; HmdExpect §0.9 open
- G18 partial: desktop chrome **framed** (windowed, no force `-noborder`); `WindowChrome_*` pure; HmdExpect §0.10 open
- G19 partial: submit **dual OUT RGBA8** only; never eng IN / virgin OUT; `SubmitLaw_*`; HmdExpect §0.11 open
- G25 partial: pose **single path** raw→tracking→modifiers; no dual angvel/public SoT; `PoseSoT_*`; HmdExpect §0.12 open
- G26 partial: menu **dedupe** by id/name; VRClimb aliases one id; `MenuLaw_*`; HmdExpect §0.13 open
- G27 partial: never call engine **blocked** / lifecycle convars; `EngineBlacklist_*`; HmdExpect §0.14 open

## Compatibility

- Module &lt; 20 refuse; 20–22 degrade; ≥23 crisp SS path
- Optional args / version gates for new features
- Engine blacklists: never call blocked convars (G27 partial: `EngineBlacklist_*`; HmdExpect §0.14 open)

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
