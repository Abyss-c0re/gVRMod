# engine

LÖVR 0.18 window on the Source-unit proof core. This is not Garry's Mod and not VPhysics.

## Run

```sh
engine/run.sh                         # flat window, gm_construct if the Steam install is present
ENGINE_MODE=stereo engine/run.sh      # two eye renders of that map, side by side
ENGINE_MAP=/path/to/map.bsp engine/run.sh
ENGINE_MAP=none engine/run.sh         # builtin colored room
ENGINE_SHOT=1 engine/run.sh           # one frame of the default map, qa/out/map.png
engine/qa/capture.sh                  # red-sphere eye pair + OpenCV check
luajit engine/tests/run.lua           # offline proofs, no window
```

The LÖVR binary is `engine/.cache/lovr.AppImage` (not in git). `run.sh` always passes `--simulator`, so it does not open WiVRn or change the OpenXR active runtime.

WASD moves. Hold the left mouse button to look. Space jumps. The player tick is `pure/move.lua`. A crate in the builtin room uses `pure/vphysics.lua`.

## What a green capture means

`qa/capture.sh` draws one red sphere from two cameras half an IPD apart and checks the pixel shift against `f * ipd / depth`. The simulator itself has one view. The two PNGs are the stereo pair.

## What is not here

Addon execution is a sandbox with an honest failure log (`qa/out/addon_matrix.tsv` after the Lua suite). A file that loads is not a working addon. Map brushes collide. Displacement collision does not. Water, ladders, and ducking are absent.

`gm_construct` is drawn from the local GarrysMod VPKs and the map pak. Those files are not copied into this repo. Brush faces sample the BSP lightmap (style 0, the flat page on bumped faces) into the vertex color. Brush entities (`func_brush`, glass, the color room) are stored around their entity origin and drawn there. Vehicle clips, triggers, and areaportals are not drawn. Static props are the reference pose of the MDL, placed with `sky_camera` scale when they sit in the 3D skybox. VVD fixups are applied so the VTX indexes the reordered vertices. The map `skyname` is drawn as the Source 2D sky cube (`gl_warp.cpp` face order) on the camera, with depth writes off, so openings show that texture. The flat `env_skypaint` clear remains where the cube is missing. The play camera uses Source `fov` 75: horizontal on a 4:3 window, widened on 16:9 so the vertical angle stays about 60°. Foliage cards with `$alphatest` discard texels below 0.7. Detail, sway, and vertex lighting on those cards are not applied. Glass and refract shaders with no albedo are omitted instead of drawn magenta. There is no lightmap atlas, no bone animation, no weapons, and no sandbox gamemode. A lit map with prop meshes is not Garry's Mod. Sky, nodraw, and clip faces are omitted. Detail, bump, and `$basetexture2` blends are not applied. The color room stays white: its material is unlit `color/white` and the BSP has no vertex colors for it.
