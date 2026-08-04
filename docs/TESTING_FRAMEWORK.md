# gVRMod Full Testing Framework

**Status:** offline gate **implemented** (contracts + Lua unit + C++ module + launcher); HMD smoke still **manual**  
**Scope:** Lua addon (`addon/vrmod-x64`), C++ module (`src/`), Cube native launcher (`native_launcher/`), shared OpenXR path helpers (`shared/`)  
**Goal:** every public util and API has automated coverage; tests survive refactors by locking **contracts**, not call graphs.

---

## 0. Ship bar — offline vs HMD smoke (G24)

Two layers. Do not conflate them in commits, PR claims, or polish-loop status.

### Offline gate (required before product push)

```bash
./scripts/test_all.sh          # contracts + Lua + C++ module + launcher
./scripts/test_all.sh --fast   # contracts + Lua only (Lua/docs/state cycles)
```

| Proves | Does **not** prove |
|--------|---------------------|
| Pure utils (math, color, fingers, experience gate, desktop crop/submit) | Real OpenXR session / WiVRn / Quest image |
| Contract inventory (every `vrmod.utils.*` / `vrmod.*` classified; pure pending = 0) | Stereo submit path in headset |
| C++ module unit (poses, distortion goldens, mat_queue law, exports) | Seamless Cube handoff feel in XR |
| Launcher pure helpers (handoff phase strings, last_play, math3d) | Panel laser hit accuracy on HMD |
| Offline Lua scenarios (menu dedupe, version smoke) | Loading/map after `take_xr`, cal continuity |

**Rule:** green offline is necessary to push product code. It is **not** “shipped Ideal VR.”

### HMD / in-game smoke (manual — no automation yet)

| Tool | Role |
|------|------|
| `./quick_test.sh` | Build + launch GMod; inject `vrmod_start`; needs desktop focus tools |
| Cube webui / `scripts/gvrmod_launcher.sh` | Product path: hold XR → GMod → `take_xr` |
| `scripts/quest_media_auto_review.sh` | Post-hoc Quest media review (smoke evidence, not a unit gate) |

**Minimum headset checklist** (claim “smoke OK” only if walked):

1. **Boot** — Cube panel or openxr launch → tracking valid (not origin-stuck).  
2. **Handoff** — Start Game → phase labels progress → GMod claims XR without long black void.  
3. **Stereo** — both eyes clear; no eng-IN submit / virgin OUT flash.  
4. **G05 stereo-load** — see **§0.1** below (load-flash walk).  
5. **UI** — laser + trigger click menus; no menu id thrash.  
6. **Desktop** — `vrmod_desktopview` 1/2/3/4 (follow-cam) as intended; mirror is secondary.  
7. **Cal** — Experience once (or skip forever); border profile reload on later boots.  
8. **Pain points** — no force `-noborder`; `mat_queue_mode` stays 1; climb/wall not thrashing.  
9. **Ambient** — soft hold tone during Cube handoff when `cube_hold.ogg` is present (default ON; silence with `GVRMOD_AMBIENT_PLAY=0`). Optional taste: `GVRMOD_AMBIENT_MASTER=0.35…0.75` (default **0.55**). See **§0.2**.

### 0.1 G05 HMD load-flash walk (manual)

Offline gate proves pure `StereoLoad_*` helpers only. **Headset** still required to claim load-flash OK.

| When | Expect in HMD | Offline label / toast | FAIL if |
|------|---------------|------------------------|---------|
| After `take_xr`, map still loading / LocalPlayer not ready | Both eyes content (may dim/black **pair**, not flat mono) | toast once: “dual-eye hold through load”; label `STEREO · DUAL HOLD LOAD`; log `G05 HMD · DUAL HOLD LOAD …` | One eye only, desktop-flat void, long virgin black wall |
| Live play, mq=1 | Clear stereo L/R | label `STEREO · DUAL` | eng-IN submit flash; mono mirror only |
| If mq ever ≥2 (must not ship) | Single-pass; right intentional clear | label `STEREO · MQ2 SINGLE` — **never dual-paint** | Dual forced under mq2 (CThread risk) |
| VR inactive | No submit | skip G05 row | Unexpected stereo thrash |

**Procedure (claim “G05 HMD OK” only if walked):**

1. Start Cube → Start Game → watch handoff phases → `take_xr`.  
2. During first map paint / any changelevel: both eyes remain a stereo pair (black pair OK; mono void FAIL).  
3. Confirm one toast at most for dual-hold; `mat_queue_mode` stays **1**.  
4. After spawn: clear dual stereo; laser UI still works.  
5. Optional: in-game `lua_run print(g_VR._stereoLoadLabel)` / `g_VR._stereoLoadHmdExpect` during load.

Pure helper: `vrmod.utils.StereoLoad_HmdExpect(policy)` → `checklist` / `pass_line` / `fail_line` / `flash_risk` (unit-tested; does **not** prove headset).

