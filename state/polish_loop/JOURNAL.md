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
