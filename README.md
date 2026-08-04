# gVRMod

<p align="center">
  <img src="docs/assets/banner.png" alt="gVRMod" width="480" />
</p>

<p align="center">
  <strong>OpenXR native module + dual-backend Lua for Garry’s Mod VR</strong><br/>
  Quest · WiVRn · Monado · SteamVR OpenXR · classic OpenVR
</p>

<p align="center">
  <a href="https://github.com/Abyss-c0re/gVRMod"><img src="https://img.shields.io/github/last-commit/Abyss-c0re/gVRMod?label=gVRMod" alt="last commit" /></a>
  <a href="https://github.com/Abyss-c0re/vrmod-x64"><img src="https://img.shields.io/badge/Lua%20addon-vrmod--x64-c41e3a" alt="addon" /></a>
  <a href="https://github.com/Abyss-c0re/vrmod-module-master"><img src="https://img.shields.io/badge/OpenVR%20module-vrmod--module--master-2f6fed" alt="openvr" /></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-CUBECHAIN-8B0000" alt="license" /></a>
</p>

---

## What is this?

**gVRMod** is the **main project**:

| Piece | Location | Role |
|-------|----------|------|
| **OpenXR module** | `src/` · CMake · `install.sh` | `gmcl_vrmod_xr_*` binary |
| **Lua addon** | submodule [`addon/vrmod-x64`](https://github.com/Abyss-c0re/vrmod-x64) | Gameplay, UI, bindings, experience |
| **OpenVR module** | sibling repo [vrmod-module-master](https://github.com/Abyss-c0re/vrmod-module-master) | Classic SteamVR path (optional) |

The Workshop / Lua tree is **not** a second product — it’s the client half of this stack. Development SoT for Lua is the submodule here; release modules ship from **this** repo’s CI/install.

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│  gVRMod  (this repo)                                    │
│  · OpenXR C++  →  gmcl_vrmod_xr_linux64 / win64          │
│  · install.sh / build.sh                                │
│  · addon/vrmod-x64  ←── git submodule                   │
└───────────────────────────┬─────────────────────────────┘
                            │
          ┌─────────────────┴─────────────────┐
          ▼                                   ▼
   require("vrmod_xr")                 require("vrmod")
   OpenXR  (Quest/WiVRn/…)             OpenVR  (module-master)
          │                                   │
          └─────────────────┬─────────────────┘
                            ▼
                   Lua: vrmod-x64 addon
                   (auto-detect backend)
```

| Backend | Binary | `require` |
|---------|--------|-----------|
| **OpenXR** | `gmcl_vrmod_xr_linux64.dll` / `gmcl_vrmod_xr_win64.dll` | `"vrmod_xr"` |
| **OpenVR** | `gmcl_vrmod_linux64.dll` / `gmcl_vrmod_win64.dll` | `"vrmod"` |

Both DLLs can live in `garrysmod/lua/bin`. Lua picks one:

```text
vrmod_prefer_backend   auto | openxr | openvr
vrmod_backend          print active backend + version
```

---


## Testing

**Offline gate** (required before product push — no headset):

```bash
./scripts/test_all.sh          # contracts + Lua + C++ module + launcher
./scripts/test_all.sh --fast   # contracts + Lua only
./test.sh                      # clean rebuild + full suite
```

**HMD smoke** is still **manual** (not automated). Offline green ≠ headset-proven. Checklist and tool map: [`docs/TESTING_FRAMEWORK.md`](docs/TESTING_FRAMEWORK.md) §0.

- Framework + ship bar: [`docs/TESTING_FRAMEWORK.md`](docs/TESTING_FRAMEWORK.md)
- Desktop follow-cam / broadcast: [`docs/DESKTOP_BROADCAST.md`](docs/DESKTOP_BROADCAST.md)
- In-game smoke helper: `./quick_test.sh`


## Features (high level)

- OpenXR first-class (Quest 3 / WiVRn / Monado / SteamVR OpenXR)
- Dual-backend Lua — same addon for OpenXR **and** OpenVR
- Controller rebind UI (OpenXR) replacing SteamVR bind UI for booleans / chords
- Seated height, stereo / mat_queue hardening, VR menus & hand panels
- Pickup, collisions, locomotion, Glide, climbing hooks, etc. (see addon docs)

---

## Quick start

### Clone

```bash
git clone --recurse-submodules https://github.com/Abyss-c0re/gVRMod.git
cd gVRMod
./scripts/install_git_hooks.sh   # optional: submodule sync on commit
```

### Build & install (Linux)

```bash
./build.sh
./install.sh
# → garrysmod/lua/bin/gmcl_vrmod_xr_linux64.dll
# → garrysmod/addons/vrmod-x64/
```

### Build (Windows)

```bat
build.bat
# → install\GarrysMod\garrysmod\lua\bin\gmcl_vrmod_xr_win64.dll
```

### OpenVR (optional)

Install [vrmod-module-master](https://github.com/Abyss-c0re/vrmod-module-master) releases next to the XR binary.  
Do **not** overwrite `gmcl_vrmod_xr_*` with OpenVR names.

### Workshop players

Subscribe / use the Lua addon from [vrmod-x64](https://github.com/Abyss-c0re/vrmod-x64) **and** install a module from **this** repo (OpenXR) or module-master (OpenVR).

---

## Developing Lua

Edit **only** under `addon/vrmod-x64/`:

```bash
# after Lua changes
./scripts/sync_vrmod_x64.sh      # commit submodule + bump parent
# install into a live GMod tree
./install.sh
# or rsync addon/vrmod-x64 → garrysmod/addons/vrmod-x64
```

If you only update git and never install, GMod keeps an old copy of the addon — that is not a “ghost bug”.

---

## Platform notes

| Platform | Graphics | Texture share | Default RT flip |
|----------|----------|---------------|-----------------|
| **Linux x64** | OpenGL (GLX / togl) | gl textures + FBO | **1** |
| **Windows x64** | D3D9/11 path | D3D share hooks | **0** |

OpenXR is **runtime-agnostic** (not WiVRn-only).

---

## Docs

| Doc | Topic |
|-----|--------|
| [addon/vrmod-x64/docs/CONTROLLER_BINDINGS.md](addon/vrmod-x64/docs/CONTROLLER_BINDINGS.md) | OpenXR rebind / Quest gold |
| [addon/vrmod-x64/docs/MODULE_COMPAT.md](addon/vrmod-x64/docs/MODULE_COMPAT.md) | Module ↔ Lua compatibility |
| [addon/vrmod-x64/docs/VRMOD_RENDER_QUALITY.md](addon/vrmod-x64/docs/VRMOD_RENDER_QUALITY.md) | Render quality |
| [addon/vrmod-x64/docs/CREDITS.md](addon/vrmod-x64/docs/CREDITS.md) | Credits |

---

## Related repositories

| Repo | Role |
|------|------|
| **[gVRMod](https://github.com/Abyss-c0re/gVRMod)** | **Project face** — OpenXR module + submodule |
| [vrmod-x64](https://github.com/Abyss-c0re/vrmod-x64) | Lua addon sources (Workshop package layout) |
| [vrmod-module-master](https://github.com/Abyss-c0re/vrmod-module-master) | OpenVR native module |

---

## License

**CUBECHAIN** — see [LICENSE](LICENSE).

---

## OpenXR native launcher (HL2VR-style bg map)

Like HL2VR’s `background0x` maps: always load a **world under the menu**.

Default: **`map_background gm_construct`** + OpenXR + auto VR + freefloat MainMenu.

```bash
./scripts/gvrmod_launcher.sh                      # bg map = gm_construct
./scripts/gvrmod_launcher.sh --map gm_flatgrass   # other bg map
./scripts/gvrmod_launcher.sh --play-map           # full +map (not background)
./scripts/gvrmod_launcher.sh --hub                # hub + full construct
```

Native line (conceptually):

```text
hl2.sh -game garrysmod -windowed -w 1280 -h 720 \
  +exec gvrmod_menu +map_background gm_construct
```

