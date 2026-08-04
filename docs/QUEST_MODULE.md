# Quest thin module (gVRLink)

**Goal:** standalone Quest host owns OpenXR; PC GMod uses a **separate** binary that only talks gVRLink.  
**Law:** `gmcl_vrmod_xr_*` (WiVRn / local OpenXR) stays unchanged. Quest is **opt-in only**.

## Binaries

| Binary | require | Backend string |
|--------|---------|----------------|
| `gmcl_vrmod_xr_linux64.dll` | `vrmod_xr` | `openxr` |
| `gmcl_vrmod_linux64.dll` | `vrmod` | `openvr` |
| `gmcl_vrmod_quest_linux64.dll` | `vrmod_quest` | `quest` |

Build (Linux):

```bash
cmake -S . -B build -DVRMOD_BUILD_QUEST=ON
cmake --build build -j
# → install/GarrysMod/garrysmod/lua/bin/gmcl_vrmod_quest_linux64.dll
```

Copy next to the XR module in `garrysmod/lua/bin/`.

## Lua

```text
vrmod_prefer_backend quest   # explicit; never auto
```

- `auto` / `openxr` / `openvr` never load `vrmod_quest`.
- `vrmod.IsQuest()` true when thin module is active.
- If quest binary missing while prefer=quest, falls back to XR then OpenVR.

## Phase 1 (this commit)

| API | Behavior |
|-----|----------|
| Init | Bind UDP pose listen (default **27101**) |
| GetPoses / UpdatePoses | Apply last `GVP1` from Quest host |
| IsHMDPresent / ShouldRender | Recent pose packets |
| SubmitSharedTexture | **No-op** until texture TX lands |
| ShareTexture* | Soft stubs |

Env:

```bash
export GVRMOD_QUEST_POSE_PORT=27101
export GVRMOD_QUEST_HOST=192.168.x.x   # optional peer hint
```

## Phase 2 (later)

- Read dual OUT / SBS from ShareTexture path
- UDP fragment video TX to Quest `:27100`
- CAP handshake for bandwidth / FOV

## Protocol

`shared/gvlink/gvlink_proto.hpp` — layout only.  
Quest APK remains **private** (outside this tree).
