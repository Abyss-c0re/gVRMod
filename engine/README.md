# engine

LÖVR 0.18 window on the Source-unit proof core. This is not Garry's Mod and not VPhysics.

## Run

```sh
engine/run.sh                         # flat, one camera, builtin room
ENGINE_MODE=stereo engine/run.sh      # two eye renders, side by side
ENGINE_MAP=/path/to/map.bsp engine/run.sh
engine/qa/capture.sh                  # headless-ish eye pair + OpenCV check
luajit engine/tests/run.lua           # offline proofs, no window
```

The LÖVR binary is `engine/.cache/lovr.AppImage` (not in git). `run.sh` always passes `--simulator`, so it does not open WiVRn or change the OpenXR active runtime.

WASD moves. Hold the left mouse button to look. Space jumps. The player tick is `pure/move.lua`. A crate in the builtin room uses `pure/vphysics.lua`.

## What a green capture means

`qa/capture.sh` draws one red sphere from two cameras half an IPD apart and checks the pixel shift against `f * ipd / depth`. The simulator itself has one view. The two PNGs are the stereo pair.

## What is not here

Addon execution is a sandbox with an honest failure log (`qa/out/addon_matrix.tsv` after the Lua suite). A file that loads is not a working addon. Map brushes collide. Displacement collision does not. Water, ladders, and ducking are absent. Materials and models are absent.
