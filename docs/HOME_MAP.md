# gVRMod Home Map (InfMap)

**Map:** `gm_infmap_home`  
**Addon:** `gVRMod/addon/cube_home` → `garrysmod/addons/cube_home`

## Credits

Home runs on **InfMap** by **Meetric** (GPL-3.0). We ship a map-local hub layout only; the chunk engine and base package are theirs.

| | |
|--|--|
| Author | Meetric |
| GitHub | https://github.com/meetric1/gmod-infinite-map |
| Workshop | https://steamcommunity.com/workshop/filedetails/?id=2905327911 (`2905327911`) |
| Docs | https://github.com/meetric1/gmod-infinite-map/blob/main/docs.md |
| Full note | [`addon/cube_home/CREDITS.md`](../addon/cube_home/CREDITS.md) |

## Intent

A **home hub** for Cube/VR that can be **improved live** (JSON layout + console) without remapping or forking InfMap.

## Passthrough / AR void (default)

The home map is **not** meant to look like flatgrass. Default product look:

| Layer | Behavior |
|-------|----------|
| World | `r_drawworld 0` — no brush geometry |
| Terrain mesh | Off (`cube_home_draw_terrain 0`) |
| Sky / clear | Pure black |
| Platforms | Layout props only (opaque) |
| OpenXR | `ALPHA_BLEND` + **dark chroma** → black pixels transparent so the real room shows through (WiVRn/Quest when supported) |

| Convar / command | Effect |
|------------------|--------|
| `cube_home_passthrough 1` | AR void on (default) |
| `cube_home_passthrough_key 0.12` | Chroma threshold (higher = more transparent) |
| `cube_home_draw_terrain 1` | Optional InfMap ground mesh (flatgrass-like) |
| `cube_home_passthrough_toggle` | Flip AR mode |

Module API (v47+): `VRMOD_SetEnvironmentBlendMode(3)`, `VRMOD_SetPassthroughChroma(true, 0.12)`.

If the runtime has no alpha blend, you still get a black void with floating platforms (better than flatgrass); rebuild/install `gmcl_vrmod_xr` for full room passthrough.

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
