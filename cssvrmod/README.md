# CSSVRMod

<p align="center"><strong>A VR mod for Counter-Strike: Source</strong><br/>
OpenXR · texture hook · C++ combat laws (from gVRMod Lua)</p>

CSSVRMod is a **sibling product** of [gVRMod](../README.md). It reuses the same foundations:

- **Texture hook (OpenGL first)** — togl `SDL_GL_SwapWindow` (gVRMod Linux). **DX9** is the original vrmod `CreateTexture` path. **Vulkan** present is the 64-bit CSS fallback when togl will not start.
- **OpenXR** — session + stereo submit + shared controller paths (`shared/openxr`)
- **App** — Cube-style launcher that finds CSS and starts it with the hook
- **Combat** — melee, hand-bullet filter, wall collision, gun-aim — ported from `addon/vrmod-x64` Lua to **pure C++** (CSS has no GLua)

## What you get

| Piece | Path | Offline? |
|-------|------|----------|
| Combat / aim / collision laws | `include/cssvrmod/` | **yes** (`cssvrmod_tests`) |
| CSS weapon catalog | `src/weapons.cpp` | **yes** |
| Launcher | `install/cssvrmod/CSSVR` | find/print without HMD |
| Hook | `install/cssvrmod/libcssvrmod_hook.so` | needs live CSS + HMD |

## Quick start

```bash
# build (from gVRMod root)
cmake -S cssvrmod -B cssvrmod/build
cmake --build cssvrmod/build -j"$(nproc)"
./cssvrmod/build/cssvrmod_tests

# locate CSS (Steam app 240)
./cssvrmod/scripts/CSSVR.sh --find

# play (WiVRn / Monado / SteamVR OpenXR)
./cssvrmod/scripts/CSSVR.sh --map de_dust2          # default: OpenGL/togl
./cssvrmod/scripts/CSSVR.sh --dx9 --map de_dust2    # original CreateTexture path
./cssvrmod/scripts/CSSVR.sh --vk --map de_dust2     # if togl crashes (this GPU)
```

Headset + CSS walk is **manual**. Offline green is not an HMD claim.

## Controls (Quest / Index-style)

| Input | CSS |
|-------|-----|
| Left stick | move |
| Right stick | snap / smooth turn |
| Primary trigger | `+attack` (view snaps to **gun**) |
| Off-hand trigger / grip | knife / `+attack2` (velocity melee) |
| A | jump |
| B | reload |
| X | use |
| Menu | scoreboard |

Aim is the **gun pose**, not the HMD crosshair. Look is HMD until you fire.

## Layout

```
cssvrmod/
  include/cssvrmod/   laws (header)
  src/                catalog, launch, hook, XR
  tests/              offline gate
  scripts/CSSVR.sh
  docs/ARCHITECTURE.md
```

License: **CUBECHAIN** (same as gVRMod).