### 0.2 G12 HMD ambient volume taste (manual)

Offline gate proves gain law + PlayerDecide + master env. **Headset** (or desktop speakers during handoff) still required to claim taste OK.

| Control | Meaning |
|---------|---------|
| (unset) | Comfort master **0.55** — soft present, not harsh |
| `GVRMOD_AMBIENT_MASTER=0.35` | Softer hold tone |
| `GVRMOD_AMBIENT_MASTER=0.75` | More present (still clamped ≤1) |
| `GVRMOD_AMBIENT_PLAY=0` | Full silence (opt-out) |

| When | Expect | FAIL if |
|------|--------|---------|
| Cube holding XR / STARTING GMOD | Soft looped hold tone if `cube_hold.ogg` present | Silent with asset present; ear-splitting; desktop speakers blast |
| `take_xr` | Gain ducks / tone recedes | Tone stays full loud into VR |
| After release / panel live | Silent | Bleed after handoff ends |

**Procedure (claim “G12 volume OK” only if walked):**

1. Default master: Start Game once — tone present and soft.  
2. Optional: restart with `GVRMOD_AMBIENT_MASTER=0.35` then `0.75` — relative taste changes.  
3. `GVRMOD_AMBIENT_PLAY=0` — confirmed silence.  
4. Note band labels: soft / gentle / comfort / present (`CubeAmbient_TasteBand`).

Pure helper: `CubeAmbient_HmdVolumeExpect(master, gain, playing, enabled, clipPresent)` → `checklist` (unit-tested; offline ≠ HMD OK).

### 0.3 G13 return-to-Cube / reclaim (manual)

Offline gate proves soft ack + pure XR plan (panel refresh). **Headset** required for reclaim feel.

| Mode | Env | Expect | FAIL if |
|------|-----|--------|---------|
| Soft ack (default) | unset | After VR exit: RETURN banner ~2.5s → `panel_live`; Cube keeps XR session | Stuck RETURN; XR dies; need hard relaunch |
| Panel refresh | `GVRMOD_CUBE_RECLAIM=1` | Same marker ack + status `RECLAIM · PANEL REFRESH`; **no** second OpenXR session | Session recreate thrash; black void |
| Action rebind | n/a offline | Deferred forever until HMD-proven | Force rebind without proof |

**Procedure (claim “G13 reclaim OK” only if walked):**

1. Cube Start → enter GMod VR → exit VR / return path that writes `cube_return.txt`.  
2. Default: RETURN chrome briefly, then panel usable.  
3. Optional: `GVRMOD_CUBE_RECLAIM=1` — log shows XR plan `panel_refresh`; session never restarts.  
4. Confirm laser/UI still works on Cube panel after return.

Pure helpers: `CubeReclaimXrPlanDecide` / `CubeReclaim_HmdExpect` (unit-tested; offline ≠ HMD OK).

### 0.4 G03 STAGE pack / height continuity (manual)

Offline gate proves parse + deferred plan + opt-in executor. **Headset** required to claim height continuity OK.

| Mode | How | Expect | FAIL if |
|------|-----|--------|---------|
| Default | no convar/file | Preview toast only; **no** seated jump | Silent auto `vrmod_seatedoffset` |
| Close | pack head ≈ live HMD | “already close”; height unchanged | Rewrite thrash |
| Too far | ΔY outside safe band | Blocked; no apply | Forced multi-meter pop |
| Opt-in apply | `vrmod_stage_apply 1` or `DATA/vrmod/stage_apply_enable.txt` | Soft seated step; continuous floor | Ceiling crush / dual height truth |

**Procedure (claim “G03 stage OK” only if walked):**

1. Cube Start with STAGE pack written → enter VR.  
2. Default: toast may show deferred ΔY; standing height stable.  
3. Optional opt-in: enable apply once — soft continuity, not a jump.  
4. Log may show `G03 HMD · DEFERRED|APPLIED|CLOSE|BLOCKED …` (`g_VR._cubeStagePackHmdExpect`).

Pure helper: `vrmod.utils.StagePack_HmdExpect(decision, plan, execRes)` (unit-tested; offline ≠ HMD OK).

### 0.5 G14 Glide vehicle input (manual)

Offline gate proves stick-prefer SoT + HmdExpect tokens. **Headset + Glide addon** required to claim drive OK.

| When | Expect | FAIL if |
|------|--------|---------|
| Enter Glide as driver | Toast: thumbstick drives, wheel optional; log `G14 HMD · DRIVER · stick SoT` | Silent enter; no toast |
| Stick steer | Vehicle turns with thumbstick (SoT) | Stick ignored; only wheel works |
| Wheel assist | Only when stick near deadzone | Wheel fights stick (dual SoT) |
| Passenger seat | No drive nets | Passenger steers |
| Unbound `/actions/driving` | Error toast about rebind | Silent dead car |
| Lights / audio | Stereo lights; engine sound tracks HMD | Mono light only one eye; silent engine |

