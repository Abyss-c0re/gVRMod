# Cube WebUI launcher — source layout

Modular C++ OpenXR host. Each unit has a **header** (API) and usually a **.cpp** (impl).

| Module | Headers / sources | Role |
|--------|-------------------|------|
| **entry** | `main.cpp`, `paths.hpp` | CLI, find GMod + XR runtime, start WiVRn |
| **host loop** | `xr_app.hpp` / `xr_app.cpp` | Session lifecycle, frame loop, handoff |
| **config** | `panel_config.hpp` / `.cpp` | `cube_webui.conf` + env |
| **world panel** | `world_panel.hpp` / `.cpp` | LOCAL pose, seed, ray hit, reface |
| **math** | `math3d.hpp` | Vec3, quat, pose helpers (header-only) |
| **GLX** | `glx_context.hpp` / `.cpp` | Hidden GLX context for OpenXR GL |
| **GL draw** | `gl_render.hpp` / `.cpp` | FBO, textures, projection, panel/laser draw |
| **XR input** | `xr_input.hpp` / `.cpp` | Thin façade → `shared/openxr` shell input |
| **shared XR** | `../shared/openxr/*` | Paths + bindings + shell set (also for vrmod) |
| **XR util** | `xr_util.hpp` / `.cpp` | Extensions, blend mode |
| **host cmds** | `host_cmd.hpp` / `.cpp` | `/tmp/cube_webui_cmd` |
| **UI** | `ui_panel.hpp` / `.cpp` | Raster WebUI (New Game / Addons / Settings) |
| **maps** | `maps_scan.hpp` / `.cpp` | BSP scan + categories |
| **addons** | `addons_mgr.hpp` / `.cpp` | Workshop/local, pages, thumbs, nomount |
| **spawn** | `gmod_spawn.hpp` / `.cpp` | cfg write + GMod process + handoff files |
| **launch map** | `launch_fill.hpp` | UI state → `LaunchRequest` |

```
main → paths → RunCubeWebUILauncher (xr_app)
                 ├─ panel_config, world_panel
                 ├─ glx_context, gl_render
                 ├─ xr_input, xr_util
                 ├─ ui_panel ← maps_scan, addons_mgr
                 └─ gmod_spawn ← launch_fill
```

Do not re-monolith `xr_app.cpp`. New features go in the matching module + header.
