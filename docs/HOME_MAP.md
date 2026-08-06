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

### AR content layer (the real product path)

Not a chroma key. Two layers:

| Layer | What |
|-------|------|
| **Room** | OpenXR `ALPHA_BLEND` / runtime passthrough under the projection |
| **Content** | Only CubeHome platforms + player + VR hands (map/InfMap **not drawn**) |

Pipeline:

1. Lua: `r_drawworld 0`, hide InfMap / non-content ents, invisible colliders (no couch cubes)  
2. Eye RT: clear depth; only AR content writes depth  
3. Module: clear swapchain **alpha 0** → blit color → **depth → alpha** (far = hole)  
4. `xrEndFrame` with `ALPHA_BLEND` + source alpha  

Signal: `VRMOD_SetPassthroughChroma(true)` + `g_VR.cubeHomeArLayer = true` before submit.

No green/black key colors. Black models stay solid (they still write depth).

## Dynamic layout

```
cube_home_set_spawn
cube_home_add_prop [model]
cube_home_reload / save / reset
cube_home_goto plaza|sandbox|range|build
```

Layout: `garrysmod/data/cube_home/layout.json`
