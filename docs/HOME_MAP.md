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

### Error-checker void + dual chroma (real-time GPU)

**Void is the Source missing-texture mosaic** (not solid pink): generated RT checker
`#FF00DC` / `#010001`, drawn as a tiled sky plane.

Submit pass (stolen eye RT) dual-keys both cells → alpha 0 for the room.

| Cell | RGB | Hex |
|------|-----|-----|
| Bright | 255, 0, 220 | `#FF00DC` |
| Dark | 1, 0, 1 | `#010001` |

| Mask (`cube_home_passthrough_mask`) | Behavior |
|-------------------------------------|----------|
| **7** (default) | Pink always + black if pink nearby (full checker) |
| **1** | Pink only |
| **2** | Black only (independent — no pink neighbor gate) |
| **3** | Both colors independent |

```
VRMOD_SetPassthroughChromaKey(...) / Key2(...)
VRMOD_SetPassthroughChromaMask(7)  -- or 1 / 2 for single color
VRMOD_SetPassthroughChroma(true, 0.18)
```

GPU cost ~9 samples/px when mask has bit4; still VR-rate.

## Dynamic layout

```
cube_home_set_spawn
cube_home_add_prop [model]
cube_home_reload / save / reset
cube_home_goto plaza|sandbox|range|build
```

Layout: `garrysmod/data/cube_home/layout.json`
