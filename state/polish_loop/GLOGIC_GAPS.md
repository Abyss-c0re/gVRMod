# glogic gaps & polish backlog

Living backlog. Agents **promote / demote / mark done** each cycle. Newest insights at top of each section.

Last updated: 2026-08-04 cycle 46

## P0 — seamless / feel broken

| ID | Gap | Notes | Status |
|----|-----|-------|--------|
| G01 | Handoff progress opaque | Cube panel says holding XR; no map/engine phases | **done** cycle1 |
| G02 | No coordinated fade on take_xr | Panel + eye-buffer fade cycle14; Cube shell done | **done** cycle14 |
| G03 | Cal / STAGE not packed into handoff | HmdExpect+§0.4 cycle35; default still off | **partial** cycle35 |
| G04 | Cold Steam/hl2 every Start | HmdExpect+§0.6 cycle37; default still cold | **partial** cycle37 |
| G05 | Loading after take_xr may not be stereo | HmdExpect+§0.1 checklist cycle32; HMD walk open | **partial** cycle32 |
| G28 | Soft handoff timeouts (no racey release) | CubeHandoffTimeout_* cycle46; HMD walk open | **partial** cycle46 |

## P1 — Cube experience polish

| ID | Gap | Notes | Status |
|----|-----|-------|--------|
| G10 | First-run gates re-spam on wrapper autostart | Should skip if cal exists + native_wrapper | **done** cycle2 |
| G11 | Quick Play (last map + gfx) missing | Reduces menu friction | **done** cycle4 |
| G12 | Audio dead during handoff | master env+HmdExpect cycle33; HMD walk open | **partial** cycle33 |
| G13 | Return-to-Cube reverse handoff | panel_refresh XR plan cycle34; rebind deferred | **partial** cycle34 |
| G14 | Glide vehicle input SoT | HmdExpect+§0.5 cycle36; HMD walk open | **partial** cycle36 |
| G15 | Opaque black HUD / wall of Real | HudLaw composite cycle38; HMD walk open | **partial** cycle38 |
| G16 | Laser + trigger UI sacred | LaserLaw pure cycle39; HMD walk open | **partial** cycle39 |
| G17 | mat_queue_mode pin (never 2 from VR) | MatQueueLaw cycle40; confirm open | **partial** cycle40 |
| G18 | Framed desktop chrome (never force -noborder) | WindowChrome_* cycle41; desktop walk open | **partial** cycle41 |
| G19 | Submit eng IN / virgin OUT forbidden | SubmitLaw_* cycle42; HMD walk open | **partial** cycle42 |
| G25 | Dual-truth pose/angvel SoT forks | PoseSoT_* cycle43; HMD walk open | **partial** cycle43 |
| G26 | QM menu thrash / VRClimb dupes | MenuLaw_* cycle44; HMD walk open | **partial** cycle44 |
| G27 | Engine blacklist never-call (W2) | EngineBlacklist_* cycle45; console walk open | **partial** cycle45 |

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
| G28 | Soft handoff 90s/180s never racey | CubeHandoffTimeout_* + xr_app wire |
| G27 | Engine blacklist never-call (W2) | EngineBlacklist_* + setConvarValue gate |
| G26 | Menu thrash / VRClimb dedupe by id | MenuLaw_* + cl_api wire |
| G25 | Pose single path (no dual angvel/public SoT) | PoseSoT_* + cl_vrmod snapshot |
| G19 | Submit dual OUT only (never eng IN / virgin) | SubmitLaw_* + cl_vrmod snapshot |
| G18 | Framed window chrome (never force -noborder) | WindowChrome_* + gmod_spawn BuildArgs |
| G17 | mat_queue pin law (never VR-write 2) | MatQueueLaw_* + cl_vrmod |
| G16 | Laser sacred law (primary+focus) | LaserLaw_* + cl_ui wire |
| G15 | HUD additive law (PROPHECY) | HudLaw_Decide + cl_hud wire |
| G04 | Warm HmdExpect + §0.6 smoke walk | WarmAttach_HmdExpect + CubeWarm |
| G14 | Glide HmdExpect + §0.5 smoke walk | Glide_HmdExpect + cl_input |
| G03 | HMD stage-apply expect + §0.4 walk | StagePack_HmdExpect |
| G13 | XR reclaim panel_refresh plan (env) | XrPlanDecide + §0.3 walk |
| G12 | Ambient master env + HMD volume expect | MasterFromEnv + §0.2 walk |
| G05 | HMD load-flash expect + §0.1 walk | StereoLoad_HmdExpect + TESTING_FRAMEWORK |
| G04 | Careful changelevel plan executor (opt-in) | WarmAttach_ChangelevelPlan + RCC wire |
| G13 | Soft reclaim ack (panel_live) | AckPlan + WriteCubeReturnMarker |
| G03 | StagePack plan executor (opt-in) | ExecuteMutations + vrmod_stage_apply |
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
