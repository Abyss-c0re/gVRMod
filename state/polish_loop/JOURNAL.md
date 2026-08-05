# Polish loop journal

Append one block per cycle. Newest at bottom.

---

## 2026-08-04 bootstrap

- Created durable polish loop state under `state/polish_loop/`.
- Interval: 11 minutes. Goal: meaningful commit when safe; never regress pain points.
- Seeded GLOGIC_GAPS from seamless-handoff analysis + Cube synthesis/watchlist.
- Next focus: G01 handoff progress (or safer docs/test if blocked).

## 2026-08-04 cycle 1 — G01 handoff phases

- Theme: seamless handoff progress (status file + UI strings only).
- Native: CubeHandoffDetailForPhase / Progress / PhaseLabel in gmod_spawn.hpp; wired in xr_app + ui_panel.
- Lua: writeHandoff("map_ready") on InitPostEntity (openxr launch / autostart).
- Tests: full test_all green; +3 launcher unit tests.
- Commits: vrmod-x64 b1ada40; gVRMod (launcher + state + submodule).
- Next: G10 first-run gates re-spam (or G20 pure utils).

## 2026-08-04 cycle 2 — G10 first-run re-spam

- Theme: skip Cube Experience on native_wrapper when Vision cal (border_profile) exists.
- Pure: Experience_ShouldRunFromState in utils/sh_experience.lua (+ 6 unit tests).
- Launch marker stores native_wrapper; IsNativeWrapperLaunch helper.
- Tests: test_all --fast PASS 4/4.
- Commits: vrmod-x64 70ff998; gVRMod (tests + submodule + state).
- Next: G20 pure utils rewire or G11 Quick Play (safe).

## 2026-08-04 cycle 3 — G20 pure utils rewire

- Theme: laser/beam color + finger curl call sites → sh_math helpers.
- ParseColor: cl_laser_pointer, cl_ui beam.
- FingerDigitIndex + LerpFingerAngle: cl_character_hands, sh_character_fbt, cl_character_ik.
- Tests: test_all --fast PASS 4/4.
- Commits: vrmod-x64 a0b9d2e; gVRMod submodule + state.
- G20 partial (more sites remain e.g. cube_framework color); next G11 Quick Play or G23 follow-cam verify.

## 2026-08-04 cycle 4 — G11 Quick Play

- Theme: last map + gfx snapshot for Cube Quick Play (safe UI/status file).
- Pure: last_play.hpp Format/Parse (+ 2 launcher unit tests).
- Save cube_last_play.txt on successful Start; restore on CubeUI_Init; QUICK PLAY button.
- No -noborder force; mat_queue untouched.
- Tests: full test_all PASS 6/6; CubeUI builds.
- Next: G23 follow-cam paths verify or G02 fade (careful).

## 2026-08-04 cycle 5 — G23 follow-cam paths

- Theme: verify/fix desktopview=4 call sites (no stereo crop fallthrough).
- Lua: IsEyeCropMode/Clamp/Cycle/Label; cl_vrmod follow vs eye-crop branch; crop for mode 1/4.
- Launcher: explicit RIGHT label; last_play clamps xr_desktopview 1..4; docs call-site table.
- Tests: full test_all PASS 6/6 (+desktop enum unit + clamp test).
- Commits: vrmod-x64 8dc402c; gVRMod submodule + launcher + docs + state.
- Next: G20 residual (cube_framework color) or inventory G21; avoid G02 fade without plan.

## 2026-08-04 cycle 6 — G20 residual color SoT

- Theme: TryParseColor + rewire cube_framework accent, laser, beam.
- Pure: vrmod.utils.TryParseColor (nil on fail); ParseColor uses it.
- Tests: --fast PASS 4/4 (+try_parse unit).
- Commits: vrmod-x64 f9d5d8a; gVRMod tests + submodule + state.
- G20 → done (core color/finger rewires complete).
- Next: G21 contracts inventory lag (gen after symbols) or safe docs G24.

## 2026-08-04 cycle 7 — G21 contract inventory

- Theme: pure symbols inventory lag — gen + check + unit tests.
- PURE_TESTED: TryParseColor, ComputeSubmitBounds, AdjustFOV, Experience_ShouldRunFromState.
- SEAM_FORCE: ComputePhysicsParams (engine/model).
- check_test_contracts fails if pure-pending > 0; pure pending now 0.
- Unit: submit_bounds + adjust_fov; launcher.yaml handoff/last_play contracts.
- Tests: --fast PASS 4/4 (30 Lua tests).
- Next: G24 HMD smoke docs only, or safe P1 G12/G14 watchlist docs.

## 2026-08-04 cycle 8 — G24 offline vs HMD smoke bar

- Theme: document-only ship bar law (no product code).
- docs/TESTING_FRAMEWORK.md §0: offline proves vs HMD checklist; agent law.
- CUBE_STANDARD + README: two-layer bar; never claim HMD from offline green.
- G24 → done (automation still open as future work).
- Next: P0 G02 fade only with tiny safe step, or G14 Glide watchlist smoke note.

