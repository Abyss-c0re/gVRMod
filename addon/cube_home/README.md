# gVRMod Home (`gm_infmap_home`)

VR home hub built on **[InfMap – Infinite Map Base](https://steamcommunity.com/workshop/filedetails/?id=2905327911)** (Workshop `2905327911`).

## Why InfMap

- Empty BSP + Lua terrain (no Hammer brush loop jank)
- Map name contract: second token must be `infmap` → InfMap API initializes
- Per-map scripts live under `lua/infmap/<mapname>/` and can grow without forking the base

## Install

1. Subscribe to InfMap base (or let CubeUI extract Workshop GMA on Start).
2. Install gVRMod (`./install.sh`) — symlinks this addon as `garrysmod/addons/cube_home`.
3. CubeUI defaults to **`gm_infmap_home`** when no last-play snapshot exists.

## Dynamic improve

Layout is JSON-driven and reloadable without remapping:

| Command | Effect |
|---------|--------|
| `cube_home_set_spawn` | Save current pos/ang as default spawn |
| `cube_home_add_prop [model]` | Add frozen prop at feet; writes layout |
| `cube_home_reload` | Rebuild from `data/cube_home/layout.json` |
| `cube_home_save` | Write current layout to disk |
| `cube_home_reset` | Restore shipped defaults |
| `cube_home_goto <zone>` | Teleport (`plaza`, `sandbox`, `range`, `build`) |

Files:

- Live: `garrysmod/data/cube_home/layout.json`
- Default: `addon/cube_home/data/cube_home/layout_default.json`

Edit JSON (zones, props, `plaza_radius`, `hill_scale`) then `cube_home_reload`.

## Map-local Lua

| File | Role |
|------|------|
| `sh_home_config.lua` | Defaults + load/save |
| `sh_collider_functions.lua` | Flat plaza height function |
| `sv_terrain_collision.lua` | InfMap terrain colliders |
| `cl_terrain_visual.lua` | Client meshes |
| `sv_home_layout.lua` | Prop/zone build + console API |
| `cl_home_ui.lua` | Labels + welcome |

## Dependency

InfMap base must provide `InfMap` global, `simplex.lua`, and entities (`infmap_terrain_*`). Without it the BSP still loads but terrain/layout will error — Cube prints a clear InitPostEntity warning.
