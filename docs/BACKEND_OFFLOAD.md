# Backend offload

Big maps hitch because Source engine traces (wall collision, climb scan) walk the BSP on the game thread. A VR frame cannot wait on that. Heavy work moves into the native module (`src/`) only when the work is pure math. The engine call stays on the main thread.

NexusCore seq 1793630197: throttle the scan. Do not thrash the input stack. The budget below is that throttle. It is not a second input path. CT 101 is not part of this change.

## Traces

`enginetrace` and `util.TraceLine` / `util.TraceHull` are **not thread-safe**. Calling them from a `std::thread` will crash. Do not pretend a background thread called `TraceLine`.

**Main thread only**

- The actual engine trace (`util.TraceLine`, `util.TraceHull`, `enginetrace`).
- Draining the new queue: at most **8** engine traces per frame (`vrmod.utils.TraceBudget_MaxPerFrame`).
- `vrmod.utils.TraceBudget_Admit(used, requested, max)` returns `run, defer`. `run` is how many to execute now. `defer` waits until a later frame. `used + run <= max` when `used` is already within `max`; if `used` is already over `max`, `run` is 0. Negative and non-numeric inputs run 0.

**A worker may**

- Build the query set (start, end, mins, maxs, mask) as plain numbers.
- Score, sort, hash, and cache that set.
- Read results the main thread already stored.
- Call `vrmod_trace_budget_admit` (`src/trace/trace_budget.cpp`). That helper is the same arithmetic as the Lua law. It does not call the engine, does not touch `ILuaBase` or the BSP, and does not start a thread.

**A worker may not**

- Call `TraceLine`, `TraceHull`, or `enginetrace`.
- Call `SetupBones`, `GetBoneMatrix`, or `ManipulateBoneAngles`.
- Touch `ILuaBase` or walk the BSP.

Lua contract: `addon/vrmod-x64/lua/vrmod/utils/sh_trace_budget_law.lua`. The utils loader picks up the `sh_` file. Nothing in the collision path calls it yet.

## Bone matrices NEXT

`SetupBones`, `GetBoneMatrix`, and `ManipulateBoneAngles` are **main-thread** studio calls. Same pattern later: cache the matrices once per frame on the main thread, then let a worker read the cache. Do not hide `SetupBones` in an unnamed thread.

`src/` has **no** calls to those three names.

Calls under `addon/vrmod-x64` (comments omitted):

- `lua/vrmod/api/cl_api.lua` — `SetupBones` 560, 563, 607; `GetBoneMatrix` 571, 572, 574, 608
- `lua/vrmod/core/cl_vrmod.lua` — `SetupBones` 2637 (viewmodel). Line 762 only mentions an old double `SetupBones`.
- `lua/vrmod/input/sh_doors.lua` — `GetBoneMatrix` 109, 137, 200
- `lua/vrmod/input/sh_locomotion.lua` — `SetupBones` 29
- `lua/vrmod/network/sh_network.lua` — `SetupBones` 471; `GetBoneMatrix` 474
- `lua/vrmod/pickup/sh_pickup_arcvr.lua` — `SetupBones` 75
- `lua/vrmod/player/cl_character_hands.lua` — `SetupBones` 12; `GetBoneMatrix` 27, 28, 144
- `lua/vrmod/player/cl_character.lua` — `SetupBones` 75, 403, 408, 737, 769, 846, 992; `GetBoneMatrix` 232, 233, 575, 588, 752, 763, 774, 876; `ManipulateBoneAngles` 302, 987 (`pcall` form), 1099
- `lua/vrmod/player/cl_foregrip.lua` — `SetupBones` 275
- `lua/vrmod/player/sh_character_fbt.lua` — `SetupBones` 91, 485; `GetBoneMatrix` 137, 138, 154, 166, 172, 173, 491, 535
- `lua/vrmod/ui/cl_halos.lua` — `SetupBones` 61
- `lua/vrmod/utils/cl_avatar_editor.lua` — `SetupBones` 351, 379, 389, 510, 704, 1202, 1340; `GetBoneMatrix` 353, 361, 709, 1095, 1106, 1211, 1428; `ManipulateBoneAngles` 1154
- `lua/vrmod/utils/cl_character_ik.lua` — `SetupBones` 94; `GetBoneMatrix` 64, 65, 383, 623, 652, 670; `ManipulateBoneAngles` 641, 681
- `lua/vrmod/utils/cl_iknet_mimic.lua` — `SetupBones` 342
- `lua/vrmod/utils/sh_pickup.lua` — `SetupBones` 753, 775
- `lua/vrmod/utils/sh_vehicles.lua` — `GetBoneMatrix` 267
- `lua/vrmod/utils/sh_weps.lua` — `SetupBones` 192, 203

`SetBoneMatrix` is the same rule when this cache lands. It is not offloaded here.

## What this change does not do yet

- Does not replace every Lua `TraceHull` / `TraceLine`. These still run on the main thread, unbounded by the new queue:
  - `lua/vrmod/utils/sh_collisions.lua` (wall collision)
  - `lua/vrmod/utils/sh_trace.lua`
  - `lua/vrmod/input/sh_climbing.lua` (climb scan)
  - `lua/vrmod/input/sh_locomotion.lua`
  - `lua/vrmod/physics/sv_collision_proxies.lua`
  - pickup, laser, eye, and look traces (`sh_pickup.lua`, `cl_laser_pointer.lua`, `sh_eye_tracking.lua`, `sh_look_target_law.lua`)
  - the HUD building grid in `lua/vrmod/ui/cl_hud.lua` (its own slice, not this queue)
- Does not cache bone matrices.
- Does not start a worker thread.
- Does not change OpenXR submit or eye pose.