## 2026-08-04 cycle 9 — G14 Glide stick SoT + toast

- Theme: Glide W3/G14 pure SoT helpers + toast copy + smoke note.
- Pure: GlideSeatIsDriver / GlidePreferStickSteer (sh_glide_sot.lua) + unit test.
- cl_input: wire seat resolve + steer blend; shorter enter/unbound toasts.
- WATCHLIST W3: HMD smoke checklist (manual).
- Tests: --fast PASS 4/4 (31 Lua, pure pending=0).
- Commits: vrmod-x64 e8318a9; gVRMod submodule + tests + state.
- G14 remains partial (full HMD smoke still open); offline SoT/toast landed.
- Next: careful P0 G02 fade design note, or G12 audio docs-only.

## 2026-08-04 cycle 10 — G02 panel fade on take_xr

- Theme: coordinated *feel* without full compositor layer fade (safe).
- Pure: CubeHandoffFadeAmount; PHASE TAKE XR · FADE; detail copy.
- xr_app sets handoffFade; ui_panel BlendTowardBlack + FADE % status.
- Full XR layer fade still open (G02 partial).
- Tests: full test_all PASS 6/6 (+fade unit test).
- Next: G03 cal/STAGE pack (careful) or G12 audio design-only.

## 2026-08-04 cycle 11 — G03 STAGE pack (write only)

- Theme: pack STAGE/LOCAL + head sample into handoff for cal continuity (no apply).
- Pure: stage_pack.hpp Format/Parse/IsUsable (+ 3 launcher unit tests).
- Write cube_stage_pack.txt on Start + refresh at take_xr; panel SPACE line.
- Keep pack across ClearCubeHandoffMarkers (GMod reads after claim).
- G03 partial — GMod origin/height apply still open (pain-point careful).
- Tests: full test_all PASS 6/6 (14 launcher tests).
- Next: careful G03 apply hint, or G12 audio design-only, or G02 layer fade.

## 2026-08-04 cycle 12 — G03 STAGE pack read + toast

- Theme: GMod side pure parse + continuity toast (no height/origin apply).
- Pure: sh_stage_pack.lua StagePack_* helpers + unit test.
- cl_openxr_launch noteStagePackOnce after VR live; g_VR._cubeStagePack.
- PURE_TESTED: 4 StagePack symbols → util.stage_pack.parse_and_hint.
- Tests: --fast PASS 4/4 (32 Lua, pure pending=0).
- Commits: vrmod-x64 d052c35; gVRMod submodule + tests + state.
- G03 remains partial (apply deferred); next G12 audio design-only or careful apply design.

## 2026-08-04 cycle 13 — G12 handoff audio gain law

- Theme: ambient gain contract during seamless handoff (no audio engine).
- Pure: CubeHandoffAudioGain (hold → duck take_xr → silence on exit).
- Panel AUDIO line; inverse of fade on release window.
- G12 partial — actual ambient clip still open.
- Tests: full test_all PASS 6/6 (+audio gain unit).
- Next: G02 XR layer design note, or G05/G04 careful inventory.

## 2026-08-04 cycle 14 — G02 eye-buffer layer fade

- Theme: coordinated fade on take_xr for full HMD content (not panel-only).
- Pure: CubeHandoffLayerFadeAlpha; GlFadeEyeBufferTowardBlack overlay.
- xr_app both eyes after lasers; panel LAYER FADE % copy.
- G02 shell done; GMod-side black/load flash still G05.
- Tests: full test_all PASS 6/6 (+layer alpha unit).
- Next: G05 stereo-during-load pure gate/inventory, or G03 apply design.

## 2026-08-04 cycle 15 — G05 stereo-load policy

- Theme: dual-eye paint through early load after take_xr (safe mq path).
- Pure: StereoLoadPolicy / ShouldPaintStereoThisFrame (+ unit test).
- cl_vrmod RenderScene uses policy; never dual under mat_queue≥2.
- G05 partial — HMD load-flash proof still open.
- Tests: --fast PASS 4/4 (33 Lua, pure pending=0).
- Commits: vrmod-x64 879a551; gVRMod submodule + tests + state.
- Next: G04 cold Start note, or G03 apply design-only.

## 2026-08-04 cycle 16 — G04 cold Start inventory

- Theme: name cold Steam/hl2 gap; detect warm process without unsafe reuse.
- Pure: CubeLaunchBootKind/Label/ColdStartProgressSeconds/ShouldSkipSpawn.
- Panel BOOT line; 55s cold progress fallback; still always cold-spawn.
- G04 partial — warm reuse needs map-change/attach protocol.
- Tests: full test_all PASS 6/6 (+cold start unit).
- Next: G03 apply design-only or G12 ambient clip.

## 2026-08-04 cycle 17 — G03 StagePack apply gate

