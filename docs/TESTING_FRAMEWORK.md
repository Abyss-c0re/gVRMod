# gVRMod Full Testing Framework

**Status:** design (implementation phased)  
**Scope:** Lua addon (`addon/vrmod-x64`), C++ module (`src/`), Cube native launcher (`native_launcher/`), shared OpenXR path helpers (`shared/`)  
**Goal:** every public util and API has automated coverage; tests survive refactors by locking **contracts**, not call graphs.

---

## 1. Why this exists

Today we have a thin offline C++ suite (`tests/`, `./test.sh`) and a manual in-game cycle (`./quick_test.sh`). Gaps:

| Gap | Risk |
|-----|------|
| Almost no Lua unit tests | Utils/API regressions (menu dedupe, math, collisions) only found in HMD |
| C++ tests only cover input / distortion / mock Lua push | Module surface (`VRMOD_*`) can drift silently |
| Launcher untested offline | Binding/path/panel math breaks only at Quest |
| Tests coupled to private helpers | Refactors break tests without product bugs |
| No inventory gate | New `vrmod.utils.*` / `vrmod.*` APIs ship untested |

This design unifies **three runners** under one contract catalog and one CI entrypoint.

---

## 2. Design principles (change-resilient)

1. **Test public contracts, not private shapes**  
   Call `vrmod.GetLeftHandPose`, `vrmod.utils.VecAlmostEqual`, `VRMOD_GetVersion` — never internals renamed next week.

2. **Behavior + schema, not line coverage vanity**  
   Each symbol is classified: pure / seam / integration. Only pure + seam get unit tests. Integration gets scenario tests.

3. **Table-driven fixtures**  
   Inputs/expected live in data tables or JSON goldens. Adding a case does not rewrite the test body.

4. **Stable IDs for cases**  
   `math.vec_almost_equal.near_true` not `test_3`. Renames keep history.

5. **Seams for engines**  
   GMod/OpenXR/GL are injected via fakes. Production code depends on small interfaces where pure logic is extracted (aligns with deep-research util modularization).

6. **Inventory is a test**  
   Generated catalog of `vrmod.*` / `vrmod.utils.*` / C exports must equal declared coverage map. New symbol without a row → CI fail (force classify + test or waive with reason).

7. **No HMD required for default PR gate**  
   Offline suite is mandatory. In-VR / Quest is optional nightly or `./quick_test.sh`.

8. **One command for the default gate**  
   `./test.sh` (or `./scripts/test_all.sh`) runs C++ + Lua + launcher unit tests.

---

## 3. Architecture

```
                    ┌─────────────────────────────┐
                    │  contracts/  (source of truth)│
                    │  api.yaml · utils.yaml · cpp  │
                    │  launcher.yaml · scenarios    │
                    └──────────────┬──────────────┘
                                   │ generates / validates
          ┌────────────────────────┼────────────────────────┐
          ▼                        ▼                        ▼
   ┌──────────────┐      ┌─────────────────┐      ┌──────────────────┐
   │ cpp_tests    │      │ lua_tests       │      │ launcher_tests   │
   │ (vrmod_tests)│      │ (bust / gmod    │      │ (cube_tests)     │
   │ CMake ON     │      │  headless mock) │      │ CMake target     │
   └──────┬───────┘      └────────┬────────┘      └────────┬─────────┘
          │                       │                        │
          └───────────────────────┼────────────────────────┘
                                  ▼
                         ./scripts/test_all.sh
                         (exit non-zero on any fail)
                                  │
                    optional ─────┴──── ./quick_test.sh (GMod + HMD)
```

### 3.1 Contract catalog

Path: `tests/contracts/`

```yaml
# tests/contracts/utils.yaml (example shape)
version: 1
symbols:
  - id: vrmod.utils.VecAlmostEqual
    file: lua/vrmod/utils/sh_math.lua
    tier: pure          # pure | seam | integration | untestable
    tests: [math.vec_almost_equal]
    notes: "Vector or table x/y/z"

  - id: vrmod.utils.UpdateHandCollisions
    tier: seam
    tests: [collisions.hand_wall.floor_ignored, collisions.hand_wall.climb_grip_skip]
    requires: [mock.trace, mock.g_VR]

  - id: vrmod.utils.DrawDeathAnimation
    tier: untestable
    waive: "render-only; covered by smoke scenario render_alive"
```

