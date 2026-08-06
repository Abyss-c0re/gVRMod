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

### Invisible void (no key color)

**Any visible key color flickers and ruins models.** Source writes opaque RGB every
pixel; OpenXR only composites on **alpha**. We never draw a green/magenta plane.

| Layer | Behavior |
|-------|----------|
| World / sky | Hidden → empty pixels = pure engine black |
| Module | Punch **only pure neutral black** → alpha 0 (thr ~0.04) |
| Models | Normal materials (dark grey ≠ pure black → solid) |
| OpenXR | `ALPHA_BLEND` + source alpha |

| Cvar | Effect |
|------|--------|
| Quick menu **Passthrough** | Toggle (map+OpenXR only) |
| `cube_home_passthrough 0/1` | Preference |
| `cube_home_passthrough_tol 0.04` | Sky-black threshold (use 0.02–0.08 only) |

```
VRMOD_SetPassthroughChroma(true, 0.04)  -- void-alpha, not a color key
VRMOD_SetEnvironmentBlendMode(3)
```

### Why not an “invisible texture”?

Source cannot write alpha-only holes into the VR RT reliably. Options:

1. **Strict sky black → alpha** (shipped now)  
2. **Depth far-plane void** (next, if depth RT is bindable at submit)  
3. **`XR_FB_passthrough` under projection** (true cameras; CubeUI already does this)

## Dynamic layout

```
cube_home_set_spawn
cube_home_add_prop [model]
cube_home_reload / save / reset
cube_home_goto plaza|sandbox|range|build
```

Layout: `garrysmod/data/cube_home/layout.json`