- Theme: pure apply decision for STAGE pack height (never auto-jump).
- Pure: StagePack_ApplyDecision / ApplyToast (close/far/eligible_deferred).
- openxr_launch: allow_apply=false; g_VR._cubeStagePackApply; toast reason.
- G03 partial — real apply still HMD-gated future work.
- Tests: --fast PASS 4/4 (pure pending=0).
- Commits: vrmod-x64 82a0a6d; gVRMod submodule + tests + state.
- Next: G12 ambient clip or G13 reverse handoff design-only.

## 2026-08-04 cycle 18 — G13 reverse handoff protocol

- Theme: return-to-Cube marker contract without auto reclaim.
- Pure: CubeReturn_* Lua + CubeReverse* launcher labels.
- VR exit writes cube_return.txt; Start clears it; toast relaunch hint.
- G13 partial — Cube shell reclaim still future.
- Tests: full test_all PASS 6/6.
- Commits: vrmod-x64 b1dc55f; gVRMod submodule + launcher + state.
- Next: G12 ambient clip or G04 warm reuse design.

## 2026-08-04 cycle 19 — G12 ambient clip contract

- Theme: name the ambient clip + status SoT without OpenAL.
- Pure: ambient_clip.hpp (ShouldPlay/Format/Parse/StatusLabel).
- handoff writes cube_ambient.txt; panel AUDIO clip labels.
- G12 partial — real player + asset still open.
- Tests: full test_all PASS 6/6 (+ambient unit).
- Next: G04 warm reuse design or ambient asset+player careful.

## 2026-08-04 cycle 20 — G04 warm reuse design

- Theme: pure warm-process eligibility + request marker; spawn still cold.
- Pure: warm_reuse.hpp Decide/Format/Parse; feature hard-off.
- Start writes cube_warm.txt when process up; never skip steam yet.
- G04 partial — real map attach/warm_reuse open.
- Tests: full test_all PASS 6/6 (+warm reuse unit).
- Next: G13 Cube reclaim poll or ambient player careful.

## 2026-08-04 cycle 21 — G13 Cube reclaim poll

- Theme: poll cube_return.txt on Cube panel; pure reclaim decide hard-off.
- Pure: cube_return.hpp Format/Parse + CubeReclaimDecide/PanelLabel.
- ReadCubeReturnMarker; xr_app 1Hz poll when !handoff; New Game RETURN banner.
- CubeReclaimEnabled hard-off — auto reclaim branch empty.
- G13 partial — true reverse reclaim still open.
- Tests: full test_all PASS 6/6 (+reclaim poll unit); CubeUI builds.
- Next: G12 ambient asset+player careful or G04 map attach.

## 2026-08-04 cycle 22 — G12 ambient asset+player

- Theme: ship hold clip asset + pure player decide; playback hard-off.
- Asset: native_launcher/assets/ambient/cube_hold.ogg; POST_BUILD install.
- Pure: PlayerDecide/AssetsDirCandidates/clip_present/StatusLabelEx.
- Resolve assets dir (env/exe/source); panel AUDIO uses real clip presence.
- CubeAmbientPlayerEnabled hard-off — start/stop backend branch empty.
- G12 partial — audible paplay/OpenAL still open.
- Tests: full test_all PASS 6/6 (+player decide unit); launcher builds.
- Next: G04 map attach or ambient backend careful.

## 2026-08-04 cycle 23 — G04 map attach design

- Theme: pure warm map-attach decide; never auto changelevel.
- Pure C++: CubeWarmAttach_NormalizeMap/Decide/Toast.
- Pure Lua: sh_warm_attach.lua Parse/Decide/Toast.
- openxr_launch noteWarmAttachOnce toast after VR live; allow_changelevel=false.
- G04 partial — skip-spawn + real changelevel still open.
- Tests: full test_all PASS 6/6 (+attach unit Lua/C++).
- Next: G12 paplay backend careful or G05 careful.

## 2026-08-04 cycle 24 — G12 ambient ffplay/paplay backend

- Theme: careful external player; default off, env opt-in.
- ambient_backend: fork ffplay -loop -volume (paplay fallback).
- Pure: PlayArgv/VolumePercent/RestartForGain/WantEnv.
- Wire xr_app AmbientBackend_Apply when GVRMOD_AMBIENT_PLAY=1.
- G12 partial — product default still silent until HMD-proven default-on.
- Tests: full test_all PASS 6/6 (+backend argv unit); launcher builds.
- Next: G05 stereo-load HMD notes or ambient default-on careful.

## 2026-08-04 cycle 25 — G05 stereo-load detect + toast

- Theme: pure loading detector + status/toast gate; wire cl_vrmod.
- Pure: StereoLoad_IsLoading / StatusLabel / ShouldToast.
- RenderScene uses IsLoading flags; one-shot dual-hold toast; clear on exit.
- TESTING_FRAMEWORK §0 G05 HMD checklist item.
- G05 partial — HMD load-flash proof still open.
- Tests: --fast PASS 4/4 (35 Lua, pure pending=0).
- Next: G03 apply careful or G04 skip-spawn design or ambient default-on.