**Procedure (claim “G14 Glide OK” only if walked):**

1. VR enter Glide car as driver — toast + checklist log.  
2. Stick left/right + throttle — responsive steer.  
3. Grip wheel with stick idle — assist only; move stick — stick wins.  
4. Exit; passenger seat if available — no drive.  
5. Optional: `print(g_VR._glideHmdExpect)` / `g_VR._glideSteerSource`.

Pure helpers: `Glide_HmdExpect` / `GlidePreferStickSteer` (unit-tested; offline ≠ HMD OK).

### 0.6 G04 warm process / changelevel (manual)

Offline gate proves skip-spawn plan + attach decide + opt-in changelevel. **Headset** required to claim warm Start OK.

| Mode | How | Expect | FAIL if |
|------|-----|--------|---------|
| Default cold | env unset | Full Steam/hl2 spawn; handoff phases normal | Accidental skip-spawn without env |
| Warm skip | `GVRMOD_WARM_REUSE=1`, process up | Skip Steam; phase warm_attach; markers | Second Steam spawn thrash |
| Same map | cube_warm wants current map | SAME MAP toast; no changelevel | Forced reload |
| Map mismatch default | warm want other map | DEFERRED toast only | Silent RCC changelevel |
| Opt-in changelevel | `vrmod_warm_changelevel 1` or enable file / `GVRMOD_WARM_CHANGELEVEL=1` | One changelevel; stereo through load (G05) | Map thrash loop / XR death |

**Procedure (claim “G04 warm OK” only if walked):**

1. Cold Start once (baseline).  
2. Leave GMod up; Cube Start with `GVRMOD_WARM_REUSE=1` — no second Steam.  
3. Default mismatch: deferred toast; stay on map.  
4. Optional opt-in changelevel once — dual-hold load; XR stays.  
5. Log may show `G04 HMD · DEFERRED|SAME MAP|CHANGELEVEL …` (`g_VR._cubeWarmHmdExpect`).

Pure helpers: `WarmAttach_HmdExpect` / `CubeWarm_HmdExpect` (unit-tested; offline ≠ HMD OK).

### 0.7 G15 HUD additive / PROPHECY (manual)

Offline gate proves composite law (`HudLaw_Decide`). **Headset** required to claim no black wall.

| Mode | Setting | Expect | FAIL if |
|------|---------|--------|---------|
| Default clear plate | `vrmod_hudtestalpha 0` | Translucent; vitals float; world fully visible | Solid black HUD rectangle |
| Dim plate | `vrmod_hudtestalpha` 80–160 | **Additive** composite; black adds no light | Opaque black slab occludes world |
| Forbidden | opaque UnlitGeneric | Must not ship | Black wall of the Real |

**Procedure (claim “G15 HUD OK” only if walked):**

1. VR start; look past HUD vitals — Real (world) still visible.  
2. Optional dim: set `vrmod_hudtestalpha 120` — plate soft, not a wall; log `HUD · ADDITIVE`.  
3. Reset alpha 0.  
4. Optional: `print(g_VR._hudLawLabel)` / `g_VR._hudLaw`.

Pure helpers: `vrmod.utils.HudLaw_Decide` / `HudLaw_HmdExpect` (unit-tested; offline ≠ HMD OK).

### 0.8 G16 laser + primary click (manual)

Offline gate proves primary-hand SoT + click action map + first-eye focus solve. **Headset** required to claim UI OK.

| Check | Expect | FAIL if |
|-------|--------|---------|
| Laser | Primary hand only (default right) | Dual free-for-all lasers |
| Hover | Stable focus; second eye reuses solve | Per-eye thrash / hover flicker |
| Trigger | Primary fire = LMB click once | Miss / double / wrong hand |
| Secondary | Secondary fire closes / RMB | Grab_end storms |
| Left primary | `vrmod_primary_hand 1` → left laser+click | Right still steals click |

**Procedure (claim “G16 laser OK” only if walked):**

1. Open Cube hub / settings — laser points from primary hand.  
2. Hover buttons stable both eyes.  
3. Trigger click opens/activates once.  
4. Optional left-hand primary — laser+click follow.  
5. Secondary closes menus cleanly.

Pure helpers: `LaserLaw_IsMenuPrimaryClick` / `LaserLaw_ShouldSolveFocus` / `LaserLaw_HmdExpect` (unit-tested; offline ≠ HMD OK).

### 0.9 G17 mat_queue pin (manual)

Offline gate proves pin prefer **1**, dual only when mq&lt;2, **never write** from VR. **Headset** optional for feel; console check is enough for write law.

| Check | Expect | FAIL if |
|-------|--------|---------|
| Live prefer | `mat_queue_mode` **1** during VR (user/console before map) | Product SetInt mq mid-session |
| Dual stereo | Legal under mq 0/1 | Dual nested RenderView under mq **2** |
| mq2 (must not ship as default) | Single-pass only if user forced 2 | Dual under 2 → CThread risk |
| VR exit | No thrash rewrite of mq | Exit restores thrash / worker death |

