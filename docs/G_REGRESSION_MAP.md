# G-series “auto fix” map — expected issues & revert guide

**Purpose:** Many polish-loop **G##** commits added *pure laws* + thin wiring into live paths. Some are toast/classify-only; some change frame behavior. Use this table when something that “used to work” breaks.

**Branch tip (product):** `vrmod-x64` `cube-stereo-g45`  
**Rule of thumb:** Prefer **targeted revert of the wire** over deleting the pure `sh_*_law.lua` (keeps offline tests). Or `git revert <sha>` if the whole commit is toxic.

**Last known gun-related reverts already on tip:**
| Fix | Commit | Notes |
|-----|--------|-------|
| Restore gun draw (undo G38 skip) | `286bf5a` | Always `DrawModel` when `g_VR.viewModel` valid |
| Undo WepInfo world-model fallback | `a49ba05` | Floating gun off-hand |
| Keep resume take_xr | `ec5b392` | Soft resume; keep unless return-to-Cube worse |

---

## How G commits are structured

Most look like:
1. **Pure** `lua/vrmod/utils/sh_*_law.lua` (decide/status only — usually safe)
2. **Wire** into `cl_vrmod.lua` / UI / physics (this is what breaks play)

**Safe-ish to keep:** pure file + toast/log only.  
**High risk:** anything that changes `PostDraw*`, `RenderView`, pose SoT, collisions, weapon bind, mat_queue, submit, or handoff timing every frame.

---

## Symptom → likely G / area

| You see | Suspect | Revert target first |
|---------|---------|---------------------|
| Guns missing | G38 draw skip (partially fixed `286bf5a`), switchweapon race, G43 nested RT | `cl_vrmod` draw hook; `sh_network` switchweapon |
| Guns float off hand | WepInfo / zero offsets / world-model path | `sh_weps` / `sh_network` switchweapon; wipe `data/vrmod/vrmod_weapons_config.json` if stale |
| Dual ghost hands+gun | G38 | `sh_worldmodel_law` wire in `SetupModelAndPlayerHooks` |
| One eye black / mono both eyes | G47, G05, mat_queue path, G19 | `sh_false_per_eye_law`, stereo-load, submit |
| Desktop OK, HMD black | G32, G46 desktop mirror, share texture | DesktopMirror / post-submit mirror off |
| FOV/Z tweak jitters one eye | G36 | `sh_fovz_law` SoftRefresh wire |
| Fisheye / tiny world | G35, border/cal | `sh_viewscale_law`, G40 border |
| Head roll warps / stereo shear | stereo_rigid (`f847434`), G33 swap eyes | `sh_stereo_rigid_law`, swap-eyes convar |
| Spawn fly-away / dead sticks | G34 | `sh_flyaway_law` wire in start |
| Hands stick / glue L=R | G42 | `sh_hand_stuck_law` heal (can over-correct) |
| Bullets stop at hands | G37 | `sh_hand_bullet_law` + `sv_collision_proxies` |
| Laser wrong hand / no UI click | G16 / G45 | `sh_laser_law` + `cl_ui` |
| Menu open freezes / crash | G43 | `sh_nested_rt_law` + `cl_ui` paint gate |
| Can’t return to Cube / RESUME dies ~90s | G13 + soft handoff G28 + take_xr | `cl_cube_bridge`, `cl_openxr_launch`, Cube soft timeout |
| Height jump on VR claim | G03 StagePack apply (if opt-in on) | `sh_stage_pack` executor; keep gate default off |
| Unexpected changelevel | G04 warm attach (if opt-in on) | `vrmod_warm_changelevel` / env |
| Glide no steer | G14 incomplete | `sh_glide_sot` + vehicle hooks |
| Bindings spam / rewrite | G31 | `sh_bindings_law` |
| Blocked convar spam gone but shadows wrong | G27 | `sh_engine_blacklist_law` (usually keep) |
| HUD black plate / washed | G15 | `sh_hud_law` composite |

---

## Full G map (product-relevant)

Risk: **L** low (toast/pure) · **M** medium (path change, opt-in) · **H** high (every frame / guns / stereo / handoff)