## 2026-08-04 cycle 26 — G03 StagePack apply plan

- Theme: pure seated-offset apply plan preview; never auto-apply.
- Pure: StagePack_ComputeApplyPlan / PlanToast / MutationsFromPlan.
- openxr_launch stores plan + empty mutations; allow_apply=false.
- G03 partial — real seatedoffset SetFloat still HMD-gated.
- Tests: --fast PASS 4/4 (pure pending=0).
- Next: G04 skip-spawn design or ambient default-on careful.

## 2026-08-04 cycle 27 — G04 skip-spawn plan

- Theme: pure skip-spawn attach plan + markers without Steam; env opt-in.
- Pure: CubeWarmSkipSpawnPlanDecide / phase labels; GVRMOD_WARM_REUSE WantEnv.
- WriteWarmAttachMarkers (openxr_launch + handoff phase=warm_attach).
- xr_app skip branch: markers + stage pack + last_play when feature on.
- Default still cold-spawn (env unset).
- G04 partial — changelevel on warm map-mismatch still open.
- Tests: full test_all PASS 6/6 (+skip spawn plan unit).
- Next: ambient default-on careful or G03 plan executor careful.

## 2026-08-04 cycle 28 — G12 ambient default-on careful

- Theme: handoff hold tone default ON; soft comfort master; opt-out env.
- Pure: EnabledFromEnv(defaultOn) / EnvIsOff / ComfortMaster 0.55.
- Product: GVRMOD_AMBIENT_PLAY unset → play when clip present; =0 silences.
- PlayerDecide volume uses comfort master; still handoff-gated.
- TESTING_FRAMEWORK ambient checklist updated.
- G12 partial → near done offline; HMD soft-volume taste open.
- Tests: full test_all PASS 6/6.
- Next: G03 plan executor careful or G13 reclaim careful.

## 2026-08-04 cycle 29 — G03 StagePack plan executor careful

- Theme: opt-in seated apply executor; product default still off.
- Pure: AllowApplyFromFlags / ShouldExecutePlan / ExecuteMutations / ExecuteToast.
- openxr_launch: vrmod_stage_apply or stage_apply_enable.txt unlocks; SetFloat seated.
- Default path unchanged (preview toast only).
- G03 partial — HMD-proven default-on still open.
- Tests: --fast PASS 4/4 (pure pending=0).
- Next: G13 reclaim careful or G04 changelevel careful.

## 2026-08-04 cycle 30 — G13 soft reclaim ack

- Theme: soft ack reverse handoff after RETURN banner; no XR rebind.
- Pure: CubeReclaimAckPlanDecide + SoftAck hold; env GVRMOD_CUBE_RECLAIM.
- WriteCubeReturnMarker; xr_app acks to panel_live after ~2.5s.
- Soft ack default ON; XR rebind still env-gated empty path.
- G13 partial → soft complete offline; full reclaim still open.
- Tests: full test_all PASS 6/6.
- Next: G04 changelevel careful or G05 HMD notes.

## 2026-08-04 cycle 31 — G04 careful changelevel plan

- Theme: opt-in warm map changelevel executor; product default still off.
- Pure Lua: MapTokenOk/Allow/Plan/Cmd/ShouldExecute/Execute/ExecuteToast.
- Pure C++: CubeWarmChangelevel* + MapTokenOk + AllowFromFlags.
- openxr_launch: vrmod_warm_changelevel or warm_changelevel_enable.txt unlocks RCC.
- Default path unchanged (deferred toast only).
- G04 partial — HMD-proven default-on still open.
- Tests: full test_all PASS 6/6.
- Next: G05 HMD load-flash notes or G12 HMD volume taste.

## 2026-08-04 cycle 32 — G05 HMD load-flash expect

- Theme: pure HMD observer contract + smoke checklist docs; no render thrash.
- Pure: StereoLoad_HmdExpect / FlashRiskIsBad (verdict, flash_risk, checklist).
- cl_vrmod logs G05 HMD checklist once with dual-hold toast.
- TESTING_FRAMEWORK §0.1 walk table + procedure.
- G05 partial — headset walk still required for 'OK'.
- Tests: --fast PASS 4/4 (pure pending=0).
- Next: G12 HMD volume taste or G13 XR reclaim careful.

## 2026-08-04 cycle 33 — G12 ambient master taste

- Theme: env master override + pure HMD volume expect; default comfort 0.55.
- Pure: MasterFromEnv/ClampMaster/TasteBand/HmdVolumeExpect.
- ComfortMaster uses GVRMOD_AMBIENT_MASTER when set (0.05..1).
- TESTING_FRAMEWORK §0.2 volume taste procedure.
- G12 partial → offline complete-ish; headset taste still open.
- Tests: full test_all PASS 6/6.
- Next: G13 XR reclaim careful or G03 HMD stage-apply notes.

## 2026-08-04 cycle 34 — G13 careful XR reclaim plan

