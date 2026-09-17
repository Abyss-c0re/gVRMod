# CSSVRMod architecture

CSS has no GMod Lua. Combat laws from `addon/vrmod-x64` are **C++** here. Texture share + OpenXR follow gVRMod / Cube compositor split.

```
┌──────────────────────────┐     GL backbuffer      ┌─────────────────────────┐
│  CSS (cstrike_linux64)   │ ─────────────────────► │  libcssvrmod_hook.so    │
│  togl + SDL swap         │                        │  · capture              │
│  Source usercmd          │ ◄── +attack / look ── │  · OpenXR submit        │
└──────────────────────────┘                        │  · Tick(combat/aim)     │
                                                    └───────────┬─────────────┘
                                                                │ xrEndFrame
                                                                ▼
                                                              HMD
```

## Layers

| Layer | Source | Role |
|-------|--------|------|
| Laws | `include/cssvrmod/*` | Melee, hand-bullet, wall sweep, aim/laser, input map |
| Catalog | `src/weapons.cpp` | CSS `weapon_*` damage / muzzle / knife |
| Launch | `src/launch.cpp` + `CSSVR` | Find Steam CSS, `LD_PRELOAD` hook |
| Hook | `src/hook_gl.cpp` | `SDL_GL_SwapWindow` + `glXSwapBuffers` |
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

## P1 / P2

1. Hook `CViewRender` / stereo views (true IPD).
2. `IEngineTrace` melee + wall in live CSS.
3. Hand worldmodels + knife swing synced to CSS knife anim.
