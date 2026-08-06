# gVRMod Home Map (InfMap)

**Map:** `gm_infmap_home`  
**Base:** [InfMap – Infinite Map Base](https://steamcommunity.com/workshop/filedetails/?id=2905327911) (Workshop **2905327911**)  
**Addon:** `gVRMod/addon/cube_home` → `garrysmod/addons/cube_home`

## Intent

A **home hub** for Cube/VR that can be **improved live** (JSON layout + console) without remapping or forking InfMap.

| Layer | Source |
|-------|--------|
| Infinite BSP + chunk API | InfMap base (required) |
| Home BSP rename | `maps/gm_infmap_home.bsp` (copy of base empty map) |
| Terrain (flat plaza + hills) | `lua/infmap/gm_infmap_home/` |
| Dynamic props/zones | `data/cube_home/layout.json` |

InfMap only initializes when the **second `_` token** of the map name is `infmap`  
(`gm_infmap_home` → `gm` | `infmap` | `home`).

## CubeUI

- Default selection (no last-play): **`gm_infmap_home`**
- Pinned first in Sandbox list when present
- On Start: `EnsureMapAvailable` also pulls InfMap base (WS extract) if missing

## Live improve

```
cube_home_set_spawn          # save spawn to layout.json
cube_home_add_prop [model]   # frozen prop at feet
cube_home_reload             # rebuild from disk
cube_home_save / reset
cube_home_goto sandbox|range|build|plaza
```

Edit `garrysmod/data/cube_home/layout.json` (zones, props, `plaza_radius`, `hill_scale`) then reload.

## Install

```bash
./install.sh --skip-build   # or full install
# links addons/cube_home → monorepo addon/cube_home
# subscribe InfMap base on Steam if not already
```

## Growth path

1. Add zones/props in JSON (no code)
2. New map-local scripts under `lua/infmap/gm_infmap_home/` (terrain, lighting, portals)
3. Optional: second map `gm_infmap_home_v2` with its own folder when a clean slate is needed