- Theme: env XR reclaim = panel refresh only; never restart OpenXR session.
- Pure: CubeReclaimXrPlanDecide/ShouldExecute/Label/HmdExpect.
- xr_app: plan from pre-ack decision; status RECLAIM · PANEL REFRESH on env path.
- TESTING_FRAMEWORK §0.3 reclaim walk.
- G13 partial — action rebind deferred; HMD walk open.
- Tests: full test_all PASS 6/6.
- Next: G03 HMD stage-apply notes or G14 Glide smoke notes.

## 2026-08-04 cycle 35 — G03 HMD stage-apply expect

- Theme: pure HMD height continuity expect + smoke checklist; no auto apply.
- Pure: StagePack_HmdExpect / HeightJumpRiskIsBad.
- openxr_launch logs G03 HMD checklist with stage pack note.
- TESTING_FRAMEWORK §0.4 walk table + procedure.
- G03 partial — headset height proof still open.
- Tests: --fast PASS 4/4 (pure pending=0).
- Next: G14 Glide smoke notes or G04 warm HMD notes.

## 2026-08-04 cycle 36 — G14 Glide HmdExpect

- Theme: pure Glide enter/drive HMD expect + smoke checklist; stick SoT unchanged.
- Pure: Glide_HmdExpect/StatusLabel/EnterToast/ShouldToastEnter/SteerSourceLabel.
- cl_input: enter toast from pure helpers; log G14 HMD checklist; track steer source.
- TESTING_FRAMEWORK §0.5 Glide walk.
- G14 partial — headset+Glide walk still open.
- Tests: --fast PASS 4/4 (pure pending=0).
- Next: G04 warm HMD notes or backlog HMD walks.

## 2026-08-04 cycle 37 — G04 warm HmdExpect

- Theme: pure warm attach/changelevel HMD expect + smoke checklist; defaults unchanged.
- Pure Lua WarmAttach_HmdExpect; pure C++ CubeWarm_HmdExpect.
- openxr_launch logs G04 HMD checklist once with warm note.
- TESTING_FRAMEWORK §0.6 warm walk.
- G04 partial — headset warm proof still open.
- Tests: full test_all PASS 6/6.
- Next: HMD walk backlog or HUD additive notes.

## 2026-08-04 cycle 38 — G15 HUD additive law

- Theme: PROPHECY HUD composite — clear translucent, dim additive; never black wall.
- Pure: HudLaw_Decide/MaterialFlags/HmdExpect/IsBlackSlabRisk.
- cl_hud: draw path applies pure flags from vrmod_hudtestalpha.
- TESTING_FRAMEWORK §0.7 HUD walk.
- G15 partial — HMD proof open.
- Tests: --fast PASS 4/4 (pure pending=0).
- Next: HMD walk backlog (G05/G12) or laser sacred notes.

## 2026-08-04 cycle 39 — G16 laser sacred law

- Theme: pure primary-hand SoT + first-eye focus solve; laser+trigger sacred.
- Pure: LaserLaw_PrimaryHand/Click/ShouldSolveFocus/HmdExpect.
- cl_ui: hand + click + focus resolve use pure helpers (fallback preserved).
- TESTING_FRAMEWORK §0.8 laser walk.
- G16 partial — HMD UI walk open.
- Tests: --fast PASS 4/4 (pure pending=0).
- Next: HMD walk backlog (G05/G12) or mat_queue pin notes.

## 2026-08-04 cycle 40 — G17 mat_queue pin law

- Theme: Cube pin prefer 1; VR never SetInt mat_queue; dual only mq<2.
- Pure: MatQueueLaw_CubePin/ClampRead/ShouldWrite/AllowDualEye/Decide/HmdExpect.
- cl_vrmod WantedMatQueueMode uses pure clamp + decision snapshot.
- TESTING_FRAMEWORK §0.9 mq walk.
- G17 partial — product already never writes; law now offline-tested.
- Tests: --fast PASS 4/4 (pure pending=0).
- Next: HMD walk backlog (G05/G12) or -noborder framed law notes.

## 2026-08-04 cycle 41 — G18 framed window chrome law

- Theme: never force -noborder; windowed framed desktop mirror pin.
- Pure: WindowChrome_Cube*/Sanitize/BuildArgs/Decide/HmdExpect.
- gmod_spawn uses pure BuildArgs for Steam/hl2 cmdline.
- TESTING_FRAMEWORK §0.10 desktop chrome walk.
- G18 partial — product already framed; law now offline-tested.
- Tests: full test_all PASS 6/6 (launcher 28 tests).
- Next: HMD walk backlog (G05/G12) or remaining partial HmdExpects.

## 2026-08-04 cycle 42 — G19 submit path law