| ID | Theme | Key commit(s) | Pure module | Wired into | Risk | Expected side effects if broken |
|----|-------|---------------|-------------|------------|------|----------------------------------|
| **G01** | Handoff progress phases | map_ready etc. | — | openxr launch / Cube panel | L | Opaque handoff UI only |
| **G03** | STAGE pack height | `d052c35` `82a0a6d` `3d98cb4`… | `sh_stage_pack.lua` | `cl_openxr_launch` | M | Seated jump if apply opt-in; blocked toast spam |
| **G04** | Warm attach / changelevel | `b299125` `c3fefcd` | `sh_warm_attach.lua` | `cl_openxr_launch` | M | Wrong map attach; changelevel if opt-in |
| **G05** | Stereo through load | `879a551` `44f3f2d` | `sh_stereo_load.lua` | start/load path | M | Mono/void during load; dual-hold toast |
| **G10** | Skip Cube Experience spam | `70ff998` | — | experience gates | L | First-run cal re-prompts |
| **G13** | Return-to-Cube marker | `b1dc55f` + bridge | `sh_cube_return.lua` | `cl_vrmod` exit, `cl_cube_bridge` | **H** | No Cube return; soft 90s drop XR; resume stuck |
| **G14** | Glide stick SoT | `e8318a9` | `sh_glide_sot.lua` | vehicle input | M | Dead Glide throttle/steer |
| **G15** | HUD additive plate | `f232677` | `sh_hud_law.lua` | HUD composite | M | Black/opaque HUD wall |
| **G16** | Laser primary hand | `5c9aa4c` | `sh_laser_law.lua` | `cl_ui` | M | Wrong-hand laser; UI click fail |
| **G17** | mat_queue never VR-write | `e462be1` | `sh_mat_queue_law.lua` | `cl_vrmod` | M | Mode 2 mono/crash if user pins 2; never auto-fix 2 |
| **G19** | Submit dual OUT only | `ae8a517` | `sh_submit_law.lua` | submit path | **H** | Black HMD / wrong eng IN path |
| **G20** | Color/finger pure utils | `a0b9d2e` `f9d5d8a` | `sh_math` helpers | laser/theme | L | Theme/laser color wrong |
| **G23** | Desktop follow-cam | `8dc402c` | — | desktopview=4 | L | Follow-cam break only |
| **G25** | Pose single SoT | `fbf50f8` | `sh_pose_sot_law.lua` | `cl_vrmod` tracking | **H** | Guns/hands desync; dual-truth jitter |
| **G26** | Menu thrash dedupe | `95edf81` | `sh_menu_law.lua` | QM | L | Missing/dup menu entries |
| **G27** | Engine blacklist | `d63c6c1` | `sh_engine_blacklist_law.lua` | convar writes | M | “Command blocked” gone; some quality cvars never set |
| **G28** | Soft 90s / hard 180s handoff | CubeUI `CubeHandoffTimeout_*` | parent launcher | `xr_app` | **H** | Cube exits XR before GMod takes it |
| **G31** | Bindings self-heal | `8ad16c5` | `sh_bindings_law.lua` | bindings load | M | Manifest rewrite loops / toast spam |
| **G32** | Stereo self-test toast | `7cffb05` | `sh_stereo_selftest_law.lua` | start timer | L | False “no HMD” toast |
| **G33** | Swap eyes content-only | `ae7e483` | `sh_swap_eyes_law.lua` | stereo UV | M | Eyes swapped or double if pose also flipped |
| **G34** | Fly-away snap | `ca1bea2` | `sh_flyaway_law.lua` | VR start | **H** | Origin snap wrong; inputs dead at start |
| **G35** | Viewscale clamp | `c126533` | `sh_viewscale_law.lua` | view scale | M | Fisheye / forced comfort clamp |
| **G36** | FOV/Z soft refresh | `704554c` | `sh_fovz_law.lua` | SoftRefresh | **H** | One-eye jitter; mid-frame UV fight |
| **G37** | Hand vs bullets | `d2b88d0` | `sh_hand_bullet_law.lua` | `sv_collision_proxies` | M | Bullets stop at hands; grab weird |
| **G38** | Worldmodel single path | `3952a64` (+ fix `286bf5a`) | `sh_worldmodel_law.lua` | `SetupModelAndPlayerHooks` | **H** | **Guns vanish** if draw skipped; dual ghost if law off |
| **G39** | VR_Init human errors | `237a742` | `sh_init_law.lua` | init error path | L | Worse error strings only |
| **G40** | Vision border fill | `07a4a5e` | `sh_border_law.lua` | border cal | M | Black bars / over-crop |
| **G41** | HMD walk inventory | `c0d83ec` | `sh_hmd_walk_law.lua` | dump concommand | L | Diagnostics only |
| **G42** | Hands stuck unstick | `ab91a01` | `sh_hand_stuck_law.lua` | pose heal | **H** | Over-heal → rubber hands / desync |
| **G43** | Nested RT menu crash | `bf6355e` | `sh_nested_rt_law.lua` | `cl_ui` | **H** | Menus never paint; or crash if law removed wrong |
| **G44** | Grab end cooldown | GrabEnd law | `sh_grab_end_law.lua` | pickup | M | Drop storms / can’t drop |
| **G45** | Primary hand left | Laser decide | `sh_laser_law` | primary hand | M | Left-hand primary wrong |
| **G46** | Desktop mirror vs HMD | `dec3dc6` | `sh_desktop_mirror_law.lua` | `cl_rendering` / eyes | **H** | Desktop mirror blacks HMD |
| **G47** | False per-eye FBO | `6e53968` | `sh_false_per_eye_law.lua` | stereo FBO | **H** | Forced SBS mono; one-eye black |
| **stereo_rigid** | Head-roll shear | `f847434` | `sh_stereo_rigid_law.lua` | eye placement | M | Flat stereo if forced; warp if off under roll |