```yaml
# tests/contracts/api.yaml
symbols:
  - id: vrmod.GetLeftHandPose
    tier: seam
    tests: [api.pose.left_hand.defaults, api.pose.left_hand.from_tracking]
  - id: vrmod.AddInGameMenuItem
    tier: pure   # with table mocks for g_VR
    tests: [api.menu.add_dedupe_by_name, api.menu.add_dedupe_by_id]
```

```yaml
# tests/contracts/cpp_module.yaml
exports:
  - id: VRMOD_GetVersion
    tier: pure
  - id: VRMOD_KeyboardSetText
    tier: seam
```

Contract checker (`scripts/check_test_contracts.py` or Lua):

1. Parse addon for `function vrmod.` / `function vrmod.utils.`
2. Parse C++ for `LUA->SetField(..., "Name")` / registered globals
3. Diff vs YAML  
4. Fail if missing classification

### 3.2 Tiers

| Tier | Meaning | How tested |
|------|---------|------------|
| **pure** | No engine; math/string/table only | Offline unit, full cases |
| **seam** | Needs fakes (trace, entity, g_VR, OpenXR) | Unit with harness mocks |
| **integration** | Multi-system scenario | Scenario scripts + optional GMod |
| **untestable** | GPU-only / OS modal | Waive + optional smoke |

Deep research (modular utils) feeds **pure** extraction: pull pure cores out of seam files so tests never need the full GMod runtime.

---

## 4. Lua testing

### 4.1 Runner choice

**Primary (CI, no GMod):** lightweight harness under `tests/lua/`

- `tests/lua/harness.lua` — assert helpers mirroring C++ macros  
- `tests/lua/mock_gmod.lua` — Vector, Angle, Color, CurTime, hook, util.Trace*, CreateConVar stubs  
- `tests/lua/mock_vrmod.lua` — minimal `g_VR`, `vrmod` bootstrap  
- Runner: `luajit tests/lua/run.lua` (or `lua5.1` if available)

**Secondary (optional):** GMod dedicated server + `lua_openscript` for true engine types when offline mocks diverge.

Do **not** require Source engine for PR default.

### 4.2 Layout

```
tests/lua/
  harness.lua
  mock/
    gmod.lua
    vrmod_env.lua
    traces.lua
  unit/
    math_test.lua
    menu_dedupe_test.lua
    rendering_bounds_test.lua
    collisions_policy_test.lua   # floor ignore, climb grip skip (policy pure-ish)
    api_pose_test.lua
  fixtures/
    poses.json
    projection_matrices.json
  run.lua
```

### 4.3 Resilience patterns

```lua
-- GOOD: contract of public API
TEST("api.menu.add_dedupe_by_name", function()
  vrmod.AddInGameMenuItem("VRClimb", 5, 3, function() end)
  vrmod.AddInGameMenuItem("VRClimb", 5, 3, function() end) -- new func
  assert_eq(#g_VR.menuItems, 1)
end)

-- BAD: private local name inside sh_collisions.lua
TEST("broken", function()
  assert(IsFloorOrCeilingNormal) -- vanishes on rename
end)
```

For wall collision policy, either:

- export small pure helpers (`vrmod.utils.IsFloorOrCeilingNormal` after util extraction), or  
- test via `UpdateHandCollisions` with mocked `util.TraceHull` returning floor normals and assert **tracking pos unchanged**.

### 4.4 API matrix generation

`scripts/gen_api_smoke_tests.lua` reads `api.yaml` and emits:

- existence: `assert(isfunction(vrmod.GetLeftHandPose))`  
- nil-safe: call with no VR active → no error, documented defaults  

Existence tests are weak but prevent “API deleted” without contract update.

### 4.5 Shared pure cores (deep research + product bugs)

**Already central — test in place:** math (`sh_math`), frames, traces, collisions, projection/crop (`cl_rendering`), menu dedupe (`AddInGameMenuItem`).