**Procedure (claim “G17 mq OK” only if walked):**

1. Before map: `mat_queue_mode 1` (or leave default).  
2. Enter VR — dual stereo; no console spam about mq writes.  
3. Optional: `print(g_VR._matQueueLabel)` → `MQ · LIVE 1 · PIN`.  
4. Never set mq 2 “for perf” during polish.

Pure helpers: `MatQueueLaw_Decide` / `MatQueueLaw_HmdExpect` (unit-tested; offline ≠ HMD OK).

### 0.10 G18 framed window chrome (manual)

Offline gate proves Cube pin **windowed + framed** (`noborder=false`), never invent `-noborder` from missing keys, BuildArgs omit `-noborder` by default. **Desktop** observation is enough for chrome law; HMD still product surface.

| Check | Expect | FAIL if |
|-------|--------|---------|
| Cold Start / shell launch | GMod desktop mirror has **title bar** (framed) | Forced `-noborder` / borderless slab |
| Args | `-windowed -w … -h …` without `-noborder` | Product injects `-noborder` by default |
| last_play missing `noborder=` | Stays framed (`false`) | Borderless invented from corrupt snapshot |
| User opt-in | Settings/last_play `noborder=1` may borderless | Forced true when key absent |

**Procedure (claim “G18 chrome OK” only if walked):**

1. Cube Start Game (defaults) — desktop GMod window shows title chrome; movable.  
2. Confirm launch log / cmdline has no forced `-noborder`.  
3. Optional: set borderless in SETTINGS once, restart — opt-in only.  
4. Optional: delete `noborder=` from last_play — reloads framed.

Pure helpers: `WindowChrome_Decide` / `WindowChrome_BuildArgs` / `WindowChrome_HmdExpect` (unit-tested; offline ≠ desktop walk).

### 0.11 G19 submit path — dual OUT only (manual)

Offline gate proves **never eng IN**, **never virgin OUT**, prefer `dual_out_rgba8`, collect only when mq&lt;2. **Headset** required to claim no flash.

| Check | Expect | FAIL if |
|-------|--------|---------|
| Live VR mq=1 | Submit dual OUT after paint (+ optional Collect blit) | eng RT id submitted; one-eye trash |
| Cold first frames | No virgin black flash both eyes | Empty OUT before blit |
| mq≥2 (must not ship default) | No live Collect; still OUT path if present | Dual live blit under mq2 |
| Debug | `g_VR._submitLawLabel` → `SUBMIT · BLIT OUT` / `DUAL OUT` | `FORBID ENG IN` |

**Procedure (claim “G19 submit OK” only if walked):**

1. Enter VR — both eyes clear; no first-frame virgin wall.  
2. Optional: `print(g_VR._submitLawLabel, g_VR._submitLawHmdExpect and g_VR._submitLawHmdExpect.checklist)`.  
3. Confirm stereo stays dual OUT through load (G05) without eng-IN flash.

Pure helpers: `SubmitLaw_Decide` / `SubmitLaw_HmdExpect` (unit-tested; offline ≠ HMD OK).

### 0.12 G25 pose SoT — single energy path (manual)

Offline gate proves **one path** raw → tracking → modifiers; guns read **tracking**; no second angvel/public pose fork. **Headset** required to claim hands/guns feel locked.

| Check | Expect | FAIL if |
|-------|--------|---------|
| Live VR | Hands + gun follow one tracking SoT after collisions/modifiers | Gun glued to raw while hands blocked (dual truth) |
| Head motion | Head vel from device raw sample once | Competing angvel stream jitter |
| Debug | `g_VR._poseSoTLabel` → `POSE · SINGLE PATH` | `ANGVEL FORK` / `GUN FORK` / `DUAL PUBLIC` |
| Pipeline | WaitGetPoses → raw → copy → modifiers → consumers | Second pose table for same limb |

**Procedure (claim “G25 pose OK” only if walked):**

1. Enter VR — hands track cleanly; gun stays on right hand through wall block.  
2. Optional: `print(g_VR._poseSoTLabel, g_VR._poseSoTHmdExpect and g_VR._poseSoTHmdExpect.checklist)`.  
3. Move quickly — no desync between laser origin and hand mesh from dual SoT.

Pure helpers: `PoseSoT_Decide` / `PoseSoT_HmdExpect` (unit-tested; offline ≠ HMD OK).

### 0.13 G26 menu thrash / QM dedupe (manual)

Offline gate proves stable **id/name** keys, climb aliases → `vrclimb`, DedupList drops dupes. **Headset** required to claim one VRClimb tile.

