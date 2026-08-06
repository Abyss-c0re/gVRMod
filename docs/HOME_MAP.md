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

### Error-pink chroma void (stable PoC)

Depth / “invisible texture” experiments regressed the headset view. Restored simple
chroma that worked as a PoC, using the **Source missing-texture pink** (not black/green):

| | |
|--|--|
| Key color | **`#FF00DC` (255, 0, 220)** — bright cell of the classic error mosaic |
| Void | Large unlit plane + `r_drawworld 0` (sky off) |
| Module v51 | Distance to key RGB → alpha 0 |
| Room | OpenXR `ALPHA_BLEND` under transparent holes |

```
VRMOD_SetPassthroughChromaKey(1, 0, 220/255)
VRMOD_SetPassthroughChroma(true, 0.18)
VRMOD_SetEnvironmentBlendMode(3)
```

Material: `cube_home/pt_void`. Avoid pure error-pink on props while PT is on.

## Dynamic layout

```
cube_home_set_spawn
cube_home_add_prop [model]
cube_home_reload / save / reset
cube_home_goto plaza|sandbox|range|build
```

Layout: `garrysmod/data/cube_home/layout.json`