**Extract then test (research order):** finger curl SoT, bone hierarchy apply, AutoScale/seated height, color parse adoption, unified smoothing, settings row-kind binder. Details in §10 Phase 3.

Tests import the pure util path; call sites become thin wrappers (behavior preserved).

---

## 5. C++ module testing

### 5.1 Keep and grow existing harness

- `tests/test_framework.h` — keep macros; add `ASSERT_THROW` / `TEST_F` fixture later if needed  
- `tests/mocks/mock_lua.h`, `mock_pose.h` — extend, do not replace  
- CMake: `VRMOD_BUILD_TESTS=ON` → `vrmod_tests`

### 5.2 Expand suites (by contract)

| Suite file | Covers |
|------------|--------|
| `test_input.cpp` | existing poses / actions |
| `test_distortion.cpp` | projection goldens |
| `test_frame_pipeline.cpp` | stereo order invariants |
| `test_lua.cpp` | Lua push protocols |
| **`test_vkeyboard.cpp`** (new) | SetText / Append / Open close state machine with mock |
| **`test_vdisplay.cpp`** (new) | size/create policy without real GL where possible |
| **`test_exports.cpp`** (new) | every registered `vrmod.X` field name listed in `cpp_module.yaml` exists in a registry table (compile-time or init-time map) |

### 5.3 Export registry (anti-drift)

In `lua_interface.cpp`, registration already pushes named fields. Add a parallel:

```cpp
// tests see this under VRMOD_TESTING
static const char* kVrmodExports[] = { "GetVersion", "KeyboardSetText", ... };
```

Or generate from a single `src/lua/exports.inc` included by production + tests.

### 5.4 Isolation

- `VRMOD_TESTING` / `VRMOD_DEV_BUILD` already used — keep fakes behind them  
- Never `dlopen` real OpenXR in unit tests; inject `XrFn` table mock  
- Link only `CORE_SOURCES` + tested units; avoid pulling full GL hooks into every binary if link cost hurts (optional split `vrmod_tests_core` / `vrmod_tests_render`)

---

## 6. Native launcher testing

### 6.1 Target

New CMake target in `native_launcher/CMakeLists.txt`:

- `cube_launcher_tests` executable  
- Same lightweight framework (share `tests/test_framework.h` via include path or copy thin header under `native_launcher/tests/`)

### 6.2 What is pure vs not

| Component | Tier | Approach |
|-----------|------|----------|
| `math3d.hpp` | pure | unit |
| `paths.cpp` | pure | temp dirs / env |
| `panel_config.cpp` | pure | fixture conf files |
| `bindings_mgr.cpp` | seam | JSON fixtures (Quest gold) |
| `maps_scan.cpp` | seam | fake filesystem tree |
| `xr_input` aim/hit UV | pure/seam | table-driven UV cases (180° upload, X mirror — golden bugs) |
| `gl_render` / real OpenXR | integration | optional; smoke with null GPU skip |
| `gmod_spawn` | integration | mock process spawn |

### 6.3 Layout

```
native_launcher/tests/
  test_main.cpp
  test_math3d.cpp
  test_paths.cpp
  test_bindings_json.cpp
  test_panel_hit.cpp
  fixtures/
    bindings_quest3_gold.json
    cube_webui_min.conf
```

Hit tests: feed NDC / laser ray → expect panel UV; lock cases that regressed (upside-down input, front face).

---

## 7. Scenario / integration layer

Path: `tests/scenarios/`

Declarative scripts (Lua or shell) for multi-step product paths:

| ID | Steps | Runner |
|----|-------|--------|
| `smoke.module_load` | require module, GetVersion | offline or GMod |
| `smoke.menu_dedupe` | double AddInGameMenuItem | offline Lua |
| `smoke.vr_start_cfg` | cfg keys present | file assert |
| `quest.bindings_gold` | JSON equals gold | offline |
| `ingame.vrmod_start` | quick_test.sh | manual/nightly |

Scenarios call **shared helpers** only (`tests/lib/`), never duplicate assert logic.

---

## 8. Shared test libraries (reuse)

```
tests/lib/
  cpp/          # optional common assert extensions
  lua/
    assert.lua
    fixtures.lua
    tracking_factory.lua   # builds g_VR.tracking / rawTracking
  fixtures/
    golden_projection/
    golden_bindings/
```

