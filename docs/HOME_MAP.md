# XR Home Passthrough

**Map:** `xr_infmap_passthrough`  
**Title:** XR Home Passthrough  
**Addon:** `gVRMod/addon/cube_home` → `garrysmod/addons/cube_home`

> InfMap only loads when the **second `_` token** is `infmap`.  
> So the map is `xr_infmap_passthrough` (not `xr_home_passthrough`).

## Credits

| | |
|--|--|
| Author | Meetric |
| GitHub | https://github.com/meetric1/gmod-infinite-map |
| Workshop | https://steamcommunity.com/workshop/filedetails/?id=2905327911 |
| Full note | [`addon/cube_home/CREDITS.md`](../addon/cube_home/CREDITS.md) |

## Passthrough (OpenXR + this map only)

| Condition | Required |
|-----------|----------|
| Map | `xr_infmap_passthrough` |
| Backend | OpenXR (`vrmod_xr`) |
| VR | Live session |

**Quick menu:** “Passthrough: ON/OFF” appears **only** when all three are true.  
It is not shown on other maps, OpenVR, or outside VR.

### Green-key void (not black)

Black chroma punched shadows/dark models. Instead:

| Layer | Behavior |
|-------|----------|
| Void | Pure **green** (`cube_home/pt_void` material + sky plane) |
| Module | Color-key chroma: distance to key RGB → alpha |
| Models | Normal materials — stay solid (not green) |
| OpenXR | `ALPHA_BLEND` + source alpha |

| Cvar / control | Effect |
|----------------|--------|
| Quick menu **Passthrough** | Toggle (map+OpenXR only) |
| `cube_home_passthrough 0/1` | Preference (archive) |
| `cube_home_passthrough_tol 0.22` | Key softness |
| `cube_home_draw_terrain 0` | Keep void (no flatgrass mesh) |

Module API (v48+):

```
VRMOD_SetPassthroughChromaKey(0, 1, 0)  -- pure green
VRMOD_SetPassthroughChroma(true, 0.22)
VRMOD_SetEnvironmentBlendMode(3)       -- auto alpha
```

### Future: “true” passthrough texture

Ideal path is a stencil/alpha material the engine writes without a visible key color
(or FB passthrough layer under projection). Green-key is the Source-safe PoC until
that exists. Avoid pure green on player models while PT is on.

## Dynamic layout

```
cube_home_set_spawn
cube_home_add_prop [model]
cube_home_reload / save / reset
cube_home_goto plaza|sandbox|range|build
```

Layout: `garrysmod/data/cube_home/layout.json`
