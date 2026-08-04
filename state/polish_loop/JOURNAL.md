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
- Save cube_last_play.txt on successful Start; restore on WebUI_Init; QUICK PLAY button.
- No -noborder force; mat_queue untouched.
- Tests: full test_all PASS 6/6; cube_webui_launcher builds.
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