Rules:

- Production code may not include `tests/`  
- Tests may depend on production public headers / Lua public modules only  
- Factories build valid `g_VR` graphs so API tests stay short

---

## 9. CI and developer UX

| Command | What |
|---------|------|
| `./test.sh` | C++ module unit tests (existing) + **extend** to call `test_all` |
| `./scripts/test_all.sh` | contracts check + cpp + lua + launcher |
| `./test.sh --no-clean` | fast incremental |
| `./quick_test.sh` | in-game (not default CI) |
| `./scripts/check_test_contracts.py` | inventory vs YAML |

Exit codes: any fail → non-zero. Print per-suite summary.

Pre-commit optional: `scripts/test_all.sh --fast` (pure only).

---

## 10. Implementation phases

### Phase 0 — Scaffold (1–2 days)

- [x] `tests/contracts/{utils,api,cpp_module,launcher}.yaml` seeded from ripgrep inventory  
- [x] `scripts/check_test_contracts.py` + `gen_contracts.py`  
- [x] `tests/lua/harness.lua` + `run.lua` + mock GMod  
- [x] Wire `scripts/test_all.sh`  
- [x] Document in README + AGENTS.md (“run tests before push”)

### Phase 1 — Pure Lua + harden C++ (3–5 days)

- [x] Unit tests: `sh_math`, menu dedupe, rendering bounds, fingers, color, calib  
- [x] Contract rows generated for all symbols  
- [x] C++ export name list test (`test_exports.cpp`)  
- [x] Golden projection already in place + desktop view enum tests

### Phase 2 — Seams (1–2 weeks)

- [ ] Mock traces → collision policy tests  
- [ ] Pose API with factory `g_VR`  
- [ ] Keyboard C++ state machine  
- [ ] Launcher paths + bindings gold + panel hit UV

### Phase 3 — Extract pure cores (deep research order)

Deep research (2026-08-03, partial) confirmed utils load early as a shared table and ranked **near-duplicate** extractions. **Rule: write pure unit tests → extract util → rewire call sites → re-run contracts.**

| Priority | Extract | From (dup sites) | Into | Offline tests |
|----------|---------|------------------|------|---------------|
| 1 | Finger curl apply | `player/cl_character_hands`, `player/sh_character_fbt` (already partly in `utils/cl_character_ik`) | single `vrmod.utils.ApplyFingerCurl` / charik API | curl tables × fractions |
| 2 | Bone hierarchy pose apply | FBT/hands parent→child `LocalToWorld` + override + matrix | `vrmod.utils.ApplyBoneHierarchy` / frames | parent chain fixtures |
| 3 | AutoScaleHeight / AutoSeatedOffset | `ui/cl_heightadjust` + `ui/cl_avatar_menu` (66.8 eye math) | one `vrmod.utils` or `vrmod.calibration` SoT | table-driven heights |
| 4 | Color parse adoption | laser/beam re-`string.match` vs `SettingsParseColor` | force call sites → one parser (`vrmod.utils.ParseColor` or keep catalog) | RGBA strings / bad input |
| 5 | Unified smoothing | `cl_api` local SmoothValue; input/player FrameTime Lerps vs `SmoothVector`/`SmoothAngle` | export `vrmod.utils.SmoothValue` | number/vec/ang cases |
| 6 | Settings row-kind binder | Derma + Cube dual switch on catalog kinds | shared dispatch util (render stays toolkit-specific) | kind → handler map existence |

**Placement rule (from research):** new general helpers = flat `sh_`/`cl_` under `lua/vrmod/utils/` extending `vrmod.utils`; cohesive features may use `vrmod.<feature>` if they need early load (avatar/iknet style). Do **not** move `api` symbols under utils without load-order review (api loads **before** utils).

**Already in utils (contract-first, little extract):** math, frames, traces, collisions, pickup, weapons, vehicles, system hooks, projection/crop/FOV, npc2rag, plus namespaces charik/iknet/avatar/algocube, VirtualDisplay, GameUIProject, MapStart.