| Check | Expect | FAIL if |
|-------|--------|---------|
| Open Quick Menu | One button per layout id | Duplicate tiles (esp. VRClimb ×2) |
| Re-Start VR | Update-in-place; no second register | Thrash grow every Start |
| Debug | After dedupe: `g_VR._menuLawLabel` → `MENU · DEDUPED` | `CLIMB DUPE` / `THRASH` sticky |
| Climb names | “VRClimb” / “VR Climb” / “VR Climbing” same id | Three climb entries |

**Procedure (claim “G26 menu OK” only if walked):**

1. Enter VR → open Quick Menu — scan for duplicate labels.  
2. Exit/re-enter VR once — count stays stable.  
3. Optional: `print(g_VR._menuLawLabel, #g_VR.menuItems)`.

Pure helpers: `MenuLaw_ItemsMatch` / `MenuLaw_Decide` / `MenuLaw_HmdExpect` (unit-tested; offline ≠ HMD OK).

### 0.14 G27 engine blacklist never-call (manual)

Offline gate proves **blocked** names (`viewmodel_fov`, `r_shadowrendertotexture`, `mat_reduceparticles`) and **lifecycle** bans (`mat_queue_mode`, `gmod_mcore_test`) refuse write. **Console** walk is enough; HMD optional.

| Check | Expect | FAIL if |
|-------|--------|---------|
| Enter VR | No `Command is blocked!` spam | Product Set/RCC blocked cvars |
| PERFORMANCE pins | No blocked keys in pin map | viewmodel_fov / shadow RT in pin list |
| mat_queue | VR never SetInt (G17 + G27 lifecycle) | CThread thrash from mq write |
| Debug | `g_VR._engineBlacklistLabel` → `ENG · CLEAN` | `BLOCKED SPAM` / `LIFECYCLE BAN` |

**Procedure (claim “G27 engine OK” only if walked):**

1. Start VR — watch developer console for blocked spam.  
2. Optional: `print(g_VR._engineBlacklistLabel)`.  
3. Confirm no viewmodel_fov thrash (VR draws own VM path).

Pure helpers: `EngineBlacklist_AllowWrite` / `EngineBlacklist_Decide` / `EngineBlacklist_HmdExpect` (unit-tested; offline ≠ console walk).

### 0.15 G28 soft handoff timeout (manual)

Offline gate proves **soft 90s** (GMod up, no take_xr), **hard 180s** ceiling, **no racey** early release without process. **Headset** required to claim seamless hold feel.

| Check | Expect | FAIL if |
|-------|--------|---------|
| Cold Start | Panel holds XR through Steam/hl2 boot | Session drops at ~40s void |
| take_xr signal | Orderly release soon after claim | Mid-frame destroy |
| GMod up, no signal | Soft release after **>90s** | Soft before 90s / without process |
| Stuck forever | Hard release after **>180s** | Infinite hold / hard kill |
| Log | `reason=phase_take_xr` / `soft_90s…` / `hard_180s…` | Silent race |

**Procedure (claim “G28 handoff timeout OK” only if walked):**

1. Cube Start Game — stay in headset; phases progress; no early void.  
2. Normal path: take_xr → fade → GMod owns XR.  
3. Optional stuck test: kill GMod mid-boot — hard ceiling ~180s orderly exit.

Pure helpers: `CubeHandoffTimeout_Decide` / `CubeHandoffTimeout_HmdExpect` (unit-tested; offline ≠ HMD OK).

### 0.16 G29 supersample cold-start cap (manual)

Offline gate proves cold cfg **≤ 1.4**, live ladder up to **2.0**, default Cube high **1.5** (capped to 1.4 on Start). **Headset** for hitch feel.

| Check | Expect | FAIL if |
|-------|--------|---------|
| Cold Start Ultra preset | `gvrmod_cube.cfg` has `vrmod_supersample` ≤ **1.4** | 1.75/2.0 injected at boot |
| High preset (1.5) | Cap to 1.4 on Start | Full 1.5 thrash first frames |
| After VR live | User may raise SS in SETTINGS | Forced permanent 1.4 forever without user |
| Ladder | idx 0..5 → 0.75…2.0 | Out-of-range crash |

**Procedure (claim “G29 SS OK” only if walked):**

1. SETTINGS Ultra → Start Game — first stereo frames without huge hitch.  
2. Inspect `garrysmod/cfg/gvrmod_cube.cfg` supersample line ≤ 1.4.  
3. Optional: raise SS in-game after stable — live path only.

Pure helpers: `CubeSs_ClampColdStart` / `CubeSs_Decide` / `CubeSs_HmdExpect` (unit-tested; offline ≠ HMD OK).

### 0.17 G30 FOV archive write-only-when-touched (manual)

Offline gate proves cold cfg **omits** `vrmod_fovscale_x/y` unless user touched SETTINGS FOV; write path clamps **0.1..2.0**. **Headset** for Vision cal survival.

