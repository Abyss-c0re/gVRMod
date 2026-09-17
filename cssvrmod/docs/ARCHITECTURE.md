# CSSVRMod architecture

CSS has no GMod Lua. Combat laws from `addon/vrmod-x64` are **C++** here. Texture share + OpenXR follow gVRMod / Cube compositor split.

```
Priority: OpenGL/togl (gVRMod Linux)     Also: DX9 CreateTexture (original module)
         │                                         │
         ▼                                         ▼
   -dx9 + SDL_VIDEODRIVER=x11              IDirect3DDevice9::CreateTexture
   SDL_GL_SwapWindow / glXSwap             Windows shaderapidx9.dll
                                           Linux libtogl.so (same D3D9 ABI)
Fallback when togl will not start: -vulkan + vkQueuePresentKHR
```

## Layers

| Layer | Source | Role |
|-------|--------|------|
| Laws | `include/cssvrmod/*` | Melee, hand-bullet, wall sweep, aim/laser, input map |
| Catalog | `src/weapons.cpp` | CSS `weapon_*` damage / muzzle / knife |
| Launch | `src/launch.cpp` + `CSSVR` | Find Steam CSS, `LD_PRELOAD` hook |
| Hook GL | `src/hook_gl.cpp` | **priority** — `SDL_GL_SwapWindow` + `glXSwapBuffers` |
| Hook DX9 | `src/hook_d3d9.cpp` | original vrmod `CreateTexture` / `Present` (togl) |
| Hook VK | `src/hook_vk.cpp` | 64-bit CSS `shaderapivk` present (fallback) |
| XR | `src/xr_host.cpp` | Session + stereo swapchains + shell input |
| Engine | `src/source_if.cpp` | `CreateInterface` probe; `ClientCmd` only after GetScreenSize self-test |

## Combat (from Lua)

- **Melee** (`sh_combat.lua`): velocity gate `threshold*50`, damage `base * min(5, 1+speed*scale) * impact`.
- **Hand bullet** (`sh_hand_bullet_law.lua` G37): hands 0.45× + drop; head 10×; proxies never solid to world.
- **Collision** (`sh_collisions.lua`): last-free hull sweep; floor/ceiling do not lock; barrel tip cannot pass a wall.
- **Aim** (`sh_weps.lua` + `cl_laser_pointer.lua` + `sh_laser_law.lua`): gun slaves the primary hand; laser primary-only; **snap-on-fire** uses gun forward for viewangles.

## Honest limits (do not claim HMD smoke from offline green)

- Submit is **mono capture → both eyes** until a dual `RenderView` hook exists (P1).
- World traces in-game need `IEngineTrace` wired (P2). Offline tests inject a `TraceFn`.
- `ClientCmd` digital move/fire is P0; analog `CUserCmd` is P2.
- Offline `cssvrmod_tests` ≠ headset-proven.
- **OpenGL is the default launch** (`CSSVR` / `--gl` → `-dx9` + `SDL_VIDEODRIVER=x11`).
- Live 64-bit CSS on RADV GFX1201: `libtogl.so` `GetAdapterCount` **SEGV** (even with no hook, even in sniper). That is a togl/adapter bug, not the capture hook. `--vk` is the proven present path on this GPU.
- Original vrmod DX9 path is `shaderapidx9` `CreateTexture` (`src/rendering/d3d/d3d_hooks.cpp` on Windows; `hook_d3d9.cpp` on Linux togl).

## P1 / P2

1. Hook `CViewRender` / stereo views (true IPD).
2. `IEngineTrace` melee + wall in live CSS.
3. Hand worldmodels + knife swing synced to CSS knife anim.
