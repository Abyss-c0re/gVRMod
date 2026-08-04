# Desktop follow camera + broadcast module

## Desktop view (`vrmod_desktopview`)

| Value | Meaning |
|------:|---------|
| 1 | None (no desktop mirror) |
| 2 | Left eye (stereo RT crop) |
| 3 | Right eye (stereo RT crop) |
| 4 | **Follow camera** — invisible cam tracks player/HMD → GMod window |

Settings: **Rendering → Desktop view → follow camera**, plus follow-cam sliders.

## Follow cam convars

| Convar | Default | Role |
|--------|---------|------|
| `vrmod_desktop_cam_mode` | 0 | 0=behind HMD, 1=behind player, 2=module-only pose |
| `vrmod_desktop_cam_dist` | 72 | Distance behind target |
| `vrmod_desktop_cam_height` | 28 | Height offset |
| `vrmod_desktop_cam_fov` | 75 | FOV |
| `vrmod_desktop_cam_smooth` | 0.15 | Pose smoothing |
| `vrmod_desktop_cam_draw` | 1 | Blit to local desktop (0 = capture for module only) |

## Broadcast / control module API

Future streaming / remote apps register **without** forking gVRMod:

```lua
-- In your addon (loads after vrmod utils):
vrmod.DesktopBroadcast.Register({
  id = "my_stream_bridge",

  OnSessionStart = function()
    -- open encoder / websocket
  end,

  OnSessionStop = function()
    -- teardown
  end,

  -- Optional: fully drive camera (world space)
  GetCamera = function()
    -- return pos, ang, fov  or  nil to use built-in follow
    return nil
  end,

  -- Called each VR frame after RenderView into the cam RT
  OnVideoFrame = function(rt, w, h, frameId, meta)
    -- meta.pos, meta.ang, meta.fov
    -- Read RT / enqueue encode / broadcast
  end,

  -- Future remote control app → gVRMod
  OnControlPoll = function()
    -- return { type = "...", ... } or nil
    return nil
  end,
})
```

Helpers:

- `vrmod.DesktopBroadcast.Unregister(id)`
- `vrmod.DesktopBroadcast.SetActive(id)`
- `vrmod.DesktopBroadcast.GetActive()`
- `vrmod.DesktopBroadcast.HasModule()`
- `vrmod.DesktopCam.ComputeFollowPose(pos, ang, dist, height)` — pure math
- `vrmod.DesktopCam.GetRT()` — current render target

## Pipeline (mode 4)

1. Stereo eyes render into `g_VR.rt` (HMD) as usual  
2. Stereo RT is popped  
3. `DesktopCam.CaptureFrame` → `render.RenderView` from follow pose into private RT  
4. `OnVideoFrame` for registered module  
5. `PresentDesktop` blits RT to GMod window (if draw enabled)

Never nests under the stereo RT (avoids nested-portal crash).

## Call-site law (G23 — verified)

| Path | Behavior |
|------|-----------------|
| Stereo paint | Dual eyes into `g_VR.rt` only |
| OpenXR Collect + Submit | **Before** any desktop present |
| `PresentDesktopMirror` | **After** submit (left/right crop or follow blit) |
| `ComputeDesktopCrop(1\|4)` | Returns `0,0` (unused crop); L/R UVs clamped safe |
| `DesktopCam.SyncFromDesktopView` | Start/stop session from cvar + frame loop |
| Settings catalog combo | value `4` = “follow camera” |
| Cube launcher XR DESKTOP VIEW | cycles 1→2→3→4; labels NONE/LEFT/RIGHT/FOLLOW CAM |
| `cube_last_play` snapshot | clamps `xr_desktopview` to 1..4 |

**Law:** never bind `g_VR.rt` as a screen material before Collect/Submit. Mid-frame CullMode NDC eye crop was breaking HMD stereo when desktopview was left/right/none.

Helpers: `IsFollowMode`, `IsEyeCropMode`, `ClampDesktopView`, `CycleDesktopView`, `DesktopViewLabel`.

## Launcher

Cube XR settings row **XR DESKTOP VIEW** cycles: NONE → LEFT → RIGHT → **FOLLOW CAM**.
