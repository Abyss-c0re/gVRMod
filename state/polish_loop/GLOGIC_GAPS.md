# glogic gaps & polish backlog

Living backlog. Agents **promote / demote / mark done** each cycle. Newest insights at top of each section.

Last updated: 2026-08-04 cycle 28

## P0 — seamless / feel broken

| ID | Gap | Notes | Status |
|----|-----|-------|--------|
| G01 | Handoff progress opaque | Cube panel says holding XR; no map/engine phases | **done** cycle1 |
| G02 | No coordinated fade on take_xr | Panel + eye-buffer fade cycle14; Cube shell done | **done** cycle14 |
| G03 | Cal / STAGE not packed into handoff | Apply plan preview cycle26; auto apply still off | **partial** cycle26 |
| G04 | Cold Steam/hl2 every Start | skip-spawn plan cycle27; env GVRMOD_WARM_REUSE | **partial** cycle27 |
| G05 | Loading after take_xr may not be stereo | IsLoading+toast cycle25; HMD proof open | **partial** cycle25 |

## P1 — Cube experience polish

| ID | Gap | Notes | Status |
|----|-----|-------|--------|
| G10 | First-run gates re-spam on wrapper autostart | Should skip if cal exists + native_wrapper | **done** cycle2 |
| G11 | Quick Play (last map + gfx) missing | Reduces menu friction | **done** cycle4 |
| G12 | Audio dead during handoff | default-on + comfort master cycle28; opt-out=0 | **partial** cycle28 |
| G13 | Return-to-Cube reverse handoff | Poll+panel cycle21; auto reclaim hard-off | **partial** cycle21 |
| G14 | Glide vehicle input SoT | Watchlist W3; partial | **partial** cycle9 (pure SoT+toast; HMD smoke open) |

## P2 — code quality / glogic hygiene

| ID | Gap | Notes | Status |
|----|-----|-------|--------|
| G20 | Pure utils not fully rewired at call sites | laser/beam/finger → sh_math helpers | **done** cycle6 (TryParseColor theme) |
| G21 | Contract inventory lag for new symbols | gen_contracts after new vrmod.* | **done** cycle7 (pure pending=0 gate) |
| G22 | VERSION drift in cubalc mirror | informational; don’t “fix” upstream VERSION | n/a |
| G23 | Desktop follow-cam call sites incomplete | follow cam landed; verify all desktopview=4 paths | **done** cycle5 |
| G24 | Offline tests green but no HMD smoke automation | document only | **done** cycle8 (checklist; auto still open) |

## P3 — watchlist / workshop

See `addon/vrmod-x64/docs/CUBE_WATCHLIST.md` (W1–W12). Prefer smoke docs and tiny toasts over rewrites.

## Recently completed (keep short)

| ID | What | Commit / note |
|----|------|----------------|
| G12 | Ambient default-on careful | EnabledFromEnv + ComfortMaster 0.55 |
| G04 | Skip-spawn plan + warm_attach markers | SkipSpawnPlan + WriteWarmAttachMarkers |
| G03 | StagePack apply plan (preview only) | ComputeApplyPlan + MutationsFromPlan |
| G05 | Stereo-load IsLoading + toast gate | sh_stereo_load + cl_vrmod |
| G12 | Ambient ffplay backend (env opt-in) | ambient_backend + PlayArgv |
| G04 | Map attach decide (hard-off) | WarmAttach_* + noteWarmAttachOnce |
| G12 | Ambient asset + player decide (hard-off) | cube_hold.ogg + PlayerDecide |
| G13 | Reclaim poll design (hard-off) | cube_return.hpp + panel poll |
| G04 | Warm reuse design (hard-off) | warm_reuse.hpp |
| G12 | Ambient clip contract (no player) | ambient_clip.hpp |
| G13 | Reverse handoff protocol (partial) | vrmod-x64 b1dc55f |
| G03 | StagePack apply gate (no auto) | vrmod-x64 82a0a6d |
| G04 | Cold Start inventory (partial) | CubeLaunchBootKind |
| G05 | Stereo-load policy (partial) | vrmod-x64 879a551 |
| G02 | Eye-buffer layer fade (Cube shell) | GlFadeEyeBufferTowardBlack |
| G12 | Handoff audio gain law (no clip) | CubeHandoffAudioGain |
| G03 | STAGE pack parse+toast (no apply) | vrmod-x64 d052c35 + stage_pack.hpp |
| G03 | STAGE pack write (no apply) | stage_pack.hpp + cube_stage_pack.txt |
| G02 | Panel dim on take_xr / release | CubeHandoffFadeAmount |
| G14 | Glide stick SoT helpers + toast | vrmod-x64 e8318a9 |
| G24 | Offline vs HMD ship bar documented | TESTING_FRAMEWORK §0 |
| G21 | Pure contract inventory + fail-on-pending | gen_contracts PURE_TESTED |
| G20 | TryParseColor theme/laser/beam SoT | vrmod-x64 f9d5d8a |
| G23 | Follow-cam mode 4 path harden | vrmod-x64 8dc402c |
| G11 | Quick Play last map+gfx snapshot | last_play.hpp + Cube UI |
| G20 | Laser/beam ParseColor + finger curl SoT | vrmod-x64 a0b9d2e |
| G10 | Skip Experience re-spam wrapper+cal | vrmod-x64 70ff998 |
| G01 | Phase-aware handoff panel + map_ready | launcher helpers + vrmod-x64 b1ada40 |
| — | No forced -noborder | 70ea961 / 1dbb1b5 |
| — | quest media find patterns | b79d72c |
| — | desktop follow cam + broadcast | vrmod-x64 1beb0cf |

## Cycle pick rule

Each cycle: pick **one** open P0 if safe, else one P1/P2 with **small reversible diff**.  
If blocked by pain points → only docs/tests/gap inventory + journal update (no fake commit).
