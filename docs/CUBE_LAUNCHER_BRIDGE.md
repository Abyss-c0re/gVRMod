# Cube launcher bridge (return / resume)

**Status:** product path in progress (G13 reverse handoff)  
**Remember:** when user leaves VR, Cube shell must come back; temporary return keeps GMod alive.

## Player intent

1. **VR exit** (any normal exit after Cube Start) → spawn CubeUI again so the HMD panel is available for New Game / Resume.
2. **Quick menu → Cube Launcher** → *temporary* return: leave VR, keep the map process, open CubeUI; **RESUME VR** re-enters without a full Steam relaunch when possible.
3. Launcher must **bridge** the gap: process-up detection, warm markers, GMod poll to `vrmod_start force`.

## Files / SoT

| Piece | Path |
|-------|------|
| Return marker | `garrysmod/data/vrmod/cube_return.txt` |
| Launch marker | `garrysmod/data/vrmod/openxr_launch.txt` (`cube_bin=…`, `warm_attach=1`) |
| Sticky Cube path | `garrysmod/data/vrmod/cube_launcher_path.txt` |
| Warm request | `garrysmod/data/vrmod/cube_warm.txt` |
| Lua bridge | `addon/vrmod-x64/lua/vrmod/ui/cl_cube_bridge.lua` |
| Pure return format | `sh_cube_return.lua` + `native_launcher/src/cube_return.hpp` |
| Soft resume Start | `xr_app.cpp` when `GModProcessRunning()` |

## Marker fields (`cube_return.txt`)

```
v=1
phase=vr_exit|xr_released|cube_claim|panel_live
map=<map>
source=vrmod_exit|quick_menu|…
intent=vr_exit|temp_return|resume
ts=<unix>
```

- **intent=temp_return** — GMod still running; Cube should paint **RESUME VR**.
- **intent=vr_exit** — normal leave; still relaunch shell; Start may cold-spawn if GMod quit.

## Flow

```
CubeUI (owns XR) → START → handoff take_xr → CubeUI exits XR session
GMod owns XR · play
  ├─ VR EXIT / disconnect VR → cube_return + spawn CubeUI (GMod may stay)
  └─ Quick: Cube Launcher → intent=temp_return → exit VR → spawn CubeUI
CubeUI panel live again
  └─ RESUME VR (process up) → warm_attach markers only → GMod poll → vrmod_start force
```

## Gaps still open (do not paper over)

| Gap | Notes |
|-----|--------|
| Cube process died without path | Need `cube_bin` from last Start; else `GVRMOD_CUBE_BIN` or desktop icon |
| Sandbox blocks `io.popen` | Spawn may fail; toast points at `/tmp/CubeUI_return.log` |
| Full dual-process XR reclaim without exit | Still not “Cube never dies”; relaunch is the product default |
| Changelevel on RESUME | Still opt-in (`vrmod_warm_changelevel`); same map only by default |
| Quest thin path | Same Lua bridge; ensure quest module also releases XR before Cube spawns |
| **XR race → bare passthrough** | Fixed soft: 2.5s delay + host retry loop + CubeUI `xrCreateSession` retries (~6s). If still stuck: desktop **gVRMod Cube** while GMod has no VR |

## False “Cube crashed” after Start Game

Normal handoff ends CubeUI with **exit 0** after it ran for many seconds.  
Old relaunch scripts treated **any** exit as failure and retried → log spam  
`FAILED` / boot loops while the game had already started.

**Rule now:** host retry only if CubeUI dies in the **first ~6s** (session create race).  
Lived longer → “normal exit (handoff/quit) — not a crash”.  
`VRMod_Start` / `afterVRLive` **cancels** any pending relaunch.

Auto-spawn CubeUI only when `ReturnToCubeLauncher` set relaunch (temp return),  
not on every VR exit from a Cube-started session.

## One pause surface

`vrmod.CloseAllPauseSurfaces` / `vrmod.OpenSoleHub` — hub open closes QM, settings,  
avatar, VirtualDisplay pause/launcher sessions. ESC and after-VR-live use sole hub.

## Recovery (HMD stuck on passthrough only)

1. Confirm GMod is **not** in VR (`vrmod_exit` / no dual-eye game view).  
2. On **desktop**, start **gVRMod Cube** / `scripts/CubeUI.sh` (claims OpenXR).  
3. Or from GMod console after XR free: `vrmod_start force` (game VR again, no Cube).  
4. Check `/tmp/CubeUI_return.log` for `xrCreateSession` failures.

## Manual HMD checklist

1. Cube → Start map → play.  
2. Quick menu **Cube Launcher** → VR ends, Cube panel returns, GMod window still up.  
3. **RESUME VR** → back in headset on same map.  
4. **VR EXIT** → Cube returns; Start/Quick Play works.  

## Env

| Env | Effect |
|-----|--------|
| `GVRMOD_CUBE_BIN` | Absolute path to CubeUI if marker missing |
| `GVRMOD_WARM_REUSE=1` | Experimental skip-spawn even without soft-resume path |
