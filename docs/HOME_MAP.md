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

### Dual chroma — Source error mosaic (real-time GPU)

Full missing-texture checkerboard keys (both cells), processed **in the submit
pass** (stolen RT → alpha) — one fullscreen shader / eye, VR-rate, not lagged async.

| Cell | Color | Hex |
|------|-------|-----|
| Bright | (255, 0, 220) | `#FF00DC` |
| Dark | (1, 0, 1) | `#010001` |

| Rule | |
|------|--|
| Pink | Always → transparent |
| Black | Transparent **only if pink is in 3×3 neighborhood** (checker), so solid black models stay |
| Cost | ~9 texture samples/pixel — fine at 2× eye res / 90 Hz |

```
VRMOD_SetPassthroughChromaKey(1, 0, 220/255)   -- pink
VRMOD_SetPassthroughChromaKey2(1/255, 0, 1/255) -- black cell
VRMOD_SetPassthroughChroma(true, 0.18)
VRMOD_SetEnvironmentBlendMode(3)
```

Void fill: `cube_home/pt_void` (pink). Dual key also punches real `error` checker textures.

## Dynamic layout

```
cube_home_set_spawn
cube_home_add_prop [model]
cube_home_reload / save / reset
cube_home_goto plaza|sandbox|range|build
```

Layout: `garrysmod/data/cube_home/layout.json`