- [x] Pure cores landed in `sh_math.lua` (SmoothValue, ParseColor, fingers, calib, floor normal, settings kinds)  
- [x] SettingsParseColor/FormatColor delegate to utils  
- [x] Collisions use shared IsFloorOrCeilingNormal  
- [x] Offline unit coverage for pure cores  
- [ ] Rewire remaining call sites (laser/beam color, player finger loops, dual AutoScale UI) — incremental

### Phase 4 — Scenarios + optional GMod

- [x] Scenario runner (`tests/scenarios/run.lua`)  
- [x] Documented in README / AGENTS; `quick_test.sh` remains optional in-game  
- [x] Quest/media loop stays smoke-only (not unit gate)

---

## 10b. Deep research → test matrix (quick map)

| Research finding | Contract tier | First test IDs |
|------------------|---------------|----------------|
| Finger curl SoT | pure (after extract) | `util.fingers.curl_lerp_unit`, `util.fingers.digit_index` |
| Bone hierarchy apply | pure | `util.bones.hierarchy_child`, `util.bones.override_wins` |
| Dual AutoScale / seated | pure | `util.calib.autoscale_668`, `util.calib.seated_offset` |
| Settings row kinds | pure map + seam render | `util.settings.kinds_complete`, smoke Derma optional |
| Color parse | pure | `util.color.parse_rgba`, `util.color.parse_bad` |
| SmoothValue / Smooth* | pure | `util.smooth.number`, `util.smooth.vector` |
| Menu add dedupe | pure (done product-side) | `api.menu.dedupe_name`, `api.menu.dedupe_id` |
| Floor/ceiling wall policy | pure or seam | `util.collisions.floor_not_wall` |
| NetReceiveLimited / AddCallbackedConvar | seam (api) | rate-limit mock clock; cvar callback fire |
| Primary-hand helpers | seam | left/right primary flip |

---

## 11. What “every util / every API” means in practice

Not every function gets 50 cases. Every function gets **exactly one** of:

1. **Unit suite reference** in contracts YAML, or  
2. **Explicit waive** with reason and owning scenario, or  
3. **Exists + nil-safe smoke** for thin getters (`GetX` that only indexes `g_VR`)

Thin getters can share one generated suite:

```lua
for _, name in ipairs(POSE_GETTERS) do
  TEST("api.smoke." .. name, function()
    assert(isfunction(vrmod[name]))
    local ok = pcall(vrmod[name], nil)
    assert(ok) -- or assert documented error
  end)
end
```

---

## 12. Anti-patterns (do not)

- Snapshot entire `cl_vrmod.lua` PreRender as a test  
- Require Quest hardware in PR CI  
- Assert on log string wording as primary signal  
- Duplicate production math in the test file (import pure module)  
- Touch `vrmod_climbing` for gVRMod coverage (test gVRMod seams only)

---

## 13. Success metrics

| Metric | Target |
|--------|--------|
| Contract coverage of `vrmod.utils.*` | 100% classified |
| Contract coverage of `vrmod.*` API | 100% classified |
| Pure symbols with ≥1 behavior test | 100% |
| Seam symbols with ≥1 mock test | ≥80% year-one |
| `./scripts/test_all.sh` time offline | < 2 min typical |
| Launcher pure/seam tests | paths, bindings, hit UV green |

---

## 14. Relation to existing files

| Existing | Role going forward |
|----------|-------------------|
| `tests/test_framework.h` | C++ assert core (share with launcher) |
| `tests/test_*.cpp` | Module suites — keep, extend |
| `./test.sh` | Becomes thin wrapper around `test_all` or first stage |
| `./quick_test.sh` | Integration only |
| `build_tests/` | Unchanged CMake pattern; add launcher target similarly |

---

## 15. Next concrete steps (when implementing)

1. Land Phase 0 scaffold + contracts inventory from current tree (~239 `vrmod.*` functions, ~82 `vrmod.utils.*`).  
2. Port the menu-dedupe regression and climb/wall floor-policy as the first “bug→test” pair.  
3. Fold deep-research modular util list into Phase 3 extraction order.  
4. Do not expand in-game automation until offline gate is green.

---

*Document version: 1.0 — design only. Implementation tracks this file; update contracts when symbols change.*