| Check | Expect | FAIL if |
|-------|--------|---------|
| Start without FOV edit | `gvrmod_cube.cfg` has omit comment, no fovscale lines | Default 1.0/1.0 clobbers Vision archive |
| SETTINGS FOV cycled then Start | cfg writes both axes at chosen scales | Touched FOV ignored |
| Extreme values | Clamped to 0.1..2.0 | Out-of-range crash / silent 0 |
| After Vision calibrate | Untouched Start keeps asymmetric cal | Linked rewrite overwrites cal |

**Procedure (claim “G30 FOV archive OK” only if walked):**

1. Run Vision/border cal; note fovscale x≠y if asymmetric.  
2. Cube Start without touching XR FOV — cfg omits fovscale; HMD FOV matches cal.  
3. SETTINGS cycle FOV scale → Start — cfg writes scales; HMD reflects user choice.

Pure helpers: `CubeFov_ShouldWrite` / `CubeFov_Decide` / `CubeFov_HmdExpect` (unit-tested; offline ≠ HMD OK).

### 0.18 G31 action-manifest self-heal + honest toast (manual)

Offline gate proves force-rewrite on start, one retry, **never abort VR**, toast required on fail. **Headset / SteamVR** for real SetActionManifest.

| Check | Expect | FAIL if |
|-------|--------|---------|
| Cold VR start | DATA `vrmod_action_manifest.txt` rewritten; SetActionManifest ok | Stale/corrupt DATA left; silent fail |
| First set fails | Rewrite again + one retry | Infinite rewrite loop or no retry |
| Still fails | Crimson toast + overlay; VR session continues | Abort VR start or silent death |
| Controllers | Actions live when set ok | Inputs dead with no toast |

**Procedure (claim “G31 bindings OK” only if walked):**

1. Delete/corrupt `garrysmod/data/vrmod/vrmod_action_manifest.txt` → Start VR — self-heal or honest toast.  
2. Normal start — no bindings toast; controllers work.  
3. Fail path (missing module path) — toast visible; HMD still renders.

Pure helpers: `BindingsLaw_Decide` / `BindingsLaw_HmdExpect` (unit-tested; offline ≠ HMD OK).

### 0.19 G32 stereo ShareTexture / HMD self-test toast (manual)

Offline gate proves ShareTexture begin/finish fail **must toast**, delayed (~2.5s) no-HMD toast, unhealthy-share hint, **never abort VR**. **Headset** for real black-HMD cases.

| Check | Expect | FAIL if |
|-------|--------|---------|
| ShareTexture begin fail | Error toast with RT size | Silent black HMD |
| ShareTexture finish fail | Error toast | Desktop OK / HMD black no toast |
| No HMD pose ~2.5s | Error toast once | Silent loading forever |
| Share unhealthy + pose | Soft hint toast | No guidance |
| Fail paths | VR session continues | Abort start |

**Procedure (claim “G32 stereo self-test OK” only if walked):**

1. Normal Start — both eyes; no share/HMD error toast.  
2. Optional: induce share fail (bad SS / module) — toast visible; desktop may still show.  
3. Optional: unplug/runtime kill mid-start — no-HMD toast; restart SteamVR path clear.

Pure helpers: `StereoSelfTest_Decide` / `StereoSelfTest_HmdExpect` (unit-tested; offline ≠ HMD OK).

### 0.20 G33 swap-eyes content-only (manual)

Offline gate proves default **off**, SBS half resolve L↔R on toggle, **no dual pose / IPD / FOV fork**. **Headset** for inverted stereo (PSVR2 etc.).

| Check | Expect | FAIL if |
|-------|--------|---------|
| Default `vrmod_swap_eyes 0` | Natural stereo; left content → left half | Crossed / double vision at default |
| Toggle 1 live | Content swaps halves; IPD/depth feel OK | Tracking thrash or FOV swapped per-eye wrong |
| Pose path | Still single raw→tracking→modifiers | Second eye pose stream |
| Cube Vision toggle | Live while VR active | Requires full restart only |

**Procedure (claim “G33 swap eyes OK” only if walked):**

1. Start VR default — stereo natural.  
2. Settings Swap eyes on — inverted fixed without world jump.  
3. Off again — returns natural.

Pure helpers: `SwapEyesLaw_ResolveSbsHalves` / `SwapEyesLaw_Decide` / `SwapEyesLaw_HmdExpect` (unit-tested; offline ≠ HMD OK).

### 0.21 G34 fly-away origin snap + action set (manual)

Offline gate proves default `/actions/main` (driving in vehicle), insane |vel_z| **> 1500** → one-shot origin snap to feet within **3s** start window. **Headset** for real fly-away.

| Check | Expect | FAIL if |
|-------|--------|---------|
| Cold VR start | Origin at player feet; main actions live | Spawn in sky / dead sticks |
| Insane head vel early | One origin snap; log G34 | Keep flying or every-frame thrash |
| After 3s window | No automatic snap | Late origin thrash |
| Vehicle enter | `/actions/driving` active | Main set stuck; dead drive input |

