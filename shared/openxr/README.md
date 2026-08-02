# Shared OpenXR input (`shared/openxr`)

**Why:** Cube native launcher and vrmod module both talk to OpenXR controllers.
They do **not** share a live session (only one app holds XR at a time), but they
**must share path strings and binding helpers** so Quest/Index/WiVRn paths never
drift between products.

| File | Use |
|------|-----|
| `openxr_paths.hpp` | Interaction profile + hand path constants |
| `openxr_bindings.hpp/.cpp` | Suggest/push/create helpers (`XrApi` = linked or PFNs) |
| `openxr_shell_input.hpp/.cpp` | Minimal shell action set (aim/trigger/grab/stick/menu) |

## Consumers

- **native_launcher** — links `openxr_bindings.cpp` + `openxr_shell_input.cpp`; uses linked loader.
- **vrmod module** — include `openxr_paths.hpp` for path SoT; can fill `XrApi` with `g_xr*` PFNs later for full binding helper use.

## Rule

New controller paths go in `openxr_paths.hpp` first, then launcher shell and/or
module binding tables. Do not hardcode `/user/hand/...` in both trees.