---

## Suggested revert order (when play is broken)

Do **one** at a time; smoke after each.

1. **Guns missing / float**  
   - Confirm tip ≥ `286bf5a` + `a49ba05`.  
   - If still bad: soft-revert G38 wire only (keep pure file); reset `vrmod_weapons_config.json`.

2. **HMD black / one eye**  
   - Soft-disable G46 desktop post-submit mirror (should already be off).  
   - Check G47 / mat_queue 2 mono path.  
   - G19 submit path last.

3. **Return / RESUME broken**  
   - G13 bridge + openxr take_xr (`ec5b392` is the fix; don’t revert unless worse).  
   - G28 soft 90s in CubeUI: hold XR until take_xr, don’t soft-exit on silence.

4. **Tracking / hands wrong**  
   - G42 hand-stuck → G25 pose SoT → G34 fly-away.

5. **Cal / FOV pain**  
   - G36 FOV/Z → G35 viewscale → G40 border.

---

## How to revert without nuking the branch

### A. Soft disable (preferred)

```lua
-- example: stop G38 from affecting draw (already done: always DrawModel)
-- example: disable hand-stuck heal
-- if vrmod.utils.HandStuckLaw_Decide then ... skip call
```

Comment out the **call site** in `cl_vrmod.lua` / physics; leave pure law for tests.

### B. Git revert one commit

```bash
cd gVRMod/addon/vrmod-x64   # or Dev/GMod/vrmod-x64
git checkout cube-stereo-g45
git revert --no-edit 3952a64   # example: G38
# resolve conflicts, then:
git push origin cube-stereo-g45
```

### C. Nuclear: pin to known-good pre-G-law core

Last core before dense G34–G43 wave (approx): **before** `ca1bea2` (G34) if you want tracking-stable era — but you lose later Cube bridge work. Better cherry-pick G13/bridge onto older base than hard reset main product.

Known product bridge stack (keep if possible):
- Cube return / sole hub / resume: `b1dc55f`, `b491c9f`, `ec5b392`, `286bf5a`
- Rigid stereo: `f847434`

---

## Pure law files (inventory)

All under `lua/vrmod/utils/`:

| File | G |
|------|---|
| `sh_stage_pack.lua` | G03 |
| `sh_warm_attach.lua` | G04 |
| `sh_stereo_load.lua` | G05 |
| `sh_cube_return.lua` | G13 |
| `sh_glide_sot.lua` | G14 |
| `sh_hud_law.lua` | G15 |
| `sh_laser_law.lua` | G16 |
| `sh_mat_queue_law.lua` | G17 |
| `sh_submit_law.lua` | G19 |
| `sh_pose_sot_law.lua` | G25 |
| `sh_menu_law.lua` | G26 |
| `sh_engine_blacklist_law.lua` | G27 |
| `sh_bindings_law.lua` | G31 |
| `sh_stereo_selftest_law.lua` | G32 |
| `sh_swap_eyes_law.lua` | G33 |
| `sh_flyaway_law.lua` | G34 |
| `sh_viewscale_law.lua` | G35 |
| `sh_fovz_law.lua` | G36 |
| `sh_hand_bullet_law.lua` | G37 |
| `sh_worldmodel_law.lua` | G38 |
| `sh_init_law.lua` | G39 |
| `sh_border_law.lua` | G40 |
| `sh_hmd_walk_law.lua` | G41 |
| `sh_hand_stuck_law.lua` | G42 |
| `sh_nested_rt_law.lua` | G43 |
| `sh_grab_end_law.lua` | G44 |
| `sh_desktop_mirror_law.lua` | G46 |
| `sh_false_per_eye_law.lua` | G47 |
| `sh_stereo_rigid_law.lua` | stereo_rigid |

---

## Also not G but recent regressions

| Change | Symptom | Action |
|--------|---------|--------|
| `ec5b392` EnsureViewModelBound / WepInfo world fallback | Floating gun | Reverted `a49ba05` |
| G38 `drawVm=false` | No gun mesh | Fixed `286bf5a` |
| Cube soft 90s without take_xr | Launcher dies, passthrough | Resume uses `OpenXR_ForceStartWithHandoff` |
| Dual addons `vrmod-x64` + `.disabled` | Weird double-load | Only one enabled tree (symlink to submodule) |

---

## Maintenance

When adding a new G commit:
1. Add a row here **same PR** (risk + symptom + wire file).
2. Prefer pure-only first; wire behind convar default **off** when behavior changes.
3. Never land “auto fix” that skips draw of `g_VR.viewModel` without drawing an alternate mesh.

**Sources:** `git log --grep='G[0-9]' cube-stereo-g45`, `state/polish_loop/GLOGIC_GAPS.md`, `docs/CUBE_WATCHLIST.md`, play regressions 2026-08-06.
