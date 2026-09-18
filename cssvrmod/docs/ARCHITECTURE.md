# CSSVRMod architecture

CSS has no GMod Lua. Combat laws from `addon/vrmod-x64` are **C++** here. Texture share + OpenXR follow gVRMod / Cube compositor split.

```
Priority: OpenGL/togl (gVRMod Linux)     Also: DX9 CreateTexture (original module)
         │                                         │
         ▼                                         ▼
   -dx9 + SDL_VIDEODRIVER=x11              IDirect3DDevice9::CreateTexture
   SDL_GL_SwapWindow / glXSwap             Windows shaderapidx9.dll
                                           Linux libtogl.so (same D3D9 ABI)
Default launch: `-vulkan` + decorated window. Each `vkQueuePresentKHR` copies
the swapchain off-thread and submits one head-locked 16:9 OpenXR quad (mono
until dual RenderView). Stereo projection of that same 2D frame is what looked
like two squares floating far apart. `CSSVR_XR=0` skips submit.
```

## Layers

| Layer | Source | Role |
|-------|--------|------|
| Laws | `include/cssvrmod/*` | Melee, hand-bullet, wall sweep, aim/laser, input map |
| Catalog | `src/weapons.cpp` | CSS `weapon_*` damage / muzzle / knife |
| Launch | `src/launch.cpp` + `CSSVR` | Find Steam CSS, `LD_PRELOAD` hook |
| Hook GL | `src/hook_gl.cpp` | **priority** — `SDL_GL_SwapWindow` + `glXSwapBuffers` |
| Hook window | `src/hook_window.cpp` | strip `SDL_WINDOW_BORDERLESS` / Motif no-decor |
| Hook DX9 | `src/hook_d3d9.cpp` | original vrmod `CreateTexture` / `Present` (togl) |
| Hook VK | `src/hook_vk.cpp` | 64-bit CSS `shaderapivk` present (fallback) |
| XR | `src/xr_host.cpp` | Session + one VIEW-space cinema quad + shell input |
| Engine | `src/source_if.cpp` | `CreateInterface` probe; `ClientCmd` only after GetScreenSize self-test |

## Combat (from Lua)

- **Melee** (`sh_combat.lua`): velocity gate `threshold*50`, damage `base * min(5, 1+speed*scale) * impact`.
- **Hand bullet** (`sh_hand_bullet_law.lua` G37): hands 0.45× + drop; head 10×; proxies never solid to world.
- **Collision** (`sh_collisions.lua`): last-free hull sweep; floor/ceiling do not lock; barrel tip cannot pass a wall.
- **Aim** (`sh_weps.lua` + `cl_laser_pointer.lua` + `sh_laser_law.lua`): gun slaves the primary hand; laser primary-only; **snap-on-fire** uses gun forward for viewangles.

## Honest limits (do not claim HMD smoke from offline green)

- Submit is **mono capture → one VIEW-space 16:9 quad** until dual `RenderView` exists (P1).
- World traces in-game need `IEngineTrace` wired (P2). Offline tests inject a `TraceFn`.
- `ClientCmd` digital move/fire is P0; analog `CUserCmd` is P2.
- Offline `cssvrmod_tests` ≠ headset-proven.
- **OpenGL is the default launch** (`CSSVR` / `--gl` → `-dx9` + `SDL_VIDEODRIVER=x11`).
- Live 64-bit CSS togl CreateDevice: adapter stubs + `GetDisplayDB` hook + launcher `gGL` seed + skip of the `raise()` on `gGL+0x3a8 != 0x8cd5`. Reaches `OpenGL: … Mesa 4.6` and the extension dump. Then **RIP=0 after CFontManager** (NULL GL entry during RT setup). No first `Present` yet. `--vk` remains the proven present dump.
- Original vrmod DX9 path is `shaderapidx9` `CreateTexture` (`src/rendering/d3d/d3d_hooks.cpp` on Windows; `hook_d3d9.cpp` on Linux togl).

## P1 / P2

1. Hook `CViewRender` / stereo views (true IPD).
2. `IEngineTrace` melee + wall in live CSS.
3. Hand worldmodels + knife swing synced to CSS knife anim.