**Procedure (claim “G34 fly-away OK” only if walked):**

1. Start VR standing — feet stable; laser/stick work first frames.  
2. Optional: induce tracking glitch — at most one snap, then stable.  
3. Enter vehicle — driving action set; stick drives.

Pure helpers: `FlyAwayLaw_ShouldSnapOrigin` / `FlyAwayLaw_Decide` / `FlyAwayLaw_HmdExpect` (unit-tested; offline ≠ HMD OK).

### 0.22 G35 viewscale fisheye law (manual)

Offline gate proves default **1.0**, clamp **0.1..2.0**, comfort **0.75..1.25** risk flag; prefer **HMD projLive**. **Headset** for real fisheye.

| Check | Expect | FAIL if |
|-------|--------|---------|
| Default viewscale 1.0 | Natural edges | Fisheye at default |
| Extreme 0.1 / 2.0 | Clamped; comfort risk flagged | Crash / unbounded warp |
| Cold Start cfg | `vrmod_viewscale` clamped | Wild scale injected |
| Identity projection | Soft-refresh until projLive | Borders stuck forever |

**Procedure (claim “G35 viewscale OK” only if walked):**

1. Vision defaults / viewscale 1.0 — natural stereo.  
2. Drag viewscale low then high — fisheye/tunnel expected; reset restores.  
3. Cold Start after extreme SETTINGS — cfg clamped.

Pure helpers: `ViewScaleLaw_Clamp` / `CubeViewScale_Decide` / `ViewScaleLaw_HmdExpect` (unit-tested; offline ≠ HMD OK).

### 0.23 G36 FOV/Z soft-refresh (manual)

Offline gate proves FOV cvars → **soft_display**, borders → **submit_bounds**, znear → **session**; **no mid-frame UV+FOV fight**; FOV clamp **0.1..2.0**. **Headset** for one-eye jitter.

| Check | Expect | FAIL if |
|-------|--------|---------|
| Live FOV scale change | SoftRefresh only; both eyes stable | Right eye wrap / freeze |
| Border H/V/scale | Submit bounds only | FOV recomputed mid-eye |
| Znear change | Session view.znear; no UV fight | Jitter / freeze on Z spam |
| Extreme FOV | Clamped; prefer Border guide | Unbounded wrap |

**Procedure (claim “G36 FOV/Z OK” only if walked):**

1. VR active — nudge FOV X/Y slowly; both eyes stable.  
2. Border offsets live — no freeze.  
3. Avoid Z spam; use Border calibrate for edges.

Pure helpers: `FovZLaw_RefreshKind` / `FovZLaw_Decide` / `FovZLaw_HmdExpect` (unit-tested; offline ≠ HMD OK).

### 0.24 G37 hand vs bullet filter (manual)

Offline gate proves hand damage scale **0.45**, head **10×**, non-bullet absorb, **no world solid**, grab allowed. **Headset/MP** for real bullet pass.

| Check | Expect | FAIL if |
|-------|--------|---------|
| Grab prop with hand | Physical grab contact | Hands ghost / non-physical |
| Bullet hits hand proxy | ~45% player dmg + drop | Silent block; no feedback |
| Bullet hits head proxy | High damage redirect | No head hit registration |
| Self melee on hand | Absorbed | Self-punch thrash |
| Proxy vs world | No solid wall fight | Sparks / climb thrash |

**Procedure (claim “G37 hand-bullet OK” only if walked):**

1. Grab prop — hands feel Real.  
2. Get shot in hand (or shoot near) — damage + drop path.  
3. Hands do not solid-block world walls (climb/wall path separate).

Pure helpers: `HandBulletLaw_Decide` / `HandBulletLaw_HmdExpect` (unit-tested; offline ≠ HMD OK).  
**Do not** rewrite climb/wall coll while verifying.

### 0.25 G38 worldmodel single path (manual)

Offline gate proves **one** path: floating_hands | worldmodel | player_body; dual ghost sanitized. **Headset** for double-draw ghosts.

| Check | Expect | FAIL if |
|-------|--------|---------|
| Floating hands on | Body hidden; clear arms | Body + arms ghosted |
| Worldmodel weapon path | WorldModelVM only; no HL2 VM | VM + worldmodel both |
| Toggle Session | Single path switches cleanly | Dual ghost residual |
| Default clarity | Prefer floating hands | Forced dual |

**Procedure (claim “G38 worldmodel OK” only if walked):**

1. Floating hands on — body gone, arms clear.  
2. Per-weapon use worldmodel — gun only once.  
3. No double-ghost hands/gun.

Pure helpers: `WorldModelLaw_ResolvePath` / `WorldModelLaw_Decide` / `WorldModelLaw_HmdExpect` (unit-tested; offline ≠ HMD OK).

### 0.26 G39 VR_Init human error surface (manual)