- Theme: dual OUT RGBA8 only; never eng IN / virgin OUT (pain #6).
- Pure: SubmitLaw_PreferTextureKind/Allow*/Decide/HmdExpect/StatusLabel.
- cl_vrmod: collect via AllowCollect; _submitLaw decision snapshot.
- TESTING_FRAMEWORK §0.11 submit walk.
- G19 partial — product path already dual OUT; law offline-tested.
- Tests: --fast PASS 4/4 (pure pending=0).
- Next: dual-truth pose SoT law (pain #4) or HMD walk backlog.

## 2026-08-04 cycle 43 — G25 pose SoT single-path law

- Theme: one energy path raw→tracking→modifiers; no dual angvel/public SoT (pain #4).
- Pure: PoseSoT_Pipeline/Sources/Allow*/Decide/HmdExpect/StatusLabel.
- cl_vrmod: _poseSoT decision snapshot after ApplyPoseModifiers.
- TESTING_FRAMEWORK §0.12 pose walk.
- G25 partial — product path already single SoT; law offline-tested.
- Tests: --fast PASS 4/4 (pure pending=0).
- Next: HMD walk backlog (G05/G12) or remaining partial HmdExpects.

## 2026-08-04 cycle 44 — G26 menu thrash / QM dedupe law

- Theme: register once; dedupe by id/name; VRClimb aliases → vrclimb (pain #9).
- Pure: MenuLaw_Normalize/StableKey/ItemsMatch/DedupList/Decide/HmdExpect.
- cl_api: wire pure helpers + _menuLaw snapshot after DedupInGameMenuItems.
- TESTING_FRAMEWORK §0.13 menu walk.
- G26 partial — product already deduped; law offline-tested.
- Tests: --fast PASS 4/4 (pure pending=0; menu API tests still green).
- Next: engine blacklist never-call law (W2) or HMD walk backlog.

## 2026-08-04 cycle 45 — G27 engine blacklist never-call law

- Theme: never call GMod x64 Blocked_ConCommands / lifecycle bans (W2).
- Pure: EngineBlacklist_IsBlocked/Lifecycle/AllowWrite/FilterMap/Decide/HmdExpect.
- cl_vrmod: setConvarValue gate + EnsurePinned _engineBlacklist snapshot.
- TESTING_FRAMEWORK §0.14 console walk.
- G27 partial — product already skipped blocked; law offline-tested.
- Tests: --fast PASS 4/4 (pure pending=0).
- Note: preserved FOV crop commit 6aa0b36 (not thrash).
- Next: HMD walk backlog or soft handoff timeout notes.

## 2026-08-04 cycle 46 — G28 soft handoff timeout law

- Theme: soft 90s (GMod up) / hard 180s; never racey early release.
- Pure: CubeHandoffSoft/HardReleaseSeconds + Timeout_Decide/HmdExpect/StatusLabel.
- xr_app: release gate via pure Decide; log reason=…
- TESTING_FRAMEWORK §0.15 handoff timeout walk.
- G28 partial — product already used 90/180; law offline-tested.
- Tests: full test_all PASS 6/6 (launcher 29 tests).
- Next: HMD walk backlog (G05/G12) or supersample cold-start cap notes.

## 2026-08-04 cycle 47 — G29 supersample cold-start cap law

- Theme: cold cfg SS ≤1.4; never crank 1.75/2.0 at Start (soft care).
- Pure: CubeSs_ColdStartCap/ClampCold/Live/Ladder/Decide/HmdExpect.
- gmod_spawn cfg + launch_fill ladder wire pure helpers.
- TESTING_FRAMEWORK §0.16 SS walk.
- G29 partial — product already capped 1.4; law offline-tested.
- Tests: full test_all PASS 6/6 (launcher 30 tests).
- Next: HMD walk backlog (G05/G12) or FOV archive write-only-when-touched notes.

## 2026-08-04 cycle 48 — G30 FOV archive write-only-when-touched

- Theme: omit fovscale unless user touched SETTINGS FOV (Vision cal soft care).
- Pure: CubeFov_Default/Min/Max/Clamp/ShouldWrite/Decide/OmitComment/HmdExpect.
- gmod_spawn cfg + launch_fill wire pure helpers.
- TESTING_FRAMEWORK §0.17 FOV archive walk.
- G30 partial — product already omit-when-untouched; law offline-tested.
- Tests: full test_all PASS 6/6 (launcher 31 tests).
- Next: HMD walk backlog (G05/G12) or remaining partial HmdExpects.

## 2026-08-04 cycle 49 — G31 action-manifest self-heal + honest toast

- Theme: W6 force-rewrite DATA bindings; retry once; toast on fail; never abort VR.
- Pure: BindingsLaw_ForceRewrite/Retry/Abort/Toast/Decide/HmdExpect.
- cl_vrmod SetupActions wires pure law + snapshot.
- TESTING_FRAMEWORK §0.18 bindings walk; PURE_TESTED map.
- G31 partial — product already self-healed; law offline-tested.
- Tests: --fast PASS 4/4 (pure pending=0; 43 Lua).
- Next: HMD walk backlog (G05/G12) or W7 submit fail toast law notes.

## 2026-08-04 cycle 50 — G32 stereo ShareTexture / HMD self-test toast (W7)

- Theme: toast on share begin/finish fail; delayed no-HMD toast; never silent black.
- Pure: StereoSelfTest_* ShouldToast/ShareOk/Decide/HmdExpect.
- cl_vrmod share path + delayed selftest wire pure law + snapshot.
- TESTING_FRAMEWORK §0.19; PURE_TESTED map.
- G32 partial — product already toasted; law offline-tested.
- Tests: --fast PASS 4/4 (pure pending=0; 44 Lua).
- Next: HMD walk backlog (G05/G12) or remaining partial HmdExpects.

## 2026-08-04 cycle 51 — G33 swap-eyes content-only law (W4)

- Theme: SBS content L↔R only; IPD/FOV/pose single path; no dual pose fork.
- Pure: SwapEyesLaw_FromAny/ResolveSbsHalves/Decide/HmdExpect.
- cl_vrmod stereo eye write uses pure ResolveSbsHalves + snapshot.
- TESTING_FRAMEWORK §0.20; PURE_TESTED map.
- G33 partial — product already content-only swap; law offline-tested.
- Tests: --fast PASS 4/4 (pure pending=0; 45 Lua).
- Next: HMD walk backlog (G05/G12) or W12 fly-away / origin snap notes.

## 2026-08-04 cycle 52 — G34 fly-away origin snap + action set (W12)

- Theme: /actions/main before input; insane |vel_z|>1500 → one-shot origin to feet in 3s window.
- Pure: FlyAwayLaw_ResolveActionSet/ShouldSnapOrigin/Decide/HmdExpect.
- cl_vrmod SetupActions + SetupNetworkAndOrigin wire; deferred snap timer.
- Also pushed user FOV crop revert 3d24d92 (Quest stereo recovery).
- TESTING_FRAMEWORK §0.21; PURE_TESTED map.
- G34 partial — HMD walk open.
- Tests: --fast PASS 4/4 (pure pending=0; 46 Lua).
- Next: HMD walk backlog (G05/G12) or W8 fisheye / viewscale notes.

## 2026-08-05 cycle 53 — G35 viewscale fisheye law (W8)

- Theme: default 1.0; clamp 0.1..2.0; comfort 0.75..1.25 risk; prefer HMD projLive.
- Pure Lua ViewScaleLaw_* + launcher CubeViewScale_*; cl_vrmod + gmod_spawn wire.
- Also pushed user stereo tilt commit 423a178 (full tests green).
- TESTING_FRAMEWORK §0.22; PURE_TESTED map.
- G35 partial — HMD fisheye walk open.
- Tests: full test_all PASS 6/6 (launcher 32; 47 Lua).
- Next: HMD walk backlog (G05/G12) or W5 FOV/Z jitter notes.

## 2026-08-05 cycle 54 — G36 FOV/Z soft-refresh law (W5)

- Theme: no mid-frame UV+FOV fight; FOV→soft_display; borders→submit_bounds; znear→session.
- Pure: FovZLaw_RefreshKind/ClampFov/Decide/HmdExpect.
- cl_vrmod Bind*Callbacks + ComputeDisplayParams FOV clamp wire.
- TESTING_FRAMEWORK §0.23; PURE_TESTED map.
- G36 partial — HMD one-eye jitter walk open.
- Tests: --fast PASS 4/4 (pure pending=0; 48 Lua).
- Next: HMD walk backlog (G05/G12) or W9 hand bullet filter notes.

## 2026-08-05 cycle 55 — G37 hand vs bullet filter law (W9)

- Theme: hands Real for grabs; bullet redirect (hand 0.45 / head 10×); non-bullet absorb; never world solid.
- Pure: HandBulletLaw_IsBullet/RedirectScale/ShouldAbsorb/Decide/HmdExpect.
- sv_collision_proxies damage redirect wires pure law (no climb/wall thrash).
- TESTING_FRAMEWORK §0.24; PURE_TESTED map.
- G37 partial — HMD/MP bullet walk open.
- Tests: --fast PASS 4/4 (pure pending=0; 49 Lua).
- Next: HMD walk backlog (G05/G12) or W10 worldmodel single path notes.

## 2026-08-05 cycle 56 — G38 worldmodel single-path law (W10)

- Theme: one draw path floating hands OR worldmodel; dual ghost forbidden/sanitized.
- Pure: WorldModelLaw_ResolvePath/Sanitize/Decide/HmdExpect.
- cl_vrmod SetupModelAndPlayerHooks wires path + skip dual VM draw.
- TESTING_FRAMEWORK §0.25; PURE_TESTED map.
- G38 partial — HMD dual-ghost walk open.
- Tests: --fast PASS 4/4 (pure pending=0; 50 Lua).
- Next: HMD walk backlog (G05/G12) or W11 VR_Init error surface notes.

## 2026-08-05 cycle 57 — G39 VR_Init human error surface (W11)

- Theme: surface codes 108/215 + human toast; module zip link; never silent fail.
- Pure: InitLaw_ParseCode/Humanize/Decide/HmdExpect.
- cl_vrmod PerformStartup wires toast + overlay + snapshot.
- TESTING_FRAMEWORK §0.26; PURE_TESTED map.
- G39 partial — runtime fail walk open.
- Tests: --fast PASS 4/4 (pure pending=0; 51 Lua).
- Next: HMD walk backlog (G05/G12) or remaining partial HmdExpects inventory.

## 2026-08-05 cycle 58 — G40 Vision border fill law (W1)

- Theme: guided scale→V→H→save; defaults scale=1 offsets=0; no slider maze; soft FOV care.
- Pure: BorderLaw_GuideBaseline/Clamp*/IsBleedRisk/Decide/HmdExpect.
- cl_border_calibrate wires baseline + clamp + snapshot.
- TESTING_FRAMEWORK §0.27; PURE_TESTED map; CUBE_WATCHLIST W1 partial.
- G40 partial — HMD fill walk open.
- Tests: --fast PASS 4/4 (pure pending=0; 52 Lua).
- Next: HMD walk backlog (G05/G12) or remaining partial HmdExpects inventory.

## 2026-08-05 cycle 59 — G41–G44 parent pure offline gate + HmdWalk inventory

- Theme: recover pure-pending=0; wire product HmdWalk/HandStuck/NestedRt/GrabEnd into parent tests.
- PURE_TESTED + unit tests for G41 inventory, G42 hands, G43 nested RT, G44 grab_end.
- SubmitBounds_MirrorLeftToBoth pure-tested; G05 HmdExpect expects mq2_mono_both.
- TESTING_FRAMEWORK §0.28–0.31 notes; GLOGIC_GAPS inventory.
- Did not thrash quest module WIP (8939da7) or cubalc_mirror.
- Tests: --fast PASS 4/4 (pure pending=0; 56 Lua).
- Next: Walk G05/G12 on HMD; or primary-hand left smoke.

## 2026-08-05 cycle 60 — G45 primary-hand left SoT (LaserLaw_Decide)

- Theme: primary-left laser+click; wrong-hand steal risk; dual laser forbidden.
- Pure: LaserLaw_Decide/AllowLaserFromHand/IsWrongHandPrimaryClick/IsStealRisk.
- cl_ui menu laser path snapshots _laserLaw + HmdExpect.
- Align submit_bounds test with clampHalf (V offset still differs under auto).
- HmdWalk catalog G45 §0.32; TESTING_FRAMEWORK.
- vrmod-x64 pushed branch cube-stereo-g45 (main non-ff: remote has mq2 mono revert).
- Tests: --fast PASS 4/4 (pure pending=0; 56 Lua).
- Next: Walk G05/G12 or primary-left on HMD; resolve vrmod-x64 main vs cube-stereo-g45.

## 2026-08-05 cycle 61 — G46 desktop mirror vs HMD isolation

- Theme: finish WIP stereo black recovery; never sample live stereo RT for desktop 2/3.
- Pure: DesktopMirror_AllowPresent/Decide/HmdExpect (eye-crop hold; follow private RT).
- cl_vrmod PresentDesktopMirror wires pure law; C++ submit prefers stolenTexture + ordered V/dest flip.
- TESTING_FRAMEWORK §0.33; HmdWalk G46; full test_all 6/6.
- vrmod-x64 still on cube-stereo-g45 (origin/main non-ff mq2 revert).
- Tests: full test_all PASS 6/6 (pure pending=0; 57 Lua; module 68; launcher 33).
- Next: HMD walk G05/G46 with desktopview 1 then 2; resolve vrmod-x64 main carefully.

## 2026-08-05 cycle 62 — G46 realign + b1a5e9e restore honesty

- Theme: user restored mid-frame desktop NDC path; pure law tracks product truth.
- DesktopMirror: mid_live_rt legacy risk; post-submit live RT still forbidden.
- Soft NaN guards on ComputeDesktopCrop; mid-frame snapshot wire.
- Fix submit_bounds test for unclamped U span under H offset.
- Push product on cube-stereo-g45 (do not force main over mq2 mono revert).
- Tests: full test_all PASS 6/6 (pure pending=0; 57 Lua).
- Next: HMD walk desktopview 1 vs 2; merge cube-stereo-g45→main only with user OK.

## 2026-08-05 cycle 63 — G47 false per-eye FBO guard

- Theme: offline pure law for both-FBO dual; SBS fallback; no color+depth false dual.
- Pure: FalsePerEyeLaw_IsLegalPair/ResolvePath/Decide/HmdExpect.
- Mirrors xr_render Submit guard; did not touch quest WIP.
- TESTING_FRAMEWORK §0.34; HmdWalk G47; pure pending=0.
- Tests: --fast PASS 4/4 (pure pending=0; 58 Lua).
- Next: HMD walk both eyes; merge cube-stereo-g45→main only with user OK.
