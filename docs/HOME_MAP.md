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

### Depth void (no color key — black is fine)

**Any RGB key (green, magenta, black) will punch materials that share that color.**  
Source writes opaque RGB; OpenXR holes need **alpha**. We use **depth**, not color:

| Layer | Behavior |
|-------|----------|
| World / sky | Hidden → depth stays at **clear (~1.0)** |
| Props / models | Write closer depth (including pure black models) |
| Module v50 | `alpha = 0` only where depth is at far/clear |
| OpenXR | `ALPHA_BLEND` composites the room under sky holes |

If depth RT cannot be sampled, void is **refused** (stays opaque) rather than
falling back to a color key.

```
VRMOD_SetPassthroughChroma(true, 0.04)  -- enables depth void (tol = far-edge soft)
VRMOD_SetEnvironmentBlendMode(3)
```

### Why not an “invisible texture”?

Source cannot emit alpha-only holes into the VR color RT. Real options:

1. **Depth far-plane void** (shipped)  
2. **`XR_FB_passthrough` under projection** (true cameras; next / CubeUI already)

## Dynamic layout

```
cube_home_set_spawn
cube_home_add_prop [model]
cube_home_reload / save / reset
cube_home_goto plaza|sandbox|range|build
```

Layout: `garrysmod/data/cube_home/layout.json`