Offline gate proves codes **108/215** humanized, module zip linked, toast required on fail. **Headset/runtime** for real init fails.

| Check | Expect | FAIL if |
|-------|--------|---------|
| No runtime / no HMD | Toast names issue; overlay has module URL | Silent fail / crash only |
| Code 108-like | HMD not found guidance | Opaque “Init failed” |
| Code 215-like | Runtime busy / restart SteamVR tip | No triage |
| Module missing/old | Zip link + version | No download path |

**Procedure (claim “G39 init surface OK” only if walked):**

1. Stop SteamVR/OpenXR — Start VR → human toast + overlay.  
2. Module missing — clear install path.  
3. Success path — no false error toast.

Pure helpers: `InitLaw_Humanize` / `InitLaw_Decide` / `InitLaw_HmdExpect` (unit-tested; offline ≠ HMD OK).

### 0.27 G40 Vision / border fill walk (manual)

Offline gate proves guided path defaults (scale=1, offsets=0), clamps, bleed risk, HmdExpect. **Headset** still required to claim bars/fill OK.

| Check | Expect | FAIL if |
|-------|--------|---------|
| Defaults / after Experience | HMD filled; no permanent black bars | Bars with scale≈1 and offsets≈0 |
| `vrmod_border_calibrate` | Guided scale → V → H → save; profile reloads later | Slider maze; cancel leaves broken crop only |
| Extreme scale/offset | Comfort bleed risk flagged; re-run guide | Stuck tunnel/crop with no guide path |
| Soft care | FOV archives not force-written mid-guide (G30) | FOV thrash fighting Vision |

**Procedure (claim “G40 border fill OK” only if walked):**

1. Fresh or Reset Experience → Vision/border path → edges fill HMD both eyes.  
2. Crimson Cube → Border calibrate one trigger → complete save → reboot keeps fill.  
3. Optional: `print(g_VR._borderLawLabel, g_VR._borderLawHmdExpect and g_VR._borderLawHmdExpect.checklist)`.

Pure helpers: `BorderLaw_GuideBaseline` / `BorderLaw_Decide` / `BorderLaw_HmdExpect` (unit-tested; offline ≠ HMD OK).

### 0.28 G41 HMD walk inventory + dump (manual aid)

Offline gate proves the **catalog** of open walks (G05…G44) + forbidden offline-smoke claim. Does **not** close any HMD walk.

| Tool | Role |
|------|------|
| `vrmod_hmd_expect_dump` | Print inventory + live `g_VR._*HmdExpect` snapshots |
| Pure `HmdWalk_Catalog` / `HmdWalk_CollectLive` | Operator/offline tokens |

**Prefer next (product feel):** G05 load-flash → G12 ambient → G40 border → G28 handoff timeouts → G04 warm.

Pure helpers: `HmdWalk_FormatReport` / `HmdWalk_Decide` / `HmdWalk_HmdExpect` (unit-tested; offline ≠ HMD OK).

### 0.29–0.31 Ship-bar offline laws (G42–G44)

| Gap | Theme | Pure | Walk |
|-----|-------|------|------|
| G42 | Hands stuck identity/raw unstick | `HandStuckLaw_*` | free hands + foregrip |
| G43 | Menu-open nested RT crash | `NestedRtLaw_*` | open menus 5s |
| G44 | grab_end drop cooldown | `GrabEndLaw_*` | pick/drop storms |

Offline green ≠ HMD OK for any of these.

**Automation gap (open):** no CI job runs OpenXR runtime + HMD. Do not invent “HMD smoke passed offline.” When automation lands, keep it optional/nightly — never block pure-util PRs.

### Agent / polish-loop law

- Offline red → fix or revert; **never push**.  
- Offline green + no HMD → say “offline gate green”; do **not** claim headset-proven.  
- G02/G03/G05 feel bugs need headset observation even when offline is green.

---

## 1. Why this exists

The offline gate and contract inventory close the old “only found in HMD” util/API gap. Remaining risk is **runtime XR** (handoff, submit, cal, Glide), covered only by manual smoke above.

| Gap (historical) | Status |
|------------------|--------|
| Almost no Lua unit tests | Closed for pure utils + menu/experience offline |
| C++ module surface drift | Module + export registry tests in `test_all` |
| Launcher untested offline | Launcher unit tests (handoff, last_play, math3d) |
| No inventory gate | `gen_contracts` + hard pure-pending fail (G21) |
| No HMD automation | **Still open** — see §0 |

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

## 15. Next concrete steps

1. Keep pure-pending at **0** when adding helpers (`PURE_TESTED` + unit test).  
2. Prefer pure extraction before touching HMD-only paths.  
3. HMD smoke remains checklist-driven until an optional OpenXR smoke job exists.  
4. Do **not** expand in-game automation as a PR-blocking gate while offline is the product gate.

---

*Document version: 1.1 — offline gate live; HMD smoke manual (G24). Update contracts when symbols change.*
