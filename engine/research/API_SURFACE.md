# GMod Lua API surface (static scan)

Scan date: 2026-10-09. Branch context: gVRMod `Dev`. This file is a research inventory for an engine compatibility layer. It is not a runtime trace.

## How this was counted

Roots, symlinks followed, `.git` directories skipped:

- `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/addons`
- `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/gamemodes`
- `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/lua`
- `/home/voldemar/Dev/GMod/gVRMod/addon`

GLua extensions the lexer accepts: `//` and `/* */` comments, `!=`, `&&`, `||`, and unary `!`, plus `continue`. A file "mentions" a global only when a parser resolves that identifier as a global read or a global assignment. Locals, function parameters, loop variables, fields after `.` or `:`, comments, and string literals do not count. `Name(` counts as a call (also `Name"..."` and `Name{...}`). `Name.Method` and `Name:Method` count only when `Name` itself is that global, not a local with the same spelling.

Unique `.lua` files after realpath dedupe: **903**. Parsed as GLua with no syntax errors: **902**. Wire Expression 2 file that is not GLua: **1**. Other parse failures: **0**.

`cube_home` and `vrmod-x64` are mounted in Steam as symlinks into `/home/voldemar/Dev/GMod/gVRMod/addon`. Frequency counts use one realpath, so those trees are not double-counted. `vrmod_arcvr` and `vrmod_climbing` are symlinks to their own git checkouts; `.git` was not walked.

Unique files by bucket: addon 307, base 276, gamemode 320.

Realpath overlap: addons∩base 0, addons∩gamemodes 0, gamemodes∩base 0.

## 1. Per-addon table

Lua file counts follow symlinks and skip `.git`. `lua/ symlink` is whether that addon's `lua` directory itself is a symlink (none of them are). `Entry symlink` is the addon directory. GMod's addon loader executes files under `lua/` only; files outside `lua/` are still counted because they are Lua we have on disk.

| Addon | Title | Path | Lua files | In `lua/` | Outside `lua/` | Entry symlink | `lua/` symlink |
|---|---|---|---:|---:|---:|---|---|
| `cube_home` | XR Home Passthrough (InfMap by Meetric) | `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/addons/cube_home` | 8 | 8 | 0 | yes → `/home/voldemar/Dev/GMod/gVRMod/addon/cube_home` | no |
| `cube_ws_2049617805` | Cube extracted WS 2049617805 | `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/addons/cube_ws_2049617805` | 0 | 0 | 0 | no | no |
| `cube_ws_2905327911` | Cube extracted WS 2905327911 | `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/addons/cube_ws_2905327911` | 24 | 24 | 0 | no | no |
| `cube_ws_2928366506` | Cube extracted WS 2928366506 | `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/addons/cube_ws_2928366506` | 9 | 9 | 0 | no | no |
| `cube_ws_3801628931` | Cube extracted WS 3801628931 | `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/addons/cube_ws_3801628931` | 0 | 0 | 0 | no | no |
| `vrmod-x64` | VRMod_x64 | `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/addons/vrmod-x64` | 141 | 138 | 3 | yes → `/home/voldemar/Dev/GMod/gVRMod/addon/vrmod-x64` | no |
| `vrmod_arcvr` | [VRMod]ArcVR:All in one | `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/addons/vrmod_arcvr` | 118 | 118 | 0 | yes → `/home/voldemar/Dev/GMod/vrmod_arcvr` | no |
| `vrmod_climbing` | [VRMod] Brush Climbing | `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/addons/vrmod_climbing` | 7 | 7 | 0 | yes → `/home/voldemar/Dev/GMod/vrmod_climbing` | no |

Steam addon `.lua` files, symlinks resolved, not yet deduped against the repo copy: **307**. Unique addon realpaths in the frequency corpus: **307**.

Outside `lua/`:

- `vrmod-x64`: `concept/algocube/cl_algocube_mirror.lua`, `concept/algocube/sh_algocube.lua`, `concept/algocube/sh_algocube_vision.lua`

Repo tree `/home/voldemar/Dev/GMod/gVRMod/addon` (same files as the Steam symlinks when the realpath matches):

| Directory | Path | Lua files | Same realpath as a Steam addon |
|---|---|---:|---|
| `cube_home` | `/home/voldemar/Dev/GMod/gVRMod/addon/cube_home` | 8 | yes |
| `vrmod-x64` | `/home/voldemar/Dev/GMod/gVRMod/addon/vrmod-x64` | 141 | yes |

### Gamemodes

These are not addons. Counted because the task inventory includes `garrysmod/gamemodes`. None of these directories are symlinks.

| Gamemode | Path | Lua files | Top-level breakdown |
|---|---|---:|---|
| `base` | `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/gamemodes/base` | 48 | `entities` 29, `gamemode` 19 |
| `sandbox` | `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/gamemodes/sandbox` | 130 | `entities` 80, `gamemode` 50 |
| `terrortown` | `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/gamemodes/terrortown` | 142 | `entities` 74, `gamemode` 68 |

Gamemode `.lua` total: **320**.

### Base `garrysmod/lua`

Path: `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/lua`. Lua files: **276**. The directory is not a symlink. This is stock GMod Lua (menu, vgui, extensions, autorun), not an addon.

| Subdirectory | Lua files |
|---|---:|
| `vgui` | 93 |
| `includes` | 85 |
| `autorun` | 34 |
| `menu` | 27 |
| `postprocess` | 14 |
| `derma` | 7 |
| `entities` | 6 |
| `drive` | 3 |
| `matproxy` | 3 |
| `weapons` | 3 |
| `skins` | 1 |

## 2. Top 60 globals by file-count

Universe: the fixed interesting set (64 exact names; `render` and `constraint` were listed twice in the request and counted once). The prefix `VRMOD_` is not one of those names; it is counted in the subsection below. Rank is **how many unique files resolve the name as a global** (read or assignment), out of 903. `Raw files` is the looser count of code identifiers with that spelling that are not preceded by `.` or `:` — it still includes locals that shadow the global, so it is an upper bound, not the compat number. `Call files` is how many files invoke it as `Name(`, `Name"..."`, or `Name{...}`. `Hits` are code-token occurrences in files where the parser agreed the name is a global; a file that also has a local of the same name can inflate hits, not the file count.

| Rank | Global | Files | % of corpus | Addon files | Raw files | Call files | Hits |
|---:|---|---:|---:|---:|---:|---:|---:|
| 1 | `IsValid` | 451 | 49.9% | 120 | 451 | 443 | 2881 |
| 2 | `Vector` | 279 | 30.9% | 181 | 279 | 279 | 2012 |
| 3 | `hook` | 256 | 28.3% | 122 | 256 | 0 | 1102 |
| 4 | `AddCSLuaFile` | 256 | 28.3% | 132 | 256 | 256 | 365 |
| 5 | `SERVER` | 251 | 27.8% | 116 | 251 | 0 | 422 |
| 6 | `util` | 237 | 26.2% | 78 | 237 | 0 | 756 |
| 7 | `CLIENT` | 229 | 25.4% | 99 | 230 | 0 | 420 |
| 8 | `Color` | 224 | 24.8% | 95 | 226 | 219 | 973 |
| 9 | `Angle` | 197 | 21.8% | 155 | 202 | 197 | 1985 |
| 10 | `g_VR` | 181 | 20.0% | 181 | 181 | 0 | 3671 |
| 11 | `vgui` | 180 | 19.9% | 18 | 180 | 0 | 831 |
| 12 | `CurTime` | 161 | 17.8% | 52 | 161 | 160 | 568 |
| 13 | `LocalPlayer` | 155 | 17.2% | 83 | 155 | 154 | 588 |
| 14 | `vrmod` | 144 | 15.9% | 144 | 144 | 0 | 4716 |
| 15 | `surface` | 138 | 15.3% | 41 | 138 | 0 | 1430 |
| 16 | `timer` | 134 | 14.8% | 75 | 134 | 0 | 402 |
| 17 | `ents` | 117 | 13.0% | 43 | 119 | 0 | 291 |
| 18 | `concommand` | 105 | 11.6% | 52 | 105 | 0 | 209 |
| 19 | `RunConsoleCommand` | 100 | 11.1% | 35 | 100 | 99 | 381 |
| 20 | `net` | 94 | 10.4% | 47 | 95 | 0 | 1299 |
| 21 | `render` | 86 | 9.5% | 29 | 86 | 0 | 774 |
| 22 | `GetConVar` | 84 | 9.3% | 45 | 84 | 83 | 278 |
| 23 | `game` | 83 | 9.2% | 29 | 85 | 0 | 185 |
| 24 | `draw` | 75 | 8.3% | 27 | 75 | 0 | 353 |
| 25 | `include` | 73 | 8.1% | 11 | 73 | 73 | 335 |
| 26 | `list` | 64 | 7.1% | 1 | 87 | 0 | 598 |
| 27 | `CreateConVar` | 61 | 6.8% | 11 | 61 | 61 | 220 |
| 28 | `player` | 56 | 6.2% | 20 | 80 | 0 | 148 |
| 29 | `language` | 47 | 5.2% | 4 | 47 | 0 | 127 |
| 30 | `constraint` | 37 | 4.1% | 6 | 38 | 0 | 70 |
| 31 | `bit` | 35 | 3.9% | 16 | 37 | 0 | 132 |
| 32 | `FrameTime` | 34 | 3.8% | 12 | 35 | 34 | 76 |
| 33 | `gui` | 34 | 3.8% | 5 | 34 | 0 | 136 |
| 34 | `cam` | 34 | 3.8% | 20 | 34 | 0 | 113 |
| 35 | `input` | 33 | 3.7% | 3 | 41 | 0 | 68 |
| 36 | `undo` | 30 | 3.3% | 1 | 32 | 0 | 238 |
| 37 | `engine` | 30 | 3.3% | 10 | 30 | 0 | 64 |
| 38 | `duplicator` | 28 | 3.1% | 0 | 28 | 0 | 109 |
| 39 | `cvars` | 26 | 2.9% | 11 | 28 | 0 | 47 |
| 40 | `sound` | 21 | 2.3% | 3 | 26 | 0 | 27 |
| 41 | `gamemode` | 19 | 2.1% | 0 | 25 | 0 | 58 |
| 42 | `cleanup` | 19 | 2.1% | 1 | 20 | 0 | 26 |
| 43 | `numpad` | 18 | 2.0% | 1 | 18 | 0 | 113 |
| 44 | `scripted_ents` | 16 | 1.8% | 3 | 16 | 0 | 29 |
| 45 | `system` | 15 | 1.7% | 9 | 16 | 0 | 27 |
| 46 | `Matrix` | 15 | 1.7% | 15 | 15 | 15 | 29 |
| 47 | `steamworks` | 14 | 1.6% | 0 | 14 | 0 | 43 |
| 48 | `properties` | 14 | 1.6% | 0 | 14 | 0 | 39 |
| 49 | `Entity` | 13 | 1.4% | 2 | 18 | 13 | 29 |
| 50 | `weapons` | 11 | 1.2% | 3 | 12 | 0 | 13 |
| 51 | `chat` | 6 | 0.7% | 2 | 7 | 0 | 17 |
| 52 | `notification` | 6 | 0.7% | 4 | 6 | 0 | 11 |
| 53 | `cookie` | 5 | 0.6% | 1 | 6 | 0 | 22 |
| 54 | `sql` | 5 | 0.6% | 0 | 5 | 0 | 44 |
| 55 | `gmod` | 3 | 0.3% | 0 | 4 | 0 | 12 |
| 56 | `achievements` | 3 | 0.3% | 0 | 3 | 0 | 5 |
| 57 | `mesh` | 2 | 0.2% | 2 | 4 | 0 | 18 |
| 58 | `killicon` | 2 | 0.2% | 0 | 2 | 0 | 46 |
| 59 | `physenv` | 2 | 0.2% | 2 | 2 | 0 | 2 |
| 60 | `umsg` | 1 | 0.1% | 0 | 1 | 0 | 8 |

### Rest of the fixed set

Same columns. Included so a zero is visible instead of looking like the name was not scanned.

| Rank | Global | Files | % of corpus | Addon files | Raw files | Call files | Hits |
|---:|---|---:|---:|---:|---:|---:|---:|
| 61 | `effects` | 1 | 0.1% | 1 | 1 | 0 | 1 |
| 62 | `resource` | 1 | 0.1% | 1 | 1 | 0 | 1 |
| 63 | `Player` | 0 | 0.0% | 0 | 5 | 0 | 0 |
| 64 | `usermessage` | 0 | 0.0% | 0 | 0 | 0 | 0 |

### `vrmod`, `g_VR`, and `VRMOD_`

- Code identifier resolved as the global `vrmod`: **144** files. Word-boundary text match `(?<![\w])vrmod(?![\w])` in the raw file, including comments and strings: **145**. Substring `vrmod` (also matches `vrmod_climbing`, paths, and similar): **150**.
- Code identifier resolved as the global `g_VR`: **181** files. Word-boundary text match: **181**. Substring `g_VR`: **181**.
- Identifier prefix `VRMOD_` in code tokens: **45** distinct names across **12** files. Names: `VRMOD_CollectEyes`, `VRMOD_GetActions`, `VRMOD_GetBackend`, `VRMOD_GetControllerSources`, `VRMOD_GetDisplayInfo`, `VRMOD_GetPoses`, `VRMOD_GetTrackedDeviceNames`, `VRMOD_GetVersion`, `VRMOD_Init`, `VRMOD_IsHMDPresent`, `VRMOD_KeyboardAppend`, `VRMOD_KeyboardClose`, `VRMOD_KeyboardGetInfo`, `VRMOD_KeyboardGetKeys`, `VRMOD_KeyboardGetText`, `VRMOD_KeyboardHitTest`, `VRMOD_KeyboardIsOpen`, `VRMOD_KeyboardOpen`, `VRMOD_KeyboardPointerClick`, `VRMOD_KeyboardSetText`, `VRMOD_KeyboardSystemAvailable`, `VRMOD_SetActionManifest`, `VRMOD_SetActiveActionSets`, `VRMOD_SetEnvironmentBlendMode`, `VRMOD_SetKnownSubmitSize`, `VRMOD_SetPassthroughChroma`, `VRMOD_SetPassthroughChromaKey`, `VRMOD_SetPassthroughChromaMask`, `VRMOD_SetRTTextureFlip`, `VRMOD_SetSubmitCropMode`, `VRMOD_SetSubmitEnabled`, `VRMOD_SetSubmitTextureBounds`, `VRMOD_ShareTextureBegin`, `VRMOD_ShareTextureFinish`, `VRMOD_ShouldRender`, `VRMOD_Shutdown`, `VRMOD_SubmitSharedTexture`, `VRMOD_TriggerHaptic`, `VRMOD_UpdatePosesAndActions`, `VRMOD_VirtualDisplayCaptureWindow`, `VRMOD_VirtualDisplayCreate`, `VRMOD_VirtualDisplayDestroy`, `VRMOD_VirtualDisplayGetInfo`, `VRMOD_VirtualDisplayIsSupported`, `VRMOD_VirtualDisplayResize`.
- Raw substring `VRMOD_`: **15** files.

Those `vrmod` code-identifier files, by addon realpath:

- `vrmod-x64`: 139
- `vrmod_climbing`: 3
- `cube_home`: 1
- `vrmod_arcvr`: 1

### Direct `Global.Method` / `Global:Method`

Only the fixed set. A hit is a code token `Global.Method` or `Global:Method` in a file where the parser also resolved that global. Colon calls on locals (`ply:Nick()`, `self:Think()`) are not in this table; they are instance methods in section 3. `function Global.Field()` definitions count as a use of `Field`. A nested call `Global.Table.Method()` still produces a `Global.Table` token hit, so `vrmod.utils` is the utils table, not one function.

Top 40 by file count:

| Rank | Symbol | Files | Token hits |
|---:|---|---:|---:|
| 1 | `hook.Add` | 205 | 609 |
| 2 | `vgui.Create` | 153 | 648 |
| 3 | `timer.Simple` | 111 | 228 |
| 4 | `concommand.Add` | 104 | 206 |
| 5 | `surface.SetDrawColor` | 97 | 381 |
| 6 | `ents.Create` | 75 | 124 |
| 7 | `hook.Run` | 70 | 146 |
| 8 | `net.Start` | 63 | 137 |
| 9 | `util.TraceLine` | 61 | 106 |
| 10 | `surface.DrawRect` | 56 | 272 |
| 11 | `hook.Remove` | 53 | 226 |
| 12 | `net.Receive` | 49 | 113 |
| 13 | `vrmod.utils` | 48 | 965 |
| 14 | `render.SetMaterial` | 48 | 76 |
| 15 | `vgui.Register` | 47 | 55 |
| 16 | `list.Set` | 46 | 446 |
| 17 | `util.Effect` | 44 | 86 |
| 18 | `hook.Call` | 43 | 104 |
| 19 | `language.GetPhrase` | 42 | 100 |
| 20 | `surface.SetMaterial` | 40 | 58 |
| 21 | `game.SinglePlayer` | 39 | 69 |
| 22 | `timer.Create` | 39 | 62 |
| 23 | `surface.DrawTexturedRect` | 37 | 54 |
| 24 | `draw.SimpleText` | 36 | 197 |
| 25 | `net.WriteEntity` | 36 | 68 |
| 26 | `util.AddNetworkString` | 33 | 82 |
| 27 | `net.ReadEntity` | 32 | 62 |
| 28 | `net.SendToServer` | 31 | 62 |
| 29 | `net.WriteUInt` | 30 | 109 |
| 30 | `undo.AddEntity` | 29 | 80 |
| 31 | `undo.Create` | 29 | 43 |
| 32 | `undo.Finish` | 29 | 43 |
| 33 | `undo.SetPlayer` | 29 | 43 |
| 34 | `surface.DrawOutlinedRect` | 28 | 50 |
| 35 | `net.ReadUInt` | 27 | 85 |
| 36 | `ents.FindByClass` | 27 | 59 |
| 37 | `draw.RoundedBox` | 27 | 52 |
| 38 | `util.TableToJSON` | 27 | 49 |
| 39 | `list.Get` | 27 | 44 |
| 40 | `net.WriteBool` | 25 | 59 |

Up to five methods per global, and only methods seen in at least 3 files:

- `hook`: `Add` (205 files), `Run` (70 files), `Remove` (53 files), `Call` (43 files), `GetTable` (8 files)
- `util`: `TraceLine` (61 files), `Effect` (44 files), `AddNetworkString` (33 files), `TableToJSON` (27 files), `IsValidPhysicsObject` (21 files), +25 more with ≥3 files
- `vgui`: `Create` (153 files), `Register` (47 files), `CreateFromTable` (10 files), `GetHoveredPanel` (7 files), `RegisterTable` (7 files), +4 more with ≥3 files
- `vrmod`: `utils` (48 files), `Toast` (21 files), `GetConvars` (15 files), `AddCallbackedConvar` (13 files), `GetSecondaryHand` (12 files), +79 more with ≥3 files
- `surface`: `SetDrawColor` (97 files), `DrawRect` (56 files), `SetMaterial` (40 files), `DrawTexturedRect` (37 files), `DrawOutlinedRect` (28 files), +13 more with ≥3 files
- `timer`: `Simple` (111 files), `Create` (39 files), `Remove` (20 files), `Exists` (6 files)
- `ents`: `Create` (75 files), `FindByClass` (27 files), `GetAll` (13 files), `FindInSphere` (12 files), `FindInBox` (6 files), +4 more with ≥3 files
- `concommand`: `Add` (104 files)
- `net`: `Start` (63 files), `Receive` (49 files), `WriteEntity` (36 files), `ReadEntity` (32 files), `SendToServer` (31 files), +23 more with ≥3 files
- `render`: `SetMaterial` (48 files), `Clear` (14 files), `DrawBeam` (14 files), `DrawScreenQuad` (13 files), `SetColorModulation` (12 files), +42 more with ≥3 files
- `game`: `SinglePlayer` (39 files), `GetMap` (17 files), `GetWorld` (16 files), `MaxPlayers` (5 files), `CleanUpMap` (5 files), +1 more with ≥3 files
- `draw`: `SimpleText` (36 files), `RoundedBox` (27 files), `DrawText` (7 files), `SimpleTextOutlined` (6 files), `NoTexture` (5 files)
- `list`: `Set` (46 files), `Get` (27 files), `GetEntry` (14 files), `Add` (3 files)
- `player`: `Iterator` (23 files), `GetAll` (21 files), `GetBySteamID` (7 files), `GetBySteamID64` (6 files)
- `language`: `GetPhrase` (42 files), `FormatPhrase` (9 files), `Add` (3 files)
- `constraint`: `RemoveConstraints` (15 files), `Weld` (13 files), `Rope` (4 files), `GetTable` (3 files), `NoCollide` (3 files)
- `bit`: `band` (19 files), `bor` (15 files), `bnot` (3 files)
- `gui`: `MouseY` (13 files), `MouseX` (11 files), `EnableScreenClicker` (10 files), `IsGameUIVisible` (5 files), `ActivateGameUI` (5 files), +1 more with ≥3 files
- `cam`: `Start2D` (13 files), `End2D` (12 files), `Start3D` (12 files), `End3D` (11 files), `End3D2D` (9 files), +4 more with ≥3 files
- `input`: `GetCursorPos` (12 files), `IsKeyDown` (5 files), `SetCursorPos` (5 files), `IsMouseDown` (5 files), `IsShiftDown` (4 files)
- `undo`: `AddEntity` (29 files), `Create` (29 files), `Finish` (29 files), `SetPlayer` (29 files), `SetCustomUndoText` (16 files)
- `engine`: `ActiveGamemode` (7 files), `GetAddons` (6 files), `GetGamemodes` (5 files), `IsPlayingDemo` (5 files), `TickInterval` (4 files), +1 more with ≥3 files
- `duplicator`: `DoGeneric` (13 files), `RegisterEntityClass` (12 files), `DoGenericPhysics` (11 files), `RegisterEntityModifier` (7 files), `StoreEntityModifier` (7 files), +2 more with ≥3 files
- `cvars`: `AddChangeCallback` (21 files), `RemoveChangeCallback` (4 files), `Bool` (3 files), `Number` (3 files)
- `sound`: `Play` (19 files)
- `gamemode`: `Call` (18 files)
- `cleanup`: `Register` (14 files)
- `numpad`: `Register` (10 files), `OnDown` (9 files), `OnUp` (8 files), `Remove` (7 files), `Activate` (3 files), +1 more with ≥3 files
- `scripted_ents`: `Register` (8 files), `GetMember` (5 files)
- `system`: `IsLinux` (11 files), `IsWindows` (6 files)
- `steamworks`: `ViewFile` (6 files), `FileInfo` (5 files), `DownloadUGC` (4 files), `ShouldMountAddon` (3 files), `Publish` (3 files)
- `properties`: `Add` (14 files), `CanBeTargeted` (12 files)
- `weapons`: `GetList` (5 files), `GetStored` (5 files)
- `chat`: `AddText` (5 files)
- `notification`: `AddLegacy` (5 files)
- `cookie`: `Set` (5 files), `GetNumber` (3 files), `GetString` (3 files)
- `sql`: `Query` (5 files), `QueryValue` (5 files), `TableExists` (4 files)

### Other globals outside the fixed set

Same parser rule (global read or assignment), top 40 by file count. These are not in the requested set, but a compatibility layer still has to provide the ones that are engine or Lua builtins. Addon-defined names (`ENT`, `SWEP`, `InfMap`, `vrmod` is in the fixed set) show up here when many files share them. This list is the whole unique corpus, not just workshop addons.

| Rank | Global | Files | Addon files |
|---:|---|---:|---:|
| 1 | `math` | 330 | 123 |
| 2 | `table` | 266 | 83 |
| 3 | `pairs` | 243 | 76 |
| 4 | `string` | 214 | 100 |
| 5 | `tostring` | 187 | 109 |
| 6 | `ipairs` | 181 | 82 |
| 7 | `tonumber` | 137 | 75 |
| 8 | `Material` | 114 | 35 |
| 9 | `ENT` | 100 | 17 |
| 10 | `AccessorFunc` | 99 | 0 |
| 11 | `SWEP` | 99 | 51 |
| 12 | `derma` | 95 | 2 |
| 13 | `type` | 78 | 58 |
| 14 | `pcall` | 76 | 72 |
| 15 | `FCVAR_ARCHIVE` | 70 | 40 |
| 16 | `ScrW` | 70 | 13 |
| 17 | `isfunction` | 70 | 53 |
| 18 | `istable` | 68 | 28 |
| 19 | `ArcticVR` | 67 | 67 |
| 20 | `ScrH` | 67 | 12 |
| 21 | `CreateClientConVar` | 65 | 37 |
| 22 | `LANG` | 60 | 0 |
| 23 | `file` | 60 | 32 |
| 24 | `print` | 58 | 39 |
| 25 | `Sound` | 56 | 5 |
| 26 | `color_white` | 54 | 18 |
| 27 | `FILL` | 51 | 5 |
| 28 | `GAMEMODE` | 51 | 1 |
| 29 | `isstring` | 50 | 11 |
| 30 | `NULL` | 48 | 16 |
| 31 | `TOP` | 47 | 6 |
| 32 | `EffectData` | 44 | 10 |
| 33 | `GM` | 44 | 0 |
| 34 | `SOLID_VPHYSICS` | 44 | 18 |
| 35 | `TEXT_ALIGN_CENTER` | 44 | 27 |
| 36 | `DEFINE_BASECLASS` | 43 | 2 |
| 37 | `SysTime` | 42 | 12 |
| 38 | `TOOL` | 39 | 1 |
| 39 | `module` | 38 | 0 |
| 40 | `Model` | 37 | 0 |

## 3. Boot subset for the small addons

"Short" means fewer than 30 `.lua` files under that addon directory. That is every `cube_ws_*` addon plus `cube_home` (8) and `vrmod_climbing` (7). `vrmod-x64` (141) and `vrmod_arcvr` (118) are out of scope for the per-file list.

A symbol is in an addon's **boot subset** when some file resolves it as a global read and no file in that same addon assigns it, **or** the addon reads an engine global and then replaces it (`or CreateSound` before `function CreateSound`). Names the addon creates itself (`InfMap = ...`, `function Foo`, `function SoundObject`) are not engine API. `Name = Name or {}` does not throw if `Name` is nil; that case is called out when the name is actually a host table (`g_VR`, `vrmod`). Lua builtins are still listed, because a bare environment that lacks `math` or `string` will error. Instance methods (`:GetPos`, panel `:Dock`) are not globals; they are listed separately because the line errors when it runs, not when the file is compiled.

Two `cube_ws_*` trees have no Lua at all (map/material packs). Their boot subset is empty.

### Recommended boot subset (`cube_ws_*`)

This is the union of symbols each `cube_ws_*` addon reads and does not define itself. Loading both map packs together still needs every name below. `InfMap` is **not** in this list when the InfMap pack defines it; it **is** included if another pack only reads it. Check the per-addon notes.

Workshop addons considered: `cube_ws_2049617805` (0 lua), `cube_ws_2905327911` (24 lua), `cube_ws_2928366506` (9 lua), `cube_ws_3801628931` (0 lua).

**Union**

Shared list for every `cube_ws_*` Lua file we have. Map-only packs add nothing.

External globals (97): read by at least one of these packs and not assigned by that same pack, plus engine names that pack reads and then replaces.

Libraries / tables (methods the files actually call, then plain fields):

- `E2Helper` — fields `Descriptions`
- `E2Lib` — methods `RegisterExtension`
- `ENT` — methods `BuildCollision`, `Draw`, `GenerateMesh`, `GenerateTrees`, `GetRenderMesh`, `Initialize`, `InitializeClient`, `InitializePhysics`, `OnRemove`, `SetLocalRenderBounds`, `SetReferenceData`, `SetupDataTables`, `Spawn`, `Think`, `TryOptimizeCollision`, `UpdateCollision`, `Use`; fields `AdminOnly`, `Author`, `Base`, `Category`, `Editable`, `Information`, `Instructions`, `Model`, `PrintName`, `Purpose`, `Spawnable`, `TargetName`, `Type`
- `InfMap` — methods `clear_parsed_objects`, `parse_obj`, `prop_update_chunk`, `unlocalize_vector`; fields `chunk_size`, `disable_pickup`, `filter`, `water_height`, `water_material`
- `SF` — methods `clampPos`
- `WireLib` — methods `clampPos`
- `bit` — methods `band`
- `cam` — methods `End2D`, `End3D`, `PopModelMatrix`, `PushModelMatrix`, `Start2D`, `Start3D`; also referenced without calling `End3D`, `PopModelMatrix`, `PushModelMatrix`, `Start3D`
- `constraint` — methods `GetTable`, `Weld`
- `coroutine` — methods `create`, `resume`, `status`, `wait`, `yield`
- `engine` — methods `ActiveGamemode`
- `ents` — methods `Create`, `CreateClientProp`, `CreateClientside`, `FindByClass`, `FindInBox`, `FindInCone`, `FindInSphere`, `GetAll`; also referenced without calling `FindInBox`, `FindInCone`, `FindInSphere`
- `file` — methods `Find`, `Read`
- `game` — methods `GetMap`, `GetWorld`, `SinglePlayer`
- `gmsave` — methods `ShouldSaveEntity`; also referenced without calling `ShouldSaveEntity`
- `hook` — methods `Add`, `Remove`, `Run`
- `math` — methods `Clamp`, `Round`, `abs`, `floor`, `max`, `min`, `pow`, `random`, `sqrt`; fields `huge`; also referenced without calling `Clamp`, `floor`
- `net` — methods `ReadAngle`, `ReadEntity`, `ReadFloat`, `ReadString`, `ReadUInt`, `Receive`, `Send`, `Start`, `WriteAngle`, `WriteEntity`, `WriteFloat`, `WriteString`, `WriteUInt`
- `physenv` — methods `SetPerformanceSettings`
- `player` — methods `GetAll`
- `render` — methods `DrawWireframeBox`, `DrawWireframeSphere`, `EnableClipping`, `GetLightColor`, `PopCustomClipPlane`, `PopFlashlightMode`, `PushCustomClipPlane`, `PushFlashlightMode`, `ResetModelLighting`, `SetLocalModelLights`, `SetMaterial`; fields `DrawBox`, `SetLightmapTexture`, `SetModelLighting`; also referenced without calling `SetLocalModelLights`, `SetMaterial`
- `resource` — methods `AddWorkshop`
- `sound` — methods `GetProperties`
- `string` — methods `Explode`, `Split`, `Trim`, `find`, `gsub`, `len`, `lower`, `sub`
- `surface` — methods `DrawRect`, `SetDrawColor`
- `table` — methods `Add`, `Copy`, `Count`, `Empty`, `RemoveByValue`, `insert`, `remove`; also referenced without calling `insert`
- `timer` — methods `Create`, `Simple`
- `util` — methods `AddNetworkString`, `BlastDamage`, `GetModelMeshes`, `GetSunInfo`, `GetSurfaceData`, `GetSurfaceIndex`, `IsInWorld`, `IsValidModel`, `SharedRandom`, `TraceEntity`, `TraceHull`, `TraceLine`; fields `IntersectRayWithPlane`; also referenced without calling `BlastDamage`, `TraceEntity`, `TraceHull`, `TraceLine`

Functions and constructors: `AddCSLuaFile`, `Angle`, `Color`, `CreateClientConVar`, `CreateMaterial`, `CurTime`, `DEFINE_BASECLASS`, `DrawMaterialOverlay`, `ErrorNoHalt`, `EyePos`, `EyeVector`, `FindMetaTable`, `FrameTime`, `GetConVar`, `GetRenderTarget`, `IsValid`, `LocalPlayer`, `Material`, `Matrix`, `Mesh`, `RunConsoleCommand`, `SafeRemoveEntity`, `Vector`, `__e2setcost`, `error`, `include`, `ipairs`, `isentity`, `pairs`, `pcall`, `print`, `tonumber`, `tostring`, `unpack`.

Constants and realm-style uppercase names: `ACT_MP_SWIM`, `CHAN_BODY`, `CLIENT`, `COLLISION_GROUP_WORLD`, `DMG_BURN`, `EFL_SERVER_ONLY`, `EF_DIMLIGHT`, `FL_STATICPROP`, `FSOLID_FORCE_WORLD_ALIGNED`, `FVPHYSICS_CONSTRAINT_STATIC`, `FVPHYSICS_NO_PLAYER_PICKUP`, `FVPHYSICS_NO_SELF_COLLISIONS`, `IN_JUMP`, `MATERIAL_FOG_LINEAR`, `MATERIAL_LIGHT_DIRECTIONAL`, `MATERIAL_TRIANGLES`, `MOVETYPE_NOCLIP`, `MOVETYPE_NONE`, `MOVETYPE_VPHYSICS`, `NULL`, `RENDERMODE_NONE`, `RENDERMODE_NORMAL`, `RENDERMODE_TRANSCOLOR`, `SERVER`, `SIMPLE_USE`, `SOLID_BBOX`, `SOLID_NONE`, `SOLID_VPHYSICS`.

Other reads (no direct call and no direct field in this addon; still must exist if that line runs): `IsEntity`, `istable`, `mesh`, `pos`, `vector_origin`.

Engine globals read and then replaced (`or Name` before `function Name`): `CreateSound`, `ParticleEffect`. The original has to exist first; the wrapper does not create it.

Assigned by at least one of these packs (that pack does not need the engine to create it; another pack in the union may still read it): `InfMap`, `PlaceCollidersChunks`, `SoundObject`, `chunks_to_render`, `convertToThreeDigits`, `lod_to_render`, `water_to_render`.

Instance methods invoked on values (`obj:Method` or a call on a local's field). These are not globals: `AddAngleVelocity`, `AddFlags`, `AddGameFlag`, `AddSolidFlags`, `AdvanceVertex`, `Alive`, `ApplyForceCenter`, `ApplyForceOffset`, `Begin`, `BoundingRadius`, `BuildCollision`, `BuildFromTriangles`, `CallOnRemove`, `ChangeVolume`, `ClearRenderTarget`, `ConCommand`, `Cross`, `Destroy`, `DistToSqr`, `Distance`, `DontDeleteOnRemove`, `Dot`, `Draw`, `DrawModel`, `DrawQuadEasy`, `DrawShadow`, `DrawSphere`, `EmitSound`, `EnableCustomCollisions`, `EnableDrag`, `EnableMotion`, `End`, `EntIndex`, `Extinguish`, `FlashlightIsOn`, `FogColor`, `FogEnd`, `FogMaxDensity`, `FogMode`, `FogStart`, `ForcePlayerDrop`, `Forward`, `GenerateMesh`, `GenerateTrees`, `GetAmbientLightColor`, `GetAngleVelocity`, `GetAngles`, `GetBoneSurfaceProp`, `GetBool`, `GetButtons`, `GetChildren`, `GetClass`, `GetCollisionGroup`, `GetDriver`, `GetEntity`, `GetFloat`, `GetForwardSpeed`, `GetGravity`, `GetInflictor`, `GetKeyValues`, `GetMass`, `GetMaterial`, `GetMaterials`, `GetMaxSpeed`, `GetMesh`, `GetModel`, `GetModelRenderBounds`, `GetMoveAngles`, `GetMoveType`, `GetNW2Vector`, `GetName`, `GetNextBot`, `GetNoDraw`, `GetNormalized`, `GetOwner`, `GetParent`, `GetPhysicsObject`, `GetPhysicsObjectCount`, `GetPhysicsObjectNum`, `GetPlanetRadius`, `GetPos`, `GetRangeSquaredTo`, `GetReferenceParent`, `GetRenderBounds`, `GetRotatedAABB`, `GetSideSpeed`, `GetTexture`, `GetTranslation`, `GetUpSpeed`, `GetVelocity`, `GetWeaponColor`, `GetWeapons`, `InVehicle`, `InfMap_ApplyForceOffset`, `InfMap_Approach`, `InfMap_CalculateForceOffset`, `InfMap_CalculateVelocityOffset`, `InfMap_DoShootEffect`, `InfMap_EyePos`, `InfMap_FaceTowards`, `InfMap_GetAttachment`, `InfMap_GetBonePosition`, `InfMap_GetDamagePosition`, `InfMap_GetPos`, `InfMap_GetShootPos`, `InfMap_GetVelocityAtPoint`, `InfMap_LocalToWorld`, `InfMap_NearestPoint`, `InfMap_SetEntity`, `InfMap_SetMaterial`, `InfMap_SetPos`, `InfMap_SetRenderBounds`, `InfMap_Spawn`, `InfMap_Stop`, `InfMap_WorldSpaceAABB`, `InfMap_WorldSpaceCenter`, `InfMap_WorldToLocal`, `Initialize`, `InitializeClient`, `InitializePhysics`, `IsAsleep`, `IsConstraint`, `IsDamageType`, `IsEFlagSet`, `IsExplosionDamage`, `IsMoveable`, `IsNPC`, `IsOnFire`, `IsOnGround`, `IsPlayer`, `IsPlayerHolding`, `IsRagdoll`, `IsSolid`, `IsValid`, `IsVehicle`, `IsWeapon`, `IsWorld`, `Length`, `LengthSqr`, `LocalToWorld`, `NetworkVar`, `Noise2D`, `Noise3D`, `Normal`, `OBBMaxs`, `OBBMins`, `OldRenderOverride`, `OverrideDepthEnable`, `PhysicsDestroy`, `PhysicsFromMesh`, `PhysicsInit`, `PopRenderTarget`, `Position`, `PushRenderTarget`, `Remove`, `RemoveEffects`, `ResetModelLighting`, `Right`, `Rotate`, `SetAngleVelocity`, `SetAngles`, `SetCollisionGroup`, `SetColor`, `SetCustomCollisionCheck`, `SetDSP`, `SetFloat`, `SetKeyValue`, `SetLocalModelLights`, `SetLocalRenderBounds`, `SetMass`, `SetMaterial`, `SetModel`, `SetModelLighting`, `SetModelScale`, `SetMoveType`, `SetNW2Vector`, `SetName`, `SetNextClientThink`, `SetNoDraw`, `SetNotSolid`, `SetOwner`, `SetPlanetRadius`, `SetPos`, `SetReferenceData`, `SetReferenceParent`, `SetRenderBounds`, `SetRenderBoundsWS`, `SetRenderMode`, `SetScale`, `SetSolid`, `SetTexture`, `SetTranslation`, `SetUseType`, `SetVector`, `SetVelocity`, `SetWeaponColor`, `Sleep`, `Spawn`, `Stop`, `StopSound`, `TexCoord`, `TryOptimizeCollision`, `Up`, `UpdateCollision`, `UserData`, `WithinAABox`, `WorldSpaceCenter`, `abs`, `floor`, `insert`, `sin`, `sqrt`.

`E2Lib`, `E2Helper`, `__e2setcost`, `WireLib`, and `SF` are Wire or Starfall, not stock GMod. `pos` is an unbound name in one detour, not an API. `mesh` and `IsEntity` are read only so a local can alias them. The per-addon notes below say which file.

Per-addon boot sets follow the file lists, so a name that one pack defines is not silently required from the engine.

### `cube_home`

Path: `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/addons/cube_home`. Title: XR Home Passthrough (InfMap by Meetric). Lua files: **8**.
Entry symlink → `/home/voldemar/Dev/GMod/gVRMod/addon/cube_home`.

Boot subset (67 external globals):

Libraries / tables:

- `InfMap` — methods `ezcoord`, `filter_entities`, `height_function`, `localize_vector`, `planet_height_function`, `planet_info`, `prop_update_chunk`; fields `chunk_resolution`, `chunk_size`, `chunk_table`, `client_chunks`, `disable_pickup`, `filter`, `megachunk_size`, `planet_data`, `planet_render_distance`, `planet_resolution`, `planet_spacing`, `planet_tree_resolution`, `planet_uv_scale`, `render_distance`, `render_max_height`, `simplex`, `terrain_material`; also referenced without calling `height_function`, `prop_update_chunk`
- `cam` — methods `IgnoreZ`, `PopModelMatrix`, `PushModelMatrix`
- `chat` — methods `AddText`
- `concommand` — methods `Add`
- `constraint` — methods `Weld`
- `cvars` — methods `AddChangeCallback`
- `draw` — methods `SimpleTextOutlined`
- `ents` — methods `Create`, `CreateClientside`, `GetAll`
- `file` — methods `CreateDir`, `Exists`, `Read`, `Write`
- `g_VR` — fields `active`, `menuItems`
- `game` — methods `GetMap`, `GetWorld`, `SinglePlayer`; also referenced without calling `GetMap`
- `hook` — methods `Add`, `Run`
- `list` — methods `Set`
- `math` — methods `Clamp`, `abs`, `max`, `random`
- `os` — methods `time`
- `physenv` — methods `SetPerformanceSettings`
- `render` — methods `Clear`, `CullMode`, `FogMode`, `OverrideDepthEnable`, `ResetModelLighting`, `SetColorModulation`, `SetLocalModelLights`, `SetMaterial`
- `string` — methods `format`, `lower`
- `table` — methods `Copy`, `insert`, `remove`
- `timer` — methods `Simple`
- `util` — methods `TableToJSON`; fields `JSONToTable`
- `vgui` — methods `Create`
- `vrmod` — methods `AddInGameMenuItem`, `GetNativePolicy`, `RemoveInGameMenuItem`; fields `GetBackend`; also referenced without calling `AddInGameMenuItem`, `GetNativePolicy`, `RemoveInGameMenuItem`

Functions and constructors: `Angle`, `Color`, `CreateClientConVar`, `ErrorNoHalt`, `EyePos`, `FrameNumber`, `IsValid`, `LocalPlayer`, `Material`, `Matrix`, `Mesh`, `RunConsoleCommand`, `SafeRemoveEntity`, `VRMOD_GetBackend`, `VRMOD_SetEnvironmentBlendMode`, `VRMOD_SetPassthroughChroma`, `VRMOD_SetPassthroughChromaKey`, `VRMOD_SetPassthroughChromaMask`, `Vector`, `include`, `ipairs`, `isangle`, `isfunction`, `isstring`, `istable`, `isvector`, `pairs`, `pcall`, `print`, `tobool`, `tonumber`, `tostring`.

Constants and uppercase names: `CLIENT`, `FCVAR_ARCHIVE`, `FILL`, `MATERIAL_CULLMODE_CCW`, `MATERIAL_CULLMODE_NONE`, `MATERIAL_FOG_NONE`, `RENDERMODE_NONE`, `RENDERMODE_NORMAL`, `RENDERMODE_TRANSCOLOR`, `SERVER`, `TEXT_ALIGN_CENTER`.

Other reads: `color_white`.

Defined in this addon: `CubeHome`.

Instance methods: `Activate`, `BuildFromTriangles`, `ChatPrint`, `Distance`, `Dock`, `DockMargin`, `Draw`, `DrawShadow`, `EnableMotion`, `EyeAngles`, `EyePos`, `GenerateMesh`, `GetBool`, `GetFloat`, `GetPhysicsObject`, `GetPos`, `InfMap_SetPos`, `IsError`, `IsPlayer`, `IsSolid`, `IsSuperAdmin`, `SetAngles`, `SetColor`, `SetEyeAngles`, `SetLocalVelocity`, `SetMaterial`, `SetModel`, `SetNoDraw`, `SetPos`, `SetRenderMode`, `SetText`, `SetTitle`, `SetTranslation`, `SetWrap`, `Spawn`, `ToScreen`, `Translate`, `Unpack`.

Per file:

- `lua/autorun/cube_home_announce.lua`
  - globals: `CLIENT`, `CubeHome`, `ErrorNoHalt`, `FILL`, `InfMap`, `SERVER`, `game`, `hook`, `list`, `string`, `tostring`, `vgui`
  - assigns: `CubeHome`
  - direct calls: `ErrorNoHalt(`, `tostring(`, `CubeHome.IsHomeMap`, `game.GetMap`, `hook.Add`, `list.Set`, `string.lower`, `vgui.Create`
  - fields: `CubeHome.INFMAP_AUTHOR`, `CubeHome.INFMAP_REPO`, `CubeHome.INFMAP_WORKSHOP`, `CubeHome.INFMAP_WORKSHOP_URL`, `CubeHome.MAP`, `CubeHome.TITLE`, `CubeHome.VOID_KEY`, `CubeHome.VOID_KEY_N`, `game.GetMap`
  - instance methods: `Dock`, `DockMargin`, `SetText`, `SetTitle`, `SetWrap`
- `lua/infmap/xr_infmap_passthrough/cl_home_ui.lua`
  - globals: `CLIENT`, `Color`, `CubeHome`, `IsValid`, `LocalPlayer`, `TEXT_ALIGN_CENTER`, `Vector`, `chat`, `color_white`, `draw`, `hook`, `ipairs`, `istable`, `math`, `timer`
  - assigns: `CubeHome`
  - direct calls: `Color(`, `IsValid(`, `LocalPlayer(`, `Vector(`, `ipairs(`, `istable(`, `CubeHome.IsHomeMap`, `CubeHome.LoadLayoutFromDisk`, `CubeHome.Vec`, `chat.AddText`, `draw.SimpleTextOutlined`, `hook.Add`, `math.Clamp`, `timer.Simple`
  - fields: `CubeHome.IsHomeMap`, `CubeHome.Layout`, `CubeHome.LoadLayoutFromDisk`, `CubeHome.Vec`
  - instance methods: `Distance`, `EyePos`, `ToScreen`
- `lua/infmap/xr_infmap_passthrough/cl_passthrough.lua`
  - globals: `CLIENT`, `CreateClientConVar`, `CubeHome`, `FCVAR_ARCHIVE`, `InfMap`, `MATERIAL_FOG_NONE`, `RunConsoleCommand`, `VRMOD_GetBackend`, `VRMOD_SetEnvironmentBlendMode`, `VRMOD_SetPassthroughChroma`, `VRMOD_SetPassthroughChromaKey`, `VRMOD_SetPassthroughChromaMask`, `concommand`, `cvars`, `g_VR`, `hook`, `isfunction`, `isstring`, `istable`, `math`, `pcall`, `print`, `render`, `string`, `table`, `timer`, `tobool`, `tonumber`, `tostring`, `vrmod`
  - assigns: `CubeHome`
  - direct calls: `CreateClientConVar(`, `RunConsoleCommand(`, `VRMOD_GetBackend(`, `VRMOD_SetEnvironmentBlendMode(`, `VRMOD_SetPassthroughChroma(`, `VRMOD_SetPassthroughChromaKey(`, `VRMOD_SetPassthroughChromaMask(`, `isfunction(`, `isstring(`, `istable(`, `pcall(`, `print(`, `tobool(`, `tonumber(`, `tostring(`, `CubeHome.CanUsePassthrough`, `CubeHome.DisablePassthrough`, `CubeHome.EnablePassthrough`, `CubeHome.IsHomeMap`, `CubeHome.RefreshPassthroughMenu`, `CubeHome.RemovePassthroughMenu`, `CubeHome.SetPassthrough`, `CubeHome.TogglePassthrough`, `concommand.Add`, `cvars.AddChangeCallback`, `hook.Add`, `math.Clamp`, `math.max`, `render.Clear`, `render.FogMode`, `string.format`, `string.lower`, `table.remove`, `timer.Simple`, `vrmod.AddInGameMenuItem`, `vrmod.GetNativePolicy`, `vrmod.RemoveInGameMenuItem`
  - fields: `CubeHome.IsHomeMap`, `CubeHome.MAP`, `CubeHome.Passthrough`, `CubeHome.VOID_KEY`, `CubeHome.VOID_KEY_N`, `InfMap.planet_render_distance`, `InfMap.render_distance`, `InfMap.terrain_material`, `g_VR.active`, `g_VR.menuItems`, `vrmod.AddInGameMenuItem`, `vrmod.GetBackend`, `vrmod.GetNativePolicy`, `vrmod.RemoveInGameMenuItem`
  - instance methods: `GetBool`, `GetFloat`
- `lua/infmap/xr_infmap_passthrough/cl_terrain_visual.lua`
  - globals: `Angle`, `Color`, `EyePos`, `FrameNumber`, `InfMap`, `IsValid`, `LocalPlayer`, `MATERIAL_CULLMODE_CCW`, `MATERIAL_CULLMODE_NONE`, `Material`, `Matrix`, `Mesh`, `RENDERMODE_NORMAL`, `RunConsoleCommand`, `SafeRemoveEntity`, `Vector`, `cam`, `ents`, `hook`, `istable`, `math`, `pairs`, `render`, `table`
  - direct calls: `Angle(`, `Color(`, `EyePos(`, `FrameNumber(`, `IsValid(`, `LocalPlayer(`, `Material(`, `Matrix(`, `Mesh(`, `RunConsoleCommand(`, `SafeRemoveEntity(`, `Vector(`, `istable(`, `pairs(`, `InfMap.localize_vector`, `cam.IgnoreZ`, `cam.PopModelMatrix`, `cam.PushModelMatrix`, `ents.CreateClientside`, `hook.Add`, `math.abs`, `render.CullMode`, `render.OverrideDepthEnable`, `render.ResetModelLighting`, `render.SetColorModulation`, `render.SetLocalModelLights`, `render.SetMaterial`, `table.Copy`
  - fields: `InfMap.chunk_size`, `InfMap.client_chunks`, `InfMap.filter`, `InfMap.height_function`, `InfMap.megachunk_size`, `InfMap.render_distance`, `InfMap.render_max_height`, `InfMap.terrain_material`
  - instance methods: `BuildFromTriangles`, `Draw`, `GenerateMesh`, `IsError`, `SetAngles`, `SetColor`, `SetMaterial`, `SetNoDraw`, `SetRenderMode`, `SetTranslation`, `Spawn`, `Translate`
- `lua/infmap/xr_infmap_passthrough/sh_00_config.lua`
  - globals: `Angle`, `CubeHome`, `Vector`, `file`, `game`, `isangle`, `istable`, `isvector`, `pcall`, `string`, `tonumber`, `tostring`, `util`
  - assigns: `CubeHome`
  - direct calls: `Angle(`, `Vector(`, `isangle(`, `istable(`, `isvector(`, `pcall(`, `tonumber(`, `tostring(`, `CubeHome.Ang`, `CubeHome.DefaultLayout`, `CubeHome.IsHomeMap`, `CubeHome.LoadLayoutFromDisk`, `CubeHome.NormalizeLayout`, `CubeHome.SaveLayout`, `CubeHome.Vec`, `file.CreateDir`, `file.Exists`, `file.Read`, `file.Write`, `game.GetMap`, `string.lower`, `util.TableToJSON`
  - fields: `CubeHome.DATA_DIR`, `CubeHome.INFMAP_AUTHOR`, `CubeHome.INFMAP_REPO`, `CubeHome.INFMAP_WORKSHOP`, `CubeHome.INFMAP_WORKSHOP_URL`, `CubeHome.LAYOUT_DEFAULT`, `CubeHome.LAYOUT_FILE`, `CubeHome.Layout`, `CubeHome.MAP`, `CubeHome.TITLE`, `CubeHome.VOID_KEY`, `CubeHome.VOID_KEY_N`, `CubeHome.VOID_TOLERANCE`, `util.JSONToTable`
  - instance methods: `Unpack`
- `lua/infmap/xr_infmap_passthrough/sh_collider_functions.lua`
  - globals: `CLIENT`, `CubeHome`, `InfMap`, `RunConsoleCommand`, `Vector`, `hook`, `include`, `math`, `physenv`, `tonumber`, `tostring`
  - direct calls: `RunConsoleCommand(`, `Vector(`, `include(`, `tonumber(`, `tostring(`, `CubeHome.IsHomeMap`, `InfMap.height_function`, `InfMap.planet_height_function`, `InfMap.planet_info`, `hook.Add`, `math.max`, `physenv.SetPerformanceSettings`
  - fields: `CubeHome.Layout`, `InfMap.chunk_resolution`, `InfMap.disable_pickup`, `InfMap.filter`, `InfMap.planet_data`, `InfMap.planet_render_distance`, `InfMap.planet_resolution`, `InfMap.planet_spacing`, `InfMap.planet_tree_resolution`, `InfMap.planet_uv_scale`, `InfMap.simplex`
  - literal includes: `include` `simplex.lua`
- `lua/infmap/xr_infmap_passthrough/sv_home_layout.lua`
  - globals: `Angle`, `Color`, `CubeHome`, `InfMap`, `IsValid`, `RENDERMODE_TRANSCOLOR`, `SafeRemoveEntity`, `Vector`, `concommand`, `ents`, `file`, `game`, `hook`, `ipairs`, `isstring`, `istable`, `math`, `os`, `print`, `string`, `table`, `timer`, `tonumber`, `tostring`
  - assigns: `CubeHome`
  - direct calls: `Angle(`, `Color(`, `IsValid(`, `SafeRemoveEntity(`, `Vector(`, `ipairs(`, `isstring(`, `istable(`, `print(`, `tonumber(`, `tostring(`, `CubeHome.AddPropAtPlayer`, `CubeHome.Ang`, `CubeHome.DefaultLayout`, `CubeHome.IsHomeMap`, `CubeHome.LoadLayoutFromDisk`, `CubeHome.RebuildLayout`, `CubeHome.SaveLayout`, `CubeHome.TeleportPlayerHome`, `CubeHome.Vec`, `InfMap.prop_update_chunk`, `concommand.Add`, `ents.Create`, `file.Exists`, `game.SinglePlayer`, `hook.Add`, `hook.Run`, `math.random`, `os.time`, `string.format`, `table.insert`, `timer.Simple`
  - fields: `CubeHome.LAYOUT_FILE`, `CubeHome.Layout`, `CubeHome._spawned`, `InfMap.prop_update_chunk`
  - instance methods: `Activate`, `ChatPrint`, `EnableMotion`, `EyeAngles`, `GetPhysicsObject`, `GetPos`, `InfMap_SetPos`, `IsPlayer`, `IsSuperAdmin`, `SetAngles`, `SetColor`, `SetEyeAngles`, `SetLocalVelocity`, `SetMaterial`, `SetModel`, `SetPos`, `SetRenderMode`, `Spawn`
- `lua/infmap/xr_infmap_passthrough/sv_terrain_collision.lua`
  - globals: `Color`, `InfMap`, `IsValid`, `RENDERMODE_NONE`, `SafeRemoveEntity`, `Vector`, `constraint`, `ents`, `game`, `hook`, `ipairs`
  - direct calls: `Color(`, `IsValid(`, `SafeRemoveEntity(`, `Vector(`, `ipairs(`, `InfMap.ezcoord`, `InfMap.filter_entities`, `InfMap.prop_update_chunk`, `constraint.Weld`, `ents.Create`, `ents.GetAll`, `game.GetWorld`, `hook.Add`
  - fields: `InfMap.chunk_table`
  - instance methods: `DrawShadow`, `EnableMotion`, `GetPhysicsObject`, `InfMap_SetPos`, `IsSolid`, `SetColor`, `SetModel`, `SetNoDraw`, `SetRenderMode`, `Spawn`

### `cube_ws_2049617805`

Path: `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/addons/cube_ws_2049617805`. Title: Cube extracted WS 2049617805. Lua files: **0**.

No `.lua` files. Nothing to boot. Contents are maps and/or materials only.

### `cube_ws_2905327911`

Path: `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/addons/cube_ws_2905327911`. Title: Cube extracted WS 2905327911. Lua files: **24**.

Boot subset (87 external globals):

Libraries / tables:

- `E2Helper` — fields `Descriptions`
- `E2Lib` — methods `RegisterExtension`
- `ENT` — methods `BuildCollision`, `Draw`, `GenerateMesh`, `GenerateTrees`, `GetRenderMesh`, `Initialize`, `InitializeClient`, `InitializePhysics`, `OnRemove`, `SetLocalRenderBounds`, `SetReferenceData`, `SetupDataTables`, `Think`, `TryOptimizeCollision`, `UpdateCollision`; fields `Author`, `Category`, `Instructions`, `PrintName`, `Purpose`, `Spawnable`, `Type`
- `SF` — methods `clampPos`
- `WireLib` — methods `clampPos`
- `bit` — methods `band`
- `cam` — methods `End2D`, `End3D`, `PopModelMatrix`, `PushModelMatrix`, `Start2D`, `Start3D`; also referenced without calling `End3D`, `PopModelMatrix`, `PushModelMatrix`, `Start3D`
- `constraint` — methods `GetTable`, `Weld`
- `coroutine` — methods `create`, `resume`, `status`, `wait`, `yield`
- `ents` — methods `Create`, `CreateClientProp`, `CreateClientside`, `FindByClass`, `FindInBox`, `FindInCone`, `FindInSphere`, `GetAll`; also referenced without calling `FindInBox`, `FindInCone`, `FindInSphere`
- `file` — methods `Find`, `Read`
- `game` — methods `GetMap`, `GetWorld`, `SinglePlayer`
- `gmsave` — methods `ShouldSaveEntity`; also referenced without calling `ShouldSaveEntity`
- `hook` — methods `Add`, `Remove`, `Run`
- `math` — methods `Clamp`, `Round`, `abs`, `floor`, `max`, `min`, `pow`, `sqrt`; fields `huge`; also referenced without calling `Clamp`, `floor`
- `net` — methods `ReadAngle`, `ReadEntity`, `ReadFloat`, `ReadString`, `ReadUInt`, `Receive`, `Send`, `Start`, `WriteAngle`, `WriteEntity`, `WriteFloat`, `WriteString`, `WriteUInt`
- `physenv` — methods `SetPerformanceSettings`
- `player` — methods `GetAll`
- `render` — methods `DrawWireframeBox`, `DrawWireframeSphere`, `GetLightColor`, `PopFlashlightMode`, `PushFlashlightMode`, `ResetModelLighting`, `SetLocalModelLights`, `SetMaterial`; fields `DrawBox`, `SetLightmapTexture`, `SetModelLighting`; also referenced without calling `SetLocalModelLights`, `SetMaterial`
- `resource` — methods `AddWorkshop`
- `sound` — methods `GetProperties`
- `string` — methods `Explode`, `Split`, `Trim`, `find`, `gsub`, `lower`, `sub`
- `surface` — methods `DrawRect`, `SetDrawColor`
- `table` — methods `Add`, `Copy`, `Count`, `Empty`, `RemoveByValue`, `insert`, `remove`; also referenced without calling `insert`
- `timer` — methods `Create`, `Simple`
- `util` — methods `AddNetworkString`, `BlastDamage`, `GetModelMeshes`, `GetSunInfo`, `GetSurfaceData`, `GetSurfaceIndex`, `IsInWorld`, `SharedRandom`, `TraceEntity`, `TraceHull`, `TraceLine`; fields `IntersectRayWithPlane`; also referenced without calling `BlastDamage`, `TraceEntity`, `TraceHull`, `TraceLine`

Functions and constructors: `AddCSLuaFile`, `Angle`, `Color`, `CreateClientConVar`, `CreateMaterial`, `CurTime`, `DrawMaterialOverlay`, `ErrorNoHalt`, `EyePos`, `FindMetaTable`, `FrameTime`, `GetConVar`, `GetRenderTarget`, `IsValid`, `LocalPlayer`, `Material`, `Matrix`, `Mesh`, `RunConsoleCommand`, `SafeRemoveEntity`, `Vector`, `__e2setcost`, `error`, `include`, `ipairs`, `isentity`, `pairs`, `pcall`, `print`, `tonumber`, `tostring`, `unpack`.

Constants and uppercase names: `ACT_MP_SWIM`, `CHAN_BODY`, `CLIENT`, `DMG_BURN`, `EFL_SERVER_ONLY`, `EF_DIMLIGHT`, `FL_STATICPROP`, `FSOLID_FORCE_WORLD_ALIGNED`, `FVPHYSICS_CONSTRAINT_STATIC`, `FVPHYSICS_NO_SELF_COLLISIONS`, `IN_JUMP`, `MATERIAL_FOG_LINEAR`, `MATERIAL_LIGHT_DIRECTIONAL`, `MATERIAL_TRIANGLES`, `MOVETYPE_NOCLIP`, `MOVETYPE_NONE`, `MOVETYPE_VPHYSICS`, `RENDERMODE_NONE`, `RENDERMODE_NORMAL`, `SERVER`, `SOLID_NONE`, `SOLID_VPHYSICS`.

Other reads: `IsEntity`, `istable`, `mesh`, `pos`, `vector_origin`.

Engine globals this addon reads and then replaces: `CreateSound`, `ParticleEffect`. The stock function has to exist before the wrapper runs.

Defined in this addon: `InfMap`, `SoundObject`.

`pos` is an unbound global in `lua/infmap/cl_inf_detours.lua` inside `EntityMT:GetPos`, which has no `pos` parameter. It is not an engine API. At runtime that read is nil unless some other file set a global `pos`.

`mesh`, `IsEntity` are copied with `local alias = alias` before use. Calls on the local show up as instance methods (`Begin`, `Position`, `TexCoord`, `Normal`, `UserData`, `AdvanceVertex`, `End` for `mesh`), not as `mesh.Begin`.

Wire / Starfall names, not stock GMod: `E2Lib`, `E2Helper`, `WireLib`, `SF`, `__e2setcost`. `WireLib` and `SF` are only touched inside `if WireLib` / `if SF`. The Expression 2 file is not loaded by the stock Lua VM.

Instance methods: `AddAngleVelocity`, `AddFlags`, `AddGameFlag`, `AddSolidFlags`, `AdvanceVertex`, `Alive`, `ApplyForceCenter`, `ApplyForceOffset`, `Begin`, `BoundingRadius`, `BuildCollision`, `BuildFromTriangles`, `CallOnRemove`, `ChangeVolume`, `ClearRenderTarget`, `ConCommand`, `Cross`, `Destroy`, `DistToSqr`, `Dot`, `Draw`, `DrawModel`, `DrawQuadEasy`, `DrawShadow`, `DrawSphere`, `EmitSound`, `EnableCustomCollisions`, `EnableMotion`, `End`, `EntIndex`, `Extinguish`, `FlashlightIsOn`, `FogColor`, `FogEnd`, `FogMaxDensity`, `FogMode`, `FogStart`, `ForcePlayerDrop`, `Forward`, `GenerateMesh`, `GenerateTrees`, `GetAmbientLightColor`, `GetAngleVelocity`, `GetAngles`, `GetBoneSurfaceProp`, `GetBool`, `GetButtons`, `GetChildren`, `GetClass`, `GetCollisionGroup`, `GetDriver`, `GetEntity`, `GetFloat`, `GetForwardSpeed`, `GetGravity`, `GetInflictor`, `GetKeyValues`, `GetMass`, `GetMaterial`, `GetMaterials`, `GetMaxSpeed`, `GetMesh`, `GetModel`, `GetModelRenderBounds`, `GetMoveAngles`, `GetMoveType`, `GetNW2Vector`, `GetName`, `GetNextBot`, `GetNoDraw`, `GetNormalized`, `GetOwner`, `GetParent`, `GetPhysicsObject`, `GetPhysicsObjectCount`, `GetPhysicsObjectNum`, `GetPlanetRadius`, `GetPos`, `GetRangeSquaredTo`, `GetReferenceParent`, `GetRenderBounds`, `GetRotatedAABB`, `GetSideSpeed`, `GetTexture`, `GetTranslation`, `GetUpSpeed`, `GetVelocity`, `GetWeaponColor`, `GetWeapons`, `InVehicle`, `InfMap_ApplyForceOffset`, `InfMap_Approach`, `InfMap_CalculateForceOffset`, `InfMap_CalculateVelocityOffset`, `InfMap_DoShootEffect`, `InfMap_EyePos`, `InfMap_FaceTowards`, `InfMap_GetAttachment`, `InfMap_GetBonePosition`, `InfMap_GetDamagePosition`, `InfMap_GetPos`, `InfMap_GetShootPos`, `InfMap_GetVelocityAtPoint`, `InfMap_LocalToWorld`, `InfMap_NearestPoint`, `InfMap_SetEntity`, `InfMap_SetMaterial`, `InfMap_SetPos`, `InfMap_SetRenderBounds`, `InfMap_Spawn`, `InfMap_Stop`, `InfMap_WorldSpaceAABB`, `InfMap_WorldSpaceCenter`, `InfMap_WorldToLocal`, `Initialize`, `InitializeClient`, `InitializePhysics`, `IsAsleep`, `IsConstraint`, `IsDamageType`, `IsEFlagSet`, `IsExplosionDamage`, `IsMoveable`, `IsNPC`, `IsOnFire`, `IsOnGround`, `IsPlayer`, `IsPlayerHolding`, `IsRagdoll`, `IsSolid`, `IsValid`, `IsVehicle`, `IsWeapon`, `IsWorld`, `Length`, `LengthSqr`, `LocalToWorld`, `NetworkVar`, `Noise2D`, `Noise3D`, `Normal`, `OBBMaxs`, `OBBMins`, `OldRenderOverride`, `OverrideDepthEnable`, `PhysicsDestroy`, `PhysicsFromMesh`, `PhysicsInit`, `PopRenderTarget`, `Position`, `PushRenderTarget`, `Remove`, `RemoveEffects`, `ResetModelLighting`, `Right`, `Rotate`, `SetAngleVelocity`, `SetAngles`, `SetCollisionGroup`, `SetCustomCollisionCheck`, `SetDSP`, `SetFloat`, `SetKeyValue`, `SetLocalModelLights`, `SetLocalRenderBounds`, `SetMass`, `SetMaterial`, `SetModel`, `SetModelLighting`, `SetMoveType`, `SetNW2Vector`, `SetNextClientThink`, `SetNoDraw`, `SetNotSolid`, `SetPlanetRadius`, `SetPos`, `SetReferenceData`, `SetReferenceParent`, `SetRenderBounds`, `SetRenderBoundsWS`, `SetRenderMode`, `SetScale`, `SetSolid`, `SetTexture`, `SetTranslation`, `SetVector`, `SetVelocity`, `SetWeaponColor`, `Sleep`, `Spawn`, `Stop`, `StopSound`, `TexCoord`, `TryOptimizeCollision`, `Up`, `UpdateCollision`, `UserData`, `WithinAABox`, `WorldSpaceCenter`, `abs`, `floor`, `insert`, `sin`, `sqrt`.

Per file:

- `lua/autorun/!!inf_init.lua`
  - globals: `AddCSLuaFile`, `CLIENT`, `InfMap`, `SERVER`, `Vector`, `file`, `game`, `include`, `ipairs`, `math`, `resource`, `string`
  - assigns: `InfMap`
  - direct calls: `AddCSLuaFile(`, `Vector(`, `include(`, `ipairs(`, `file.Find`, `game.GetMap`, `math.pow`, `resource.AddWorkshop`, `string.Explode`, `string.lower`, `string.sub`
  - fields: `InfMap.chunk_size`, `InfMap.source_bounds`
- `lua/entities/gmod_wire_expression2/core/custom/cl_infmap.lua`
  - globals: `E2Helper`
  - fields: `E2Helper.Descriptions`
- `lua/entities/gmod_wire_expression2/core/custom/infmap.lua` — **not GLua** (Wire `e2function` syntax; names below are the real dependencies, not the broken parse)
  - globals: `E2Lib`, `InfMap`, `__e2setcost`
  - direct calls: `__e2setcost(`, `E2Lib.RegisterExtension`, `InfMap.height_function`
- `lua/entities/infmap_clone.lua`
  - globals: `AddCSLuaFile`, `CLIENT`, `ENT`, `InfMap`, `IsValid`, `MOVETYPE_VPHYSICS`, `SOLID_VPHYSICS`, `SafeRemoveEntity`, `print`
  - direct calls: `AddCSLuaFile(`, `IsValid(`, `SafeRemoveEntity(`, `print(`, `ENT.Initialize`, `ENT.InitializeClient`, `ENT.InitializePhysics`, `ENT.SetReferenceData`, `ENT.SetupDataTables`, `ENT.Think`, `InfMap.prop_update_chunk`
  - fields: `ENT.Author`, `ENT.Category`, `ENT.Instructions`, `ENT.PrintName`, `ENT.Purpose`, `ENT.Spawnable`, `ENT.Type`, `InfMap.chunk_size`
  - instance methods: `EnableCustomCollisions`, `EnableMotion`, `GetAngles`, `GetCollisionGroup`, `GetMesh`, `GetModel`, `GetPhysicsObject`, `GetPos`, `GetReferenceParent`, `InfMap_GetPos`, `InfMap_SetPos`, `Initialize`, `InitializeClient`, `InitializePhysics`, `IsValid`, `NetworkVar`, `PhysicsFromMesh`, `PhysicsInit`, `SetAngles`, `SetCollisionGroup`, `SetModel`, `SetMoveType`, `SetNoDraw`, `SetPos`, `SetReferenceParent`, `SetSolid`
- `lua/entities/infmap_obj_collider.lua`
  - globals: `AddCSLuaFile`, `CLIENT`, `CurTime`, `ENT`, `FL_STATICPROP`, `FSOLID_FORCE_WORLD_ALIGNED`, `FVPHYSICS_CONSTRAINT_STATIC`, `FVPHYSICS_NO_SELF_COLLISIONS`, `InfMap`, `LocalPlayer`, `MOVETYPE_NONE`, `SERVER`, `SOLID_VPHYSICS`, `ents`, `hook`, `ipairs`, `table`
  - direct calls: `AddCSLuaFile(`, `CurTime(`, `LocalPlayer(`, `ipairs(`, `ENT.Initialize`, `ENT.Think`, `ENT.TryOptimizeCollision`, `ENT.UpdateCollision`, `InfMap.ezcoord`, `InfMap.filter_entities`, `ents.FindByClass`, `hook.Add`, `table.Count`
  - fields: `ENT.Author`, `ENT.Category`, `ENT.Instructions`, `ENT.PrintName`, `ENT.Purpose`, `ENT.Spawnable`, `ENT.Type`, `InfMap.all_ents`
  - instance methods: `AddFlags`, `AddGameFlag`, `AddSolidFlags`, `DrawShadow`, `EnableCustomCollisions`, `EnableMotion`, `GetPhysicsObject`, `PhysicsDestroy`, `PhysicsFromMesh`, `SetMass`, `SetMoveType`, `SetNextClientThink`, `SetNoDraw`, `SetNotSolid`, `SetSolid`, `TryOptimizeCollision`, `UpdateCollision`
- `lua/entities/infmap_planet.lua`
  - globals: `AddCSLuaFile`, `Angle`, `CLIENT`, `ENT`, `EyePos`, `FL_STATICPROP`, `FSOLID_FORCE_WORLD_ALIGNED`, `FVPHYSICS_CONSTRAINT_STATIC`, `FVPHYSICS_NO_SELF_COLLISIONS`, `InfMap`, `IsValid`, `LocalPlayer`, `MATERIAL_TRIANGLES`, `MOVETYPE_NONE`, `Material`, `Matrix`, `Mesh`, `SERVER`, `SOLID_VPHYSICS`, `Vector`, `cam`, `ipairs`, `math`, `mesh`, `print`, `render`, `table`, `tostring`, `util`
  - direct calls: `AddCSLuaFile(`, `Angle(`, `EyePos(`, `IsValid(`, `LocalPlayer(`, `Material(`, `Matrix(`, `Mesh(`, `Vector(`, `ipairs(`, `print(`, `tostring(`, `ENT.BuildCollision`, `ENT.Draw`, `ENT.GenerateMesh`, `ENT.GenerateTrees`, `ENT.GetRenderMesh`, `ENT.Initialize`, `ENT.SetupDataTables`, `ENT.Think`, `math.min`, `render.PopFlashlightMode`, `render.PushFlashlightMode`, `table.Add`, `table.insert`, `util.GetModelMeshes`, `util.SharedRandom`
  - fields: `ENT.Author`, `ENT.Category`, `ENT.Instructions`, `ENT.PrintName`, `ENT.Purpose`, `ENT.Spawnable`, `ENT.Type`, `InfMap.chunk_size`, `InfMap.planet_height_function`, `InfMap.planet_resolution`, `InfMap.planet_tree_resolution`, `InfMap.planet_uv_scale`, `cam.PopModelMatrix`, `cam.PushModelMatrix`, `render.SetLightmapTexture`, `render.SetLocalModelLights`, `render.SetMaterial`, `render.SetModelLighting`
  - instance methods: `AddFlags`, `AddGameFlag`, `AddSolidFlags`, `AdvanceVertex`, `Begin`, `BuildCollision`, `BuildFromTriangles`, `Cross`, `DistToSqr`, `Draw`, `DrawModel`, `DrawShadow`, `EnableCustomCollisions`, `EnableMotion`, `End`, `FlashlightIsOn`, `GenerateMesh`, `GenerateTrees`, `GetMaterial`, `GetNormalized`, `GetPhysicsObject`, `GetPlanetRadius`, `InfMap_GetPos`, `Initialize`, `LengthSqr`, `NetworkVar`, `Normal`, `PhysicsDestroy`, `PhysicsFromMesh`, `Position`, `SetAngles`, `SetMass`, `SetMoveType`, `SetRenderBounds`, `SetScale`, `SetSolid`, `SetTranslation`, `TexCoord`, `UserData`
- `lua/entities/infmap_terrain_collider.lua`
  - globals: `AddCSLuaFile`, `CLIENT`, `ENT`, `FL_STATICPROP`, `FSOLID_FORCE_WORLD_ALIGNED`, `FVPHYSICS_CONSTRAINT_STATIC`, `FVPHYSICS_NO_SELF_COLLISIONS`, `InfMap`, `IsValid`, `MOVETYPE_NONE`, `RENDERMODE_NONE`, `SERVER`, `SOLID_VPHYSICS`, `SafeRemoveEntity`, `Vector`, `print`, `table`
  - direct calls: `AddCSLuaFile(`, `IsValid(`, `SafeRemoveEntity(`, `Vector(`, `print(`, `ENT.BuildCollision`, `ENT.Initialize`, `ENT.Think`, `InfMap.split_convex`, `table.Add`
  - fields: `ENT.Author`, `ENT.Category`, `ENT.Instructions`, `ENT.PrintName`, `ENT.Purpose`, `ENT.Spawnable`, `ENT.Type`, `InfMap.chunk_resolution`, `InfMap.chunk_size`, `InfMap.height_function`, `InfMap.source_bounds`
  - instance methods: `AddFlags`, `AddGameFlag`, `AddSolidFlags`, `BuildCollision`, `DrawShadow`, `EnableCustomCollisions`, `EnableMotion`, `GetPhysicsObject`, `Initialize`, `PhysicsDestroy`, `PhysicsFromMesh`, `SetMass`, `SetMoveType`, `SetRenderMode`, `SetSolid`
- `lua/entities/infmap_terrain_render.lua`
  - globals: `AddCSLuaFile`, `ENT`, `InfMap`, `IsValid`, `LocalPlayer`, `MATERIAL_TRIANGLES`, `MOVETYPE_NONE`, `Material`, `Matrix`, `Mesh`, `RENDERMODE_NORMAL`, `SERVER`, `SOLID_NONE`, `Vector`, `coroutine`, `ipairs`, `math`, `mesh`, `table`, `util`
  - direct calls: `AddCSLuaFile(`, `IsValid(`, `LocalPlayer(`, `Material(`, `Matrix(`, `Mesh(`, `Vector(`, `ipairs(`, `ENT.GenerateMesh`, `ENT.GetRenderMesh`, `ENT.Initialize`, `ENT.OnRemove`, `ENT.SetLocalRenderBounds`, `ENT.Think`, `InfMap.unlocalize_vector`, `coroutine.create`, `coroutine.resume`, `coroutine.status`, `coroutine.wait`, `coroutine.yield`, `math.max`, `math.min`, `table.Empty`, `util.GetModelMeshes`
  - fields: `ENT.Author`, `ENT.Category`, `ENT.Instructions`, `ENT.PrintName`, `ENT.Purpose`, `ENT.Spawnable`, `ENT.Type`, `InfMap.chunk_resolution`, `InfMap.chunk_size`, `InfMap.megachunk_size`, `InfMap.source_bounds`, `InfMap.uv_scale`
  - instance methods: `AdvanceVertex`, `Begin`, `Cross`, `Destroy`, `DrawShadow`, `End`, `GetAngles`, `GetModel`, `GetTranslation`, `Length`, `Normal`, `Position`, `SetAngles`, `SetModel`, `SetMoveType`, `SetRenderBounds`, `SetRenderBoundsWS`, `SetRenderMode`, `SetSolid`, `SetTranslation`, `TexCoord`, `UserData`
- `lua/infmap/cl_inf_chunks.lua`
  - globals: `Angle`, `Color`, `CreateClientConVar`, `EF_DIMLIGHT`, `ErrorNoHalt`, `EyePos`, `InfMap`, `IsValid`, `LocalPlayer`, `Material`, `Vector`, `cam`, `ents`, `hook`, `ipairs`, `math`, `pcall`, `player`, `render`, `table`, `timer`, `vector_origin`
  - direct calls: `Angle(`, `Color(`, `CreateClientConVar(`, `ErrorNoHalt(`, `IsValid(`, `LocalPlayer(`, `Material(`, `Vector(`, `ipairs(`, `pcall(`, `InfMap.filter_entities`, `InfMap.prop_update_chunk`, `InfMap.unlocalize_vector`, `ents.GetAll`, `hook.Add`, `hook.Run`, `player.GetAll`, `render.DrawWireframeBox`, `render.DrawWireframeSphere`, `table.insert`, `table.remove`, `timer.Create`
  - fields: `InfMap.all_ents`, `InfMap.chunk_size`, `InfMap.source_bounds`, `cam.End3D`, `cam.Start3D`, `math.huge`, `render.DrawBox`, `render.SetMaterial`
  - instance methods: `Alive`, `BoundingRadius`, `ConCommand`, `DrawModel`, `EntIndex`, `GetAngles`, `GetBool`, `GetChildren`, `GetMaterial`, `GetMaterials`, `GetModelRenderBounds`, `GetNoDraw`, `GetRenderBounds`, `GetRotatedAABB`, `GetWeaponColor`, `InfMap_GetPos`, `IsPlayer`, `IsWeapon`, `Length`, `LengthSqr`, `OBBMaxs`, `OBBMins`, `OldRenderOverride`, `RemoveEffects`, `SetRenderBounds`, `SetRenderBoundsWS`, `SetWeaponColor`
- `lua/infmap/cl_inf_detours.lua`
  - globals: `FindMetaTable`, `InfMap`, `IsEntity`, `IsValid`, `LocalPlayer`, `ParticleEffect`, `Vector`, `ents`, `game`, `hook`, `ipairs`, `istable`, `math`, `net`, `pairs`, `pos`, `table`, `util`
  - direct calls: `FindMetaTable(`, `IsValid(`, `LocalPlayer(`, `ParticleEffect(`, `Vector(`, `ipairs(`, `pairs(`, `InfMap.unlocalize_vector`, `ents.FindInBox`, `ents.FindInCone`, `ents.FindInSphere`, `ents.GetAll`, `game.GetWorld`, `hook.Add`, `net.ReadAngle`, `net.ReadEntity`, `net.ReadFloat`, `net.ReadString`, `net.Receive`, `table.insert`, `util.TraceEntity`, `util.TraceHull`, `util.TraceLine`
  - fields: `InfMap.FindInBox`, `InfMap.FindInCone`, `InfMap.FindInSphere`, `InfMap.TraceEntity`, `InfMap.TraceHull`, `InfMap.TraceLine`, `InfMap.disable_pickup`, `InfMap.source_bounds`, `ents.FindInBox`, `ents.FindInCone`, `ents.FindInSphere`, `math.Clamp`, `util.TraceEntity`, `util.TraceHull`, `util.TraceLine`
  - instance methods: `DistToSqr`, `Dot`, `GetClass`, `GetNormalized`, `GetPos`, `InfMap_GetBonePosition`, `InfMap_GetPos`, `InfMap_LocalToWorld`, `InfMap_SetPos`, `InfMap_SetRenderBounds`, `InfMap_WorldSpaceAABB`, `InfMap_WorldSpaceCenter`, `WithinAABox`, `WorldSpaceCenter`
- `lua/infmap/gm_infmap/cl_planets_visual.lua`
  - globals: `EyePos`, `InfMap`, `LocalPlayer`, `Material`, `Vector`, `hook`, `render`
  - direct calls: `EyePos(`, `LocalPlayer(`, `Material(`, `Vector(`, `InfMap.localize_vector`, `InfMap.planet_info`, `InfMap.unlocalize_vector`, `hook.Add`
  - fields: `InfMap.planet_data`, `InfMap.planet_render_distance`, `InfMap.planet_spacing`
  - instance methods: `DrawSphere`, `GetAmbientLightColor`, `LengthSqr`, `OverrideDepthEnable`, `SetFloat`, `SetLocalModelLights`, `SetMaterial`, `SetModelLighting`, `SetVector`
- `lua/infmap/gm_infmap/cl_terrain_visual.lua`
  - globals: `Angle`, `Color`, `CreateMaterial`, `CurTime`, `GetRenderTarget`, `InfMap`, `IsValid`, `LocalPlayer`, `MATERIAL_FOG_LINEAR`, `Material`, `Matrix`, `Mesh`, `SafeRemoveEntity`, `Vector`, `cam`, `coroutine`, `ents`, `hook`, `math`, `render`, `surface`, `table`
  - direct calls: `Angle(`, `Color(`, `CreateMaterial(`, `CurTime(`, `GetRenderTarget(`, `IsValid(`, `LocalPlayer(`, `Material(`, `Matrix(`, `Mesh(`, `SafeRemoveEntity(`, `Vector(`, `InfMap.localize_vector`, `InfMap.unlocalize_vector`, `cam.End2D`, `cam.PopModelMatrix`, `cam.PushModelMatrix`, `cam.Start2D`, `coroutine.create`, `coroutine.resume`, `coroutine.status`, `coroutine.yield`, `ents.CreateClientside`, `hook.Add`, `math.abs`, `math.max`, `surface.DrawRect`, `surface.SetDrawColor`, `table.Copy`
  - fields: `InfMap.chunk_size`, `InfMap.client_chunks`, `InfMap.cloud_mats`, `InfMap.cloud_rts`, `InfMap.filter`, `InfMap.height_function`, `InfMap.megachunk_size`, `InfMap.render_distance`, `InfMap.render_max_height`, `InfMap.simplex`, `InfMap.terrain_material`
  - instance methods: `BuildFromTriangles`, `ClearRenderTarget`, `Draw`, `DrawQuadEasy`, `FogColor`, `FogEnd`, `FogMaxDensity`, `FogMode`, `FogStart`, `GenerateMesh`, `GetName`, `Noise2D`, `Noise3D`, `OverrideDepthEnable`, `PopRenderTarget`, `PushRenderTarget`, `ResetModelLighting`, `SetAngles`, `SetFloat`, `SetLocalModelLights`, `SetLocalRenderBounds`, `SetMaterial`, `SetNoDraw`, `SetTranslation`, `Spawn`
- `lua/infmap/gm_infmap/sh_collider_functions.lua`
  - globals: `CLIENT`, `InfMap`, `Material`, `RunConsoleCommand`, `Vector`, `hook`, `include`, `math`, `physenv`, `tostring`, `util`
  - direct calls: `Material(`, `RunConsoleCommand(`, `Vector(`, `include(`, `tostring(`, `InfMap.height_function`, `InfMap.planet_height_function`, `InfMap.planet_info`, `hook.Add`, `math.Round`, `math.floor`, `math.max`, `physenv.SetPerformanceSettings`, `util.SharedRandom`
  - fields: `InfMap.chunk_resolution`, `InfMap.chunk_size`, `InfMap.disable_pickup`, `InfMap.filter`, `InfMap.planet_data`, `InfMap.planet_render_distance`, `InfMap.planet_resolution`, `InfMap.planet_spacing`, `InfMap.planet_tree_resolution`, `InfMap.planet_uv_scale`, `InfMap.simplex`
  - literal includes: `include` `simplex.lua`
- `lua/infmap/gm_infmap/sh_terrain_water.lua`
  - globals: `ACT_MP_SWIM`, `CHAN_BODY`, `CLIENT`, `DrawMaterialOverlay`, `EyePos`, `FrameTime`, `GetConVar`, `IN_JUMP`, `InfMap`, `IsValid`, `LocalPlayer`, `Material`, `Matrix`, `Mesh`, `SERVER`, `Vector`, `bit`, `cam`, `ents`, `hook`, `ipairs`, `math`, `render`, `util`
  - direct calls: `DrawMaterialOverlay(`, `EyePos(`, `FrameTime(`, `GetConVar(`, `LocalPlayer(`, `Material(`, `Matrix(`, `Mesh(`, `Vector(`, `ipairs(`, `InfMap.unlocalize_vector`, `bit.band`, `cam.PopModelMatrix`, `cam.PushModelMatrix`, `ents.FindByClass`, `hook.Add`, `math.abs`, `math.max`, `math.min`, `render.SetMaterial`, `util.TraceHull`
  - fields: `InfMap.render_max_height`, `InfMap.water_height`, `InfMap.water_material`
  - instance methods: `AddAngleVelocity`, `ApplyForceCenter`, `ApplyForceOffset`, `BuildFromTriangles`, `Draw`, `EmitSound`, `Extinguish`, `Forward`, `GetAngleVelocity`, `GetButtons`, `GetClass`, `GetFloat`, `GetForwardSpeed`, `GetGravity`, `GetMass`, `GetMaterial`, `GetMaxSpeed`, `GetMoveAngles`, `GetNormalized`, `GetPhysicsObject`, `GetPos`, `GetSideSpeed`, `GetUpSpeed`, `GetVelocity`, `InVehicle`, `IsAsleep`, `IsOnFire`, `IsOnGround`, `IsPlayerHolding`, `IsValid`, `Length`, `LocalToWorld`, `OBBMaxs`, `OBBMins`, `Right`, `SetDSP`, `SetScale`, `SetTranslation`, `SetVelocity`, `Sleep`, `Up`
- `lua/infmap/gm_infmap/sv_planets_collision.lua`
  - globals: `InfMap`, `IsValid`, `SafeRemoveEntity`, `ents`, `hook`, `ipairs`
  - direct calls: `IsValid(`, `SafeRemoveEntity(`, `ipairs(`, `InfMap.ezcoord`, `InfMap.filter_entities`, `InfMap.localize_vector`, `InfMap.planet_info`, `InfMap.prop_update_chunk`, `ents.Create`, `ents.GetAll`, `hook.Add`
  - fields: `InfMap.planet_chunk_table`, `InfMap.planet_data`, `InfMap.planet_spacing`
  - instance methods: `GetName`, `IsSolid`, `SetMaterial`, `SetModel`, `SetPlanetRadius`, `Spawn`
- `lua/infmap/gm_infmap/sv_terrain_collision.lua`
  - globals: `InfMap`, `IsValid`, `SafeRemoveEntity`, `Vector`, `constraint`, `ents`, `game`, `hook`, `ipairs`, `print`
  - direct calls: `IsValid(`, `SafeRemoveEntity(`, `Vector(`, `ipairs(`, `print(`, `InfMap.ezcoord`, `InfMap.filter_entities`, `InfMap.prop_update_chunk`, `constraint.Weld`, `ents.Create`, `ents.GetAll`, `game.GetWorld`, `hook.Add`
  - fields: `InfMap.chunk_table`
  - instance methods: `EnableMotion`, `GetPhysicsObject`, `InfMap_SetPos`, `IsSolid`, `SetMaterial`, `SetModel`, `Spawn`
- `lua/infmap/sh_inf_chunks.lua`
  - globals: `InfMap`, `game`, `hook`
  - direct calls: `game.GetWorld`, `hook.Add`
  - fields: `InfMap.disable_pickup`
  - instance methods: `GetClass`
- `lua/infmap/sh_inf_functions.lua`
  - globals: `InfMap`, `SERVER`, `Vector`, `constraint`, `ipairs`, `math`, `pairs`, `table`, `util`
  - direct calls: `Vector(`, `ipairs(`, `pairs(`, `InfMap.constrained_status`, `InfMap.ezcoord`, `InfMap.filter_entities`, `InfMap.get_all_constrained`, `InfMap.get_all_parents`, `InfMap.in_chunk`, `InfMap.intersect_box`, `InfMap.localize_vector`, `InfMap.reset_constrained_data`, `InfMap.split_convex`, `InfMap.unlocalize_vector`, `constraint.GetTable`, `table.Add`
  - fields: `InfMap.chunk_size`, `InfMap.disable_pickup`, `InfMap.filter`, `math.floor`, `table.insert`, `util.IntersectRayWithPlane`
  - instance methods: `Dot`, `EntIndex`, `GetChildren`, `GetClass`, `GetNoDraw`, `GetNormalized`, `GetOwner`, `GetParent`, `GetPhysicsObject`, `IsConstraint`, `IsMoveable`, `IsPlayer`, `IsPlayerHolding`, `IsSolid`, `IsValid`, `IsWeapon`
- `lua/infmap/sh_inf_networking.lua`
  - globals: `EFL_SERVER_ONLY`, `ErrorNoHalt`, `InfMap`, `IsValid`, `LocalPlayer`, `SERVER`, `SafeRemoveEntity`, `hook`, `ipairs`, `pairs`, `pcall`, `util`
  - direct calls: `ErrorNoHalt(`, `IsValid(`, `LocalPlayer(`, `SafeRemoveEntity(`, `ipairs(`, `pairs(`, `pcall(`, `InfMap.get_all_parents`, `InfMap.prop_update_chunk`, `hook.Add`, `hook.Run`, `util.AddNetworkString`
  - instance methods: `GetDriver`, `GetNW2Vector`, `GetParent`, `GetWeapons`, `IsConstraint`, `IsEFlagSet`, `IsNPC`, `IsPlayer`, `SetCustomCollisionCheck`, `SetNW2Vector`
- `lua/infmap/sh_inf_obj.lua`
  - globals: `CLIENT`, `CreateMaterial`, `EyePos`, `InfMap`, `IsValid`, `LocalPlayer`, `MATERIAL_LIGHT_DIRECTIONAL`, `Material`, `Mesh`, `SERVER`, `Vector`, `cam`, `coroutine`, `ents`, `file`, `hook`, `ipairs`, `pairs`, `pcall`, `player`, `print`, `render`, `string`, `table`, `timer`, `tonumber`, `util`
  - direct calls: `CreateMaterial(`, `EyePos(`, `IsValid(`, `LocalPlayer(`, `Material(`, `Mesh(`, `Vector(`, `ipairs(`, `pairs(`, `pcall(`, `print(`, `tonumber(`, `InfMap.clear_parsed_objects`, `InfMap.ezcoord`, `InfMap.filter_entities`, `InfMap.localize_vector`, `InfMap.parse_obj`, `InfMap.prop_update_chunk`, `InfMap.unlocalize_vector`, `cam.End3D`, `cam.Start3D`, `coroutine.create`, `coroutine.resume`, `coroutine.status`, `coroutine.yield`, `ents.Create`, `ents.FindByClass`, `file.Read`, `hook.Add`, `hook.Remove`, `player.GetAll`, `render.GetLightColor`, `render.ResetModelLighting`, `render.SetLocalModelLights`, `render.SetMaterial`, `string.Split`, `string.Trim`, `table.Empty`, `table.insert`, `table.remove`, `timer.Simple`, `util.GetSunInfo`
  - fields: `InfMap.parsed_collision_data`, `InfMap.parsed_objects`
  - instance methods: `BuildFromTriangles`, `Cross`, `Destroy`, `Draw`, `GetAngles`, `GetNormalized`, `GetPhysicsObject`, `GetTexture`, `IsValid`, `LengthSqr`, `Remove`, `Rotate`, `SetModel`, `SetTexture`, `Spawn`, `UpdateCollision`
- `lua/infmap/sh_inf_sound.lua`
  - globals: `CreateSound`, `FindMetaTable`, `InfMap`, `IsValid`, `LocalPlayer`, `SERVER`, `SoundObject`, `ents`, `game`, `hook`, `ipairs`, `math`, `net`, `pairs`, `pcall`, `player`, `sound`, `string`, `table`, `tostring`, `util`
  - assigns: `CreateSound`, `SoundObject`
  - direct calls: `FindMetaTable(`, `IsValid(`, `LocalPlayer(`, `SoundObject(`, `ipairs(`, `pairs(`, `pcall(`, `tostring(`, `InfMap.CreateSound`, `InfMap.filter_entities`, `ents.CreateClientProp`, `ents.GetAll`, `game.SinglePlayer`, `hook.Add`, `math.Clamp`, `net.ReadEntity`, `net.ReadFloat`, `net.ReadString`, `net.ReadUInt`, `net.Receive`, `net.Send`, `net.Start`, `net.WriteEntity`, `net.WriteFloat`, `net.WriteString`, `net.WriteUInt`, `player.GetAll`, `sound.GetProperties`, `string.find`, `string.gsub`, `string.lower`, `table.RemoveByValue`, `util.AddNetworkString`, `util.GetSurfaceData`, `util.GetSurfaceIndex`
  - fields: `InfMap.CreateSound`, `InfMap.chunk_size`
  - instance methods: `CallOnRemove`, `ChangeVolume`, `DistToSqr`, `EmitSound`, `GetBoneSurfaceProp`, `GetClass`, `GetParent`, `GetPos`, `InfMap_GetPos`, `InfMap_SetPos`, `InfMap_Stop`, `IsVehicle`, `IsWorld`, `Remove`, `Spawn`, `Stop`, `StopSound`
- `lua/infmap/sv_inf_chunks.lua`
  - globals: `InfMap`, `IsValid`, `MOVETYPE_NOCLIP`, `SafeRemoveEntity`, `Vector`, `coroutine`, `ents`, `hook`, `ipairs`, `math`, `pairs`, `pcall`, `print`, `table`, `timer`
  - direct calls: `IsValid(`, `SafeRemoveEntity(`, `Vector(`, `ipairs(`, `pairs(`, `pcall(`, `print(`, `InfMap.constrained_status`, `InfMap.filter_entities`, `InfMap.in_chunk`, `InfMap.intersect_box`, `InfMap.localize_vector`, `InfMap.prop_update_chunk`, `InfMap.reset_constrained_data`, `coroutine.create`, `coroutine.resume`, `coroutine.yield`, `ents.Create`, `ents.GetAll`, `hook.Add`, `math.Clamp`, `table.remove`, `timer.Create`, `timer.Simple`
  - fields: `InfMap.chunk_size`, `InfMap.gravhull_ents`
  - instance methods: `Alive`, `BoundingRadius`, `ForcePlayerDrop`, `GetAngleVelocity`, `GetAngles`, `GetClass`, `GetModel`, `GetMoveType`, `GetOwner`, `GetParent`, `GetPhysicsObject`, `GetPhysicsObjectCount`, `GetPhysicsObjectNum`, `GetVelocity`, `InfMap_GetPos`, `InfMap_SetPos`, `InfMap_WorldSpaceAABB`, `IsPlayer`, `IsPlayerHolding`, `IsRagdoll`, `IsSolid`, `IsValid`, `SetAngleVelocity`, `SetAngles`, `SetReferenceData`, `SetVelocity`, `Spawn`
- `lua/infmap/sv_inf_detours.lua`
  - globals: `DMG_BURN`, `FindMetaTable`, `InfMap`, `IsEntity`, `IsValid`, `ParticleEffect`, `SF`, `Vector`, `WireLib`, `ents`, `game`, `gmsave`, `hook`, `ipairs`, `isentity`, `istable`, `math`, `net`, `pairs`, `player`, `table`, `timer`, `tostring`, `util`
  - assigns: `ParticleEffect`
  - direct calls: `FindMetaTable(`, `IsValid(`, `Vector(`, `ipairs(`, `isentity(`, `pairs(`, `tostring(`, `InfMap.BlastDamage`, `InfMap.ParticleEffect`, `InfMap.ShouldSaveEntity`, `InfMap.localize_vector`, `InfMap.prop_update_chunk`, `InfMap.unlocalize_vector`, `SF.clampPos`, `WireLib.clampPos`, `ents.FindInBox`, `ents.FindInCone`, `ents.FindInSphere`, `ents.GetAll`, `game.GetWorld`, `gmsave.ShouldSaveEntity`, `hook.Add`, `math.sqrt`, `net.Send`, `net.Start`, `net.WriteAngle`, `net.WriteEntity`, `net.WriteFloat`, `net.WriteString`, `player.GetAll`, `table.insert`, `timer.Simple`, `util.BlastDamage`, `util.IsInWorld`, `util.TraceEntity`, `util.TraceHull`, `util.TraceLine`
  - fields: `InfMap.BlastDamage`, `InfMap.FindInBox`, `InfMap.FindInCone`, `InfMap.FindInSphere`, `InfMap.ParticleEffect`, `InfMap.ShouldSaveEntity`, `InfMap.TraceEntity`, `InfMap.TraceHull`, `InfMap.TraceLine`, `InfMap.disable_pickup`, `ents.FindInBox`, `ents.FindInCone`, `ents.FindInSphere`, `gmsave.ShouldSaveEntity`, `math.huge`, `util.BlastDamage`, `util.TraceEntity`, `util.TraceHull`, `util.TraceLine`
  - instance methods: `DistToSqr`, `Dot`, `GetClass`, `GetEntity`, `GetInflictor`, `GetKeyValues`, `GetNextBot`, `GetNormalized`, `GetOwner`, `GetPos`, `GetRangeSquaredTo`, `InfMap_ApplyForceOffset`, `InfMap_Approach`, `InfMap_CalculateForceOffset`, `InfMap_CalculateVelocityOffset`, `InfMap_DoShootEffect`, `InfMap_EyePos`, `InfMap_FaceTowards`, `InfMap_GetAttachment`, `InfMap_GetBonePosition`, `InfMap_GetDamagePosition`, `InfMap_GetPos`, `InfMap_GetShootPos`, `InfMap_GetVelocityAtPoint`, `InfMap_LocalToWorld`, `InfMap_NearestPoint`, `InfMap_SetEntity`, `InfMap_SetMaterial`, `InfMap_SetPos`, `InfMap_Spawn`, `InfMap_WorldSpaceAABB`, `InfMap_WorldSpaceCenter`, `InfMap_WorldToLocal`, `IsConstraint`, `IsDamageType`, `IsExplosionDamage`, `SetKeyValue`, `SetPos`, `WithinAABox`, `WorldSpaceCenter`
- `lua/simplex.lua`
  - globals: `AddCSLuaFile`, `bit`, `error`, `ipairs`, `math`, `table`, `tonumber`, `unpack`
  - direct calls: `AddCSLuaFile(`, `unpack(`, `bit.band`
  - instance methods: `abs`, `floor`, `insert`, `sin`, `sqrt`

### `cube_ws_2928366506`

Path: `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/addons/cube_ws_2928366506`. Title: Cube extracted WS 2928366506. Lua files: **9**.

Boot subset (50 external globals):

Libraries / tables:

- `ENT` — methods `Initialize`, `OnRemove`, `Spawn`, `UpdateCollision`, `Use`; fields `AdminOnly`, `Author`, `Base`, `Category`, `Editable`, `Information`, `Instructions`, `Model`, `PrintName`, `Purpose`, `Spawnable`, `TargetName`, `Type`
- `InfMap` — methods `clear_parsed_objects`, `parse_obj`, `prop_update_chunk`, `unlocalize_vector`; fields `chunk_size`, `disable_pickup`, `filter`, `water_height`, `water_material`
- `bit` — methods `band`
- `engine` — methods `ActiveGamemode`
- `ents` — methods `Create`, `FindByClass`, `GetAll`
- `game` — methods `GetMap`, `SinglePlayer`
- `hook` — methods `Add`
- `math` — methods `abs`, `min`, `random`; fields `huge`
- `player` — methods `GetAll`
- `render` — methods `EnableClipping`, `PopCustomClipPlane`, `PushCustomClipPlane`
- `string` — methods `Explode`, `len`, `sub`
- `table` — methods `Empty`, `insert`
- `util` — methods `GetModelMeshes`, `IsValidModel`, `TraceHull`

Functions and constructors: `AddCSLuaFile`, `Angle`, `Color`, `DEFINE_BASECLASS`, `DrawMaterialOverlay`, `EyePos`, `EyeVector`, `FrameTime`, `GetConVar`, `IsValid`, `LocalPlayer`, `Material`, `Matrix`, `Mesh`, `RunConsoleCommand`, `SafeRemoveEntity`, `Vector`, `ipairs`, `print`, `tostring`.

Constants and uppercase names: `ACT_MP_SWIM`, `CHAN_BODY`, `CLIENT`, `COLLISION_GROUP_WORLD`, `FL_STATICPROP`, `FSOLID_FORCE_WORLD_ALIGNED`, `FVPHYSICS_CONSTRAINT_STATIC`, `FVPHYSICS_NO_PLAYER_PICKUP`, `FVPHYSICS_NO_SELF_COLLISIONS`, `IN_JUMP`, `MOVETYPE_NONE`, `NULL`, `RENDERMODE_TRANSCOLOR`, `SERVER`, `SIMPLE_USE`, `SOLID_BBOX`, `SOLID_VPHYSICS`.

Defined in this addon: `PlaceCollidersChunks`, `chunks_to_render`, `convertToThreeDigits`, `lod_to_render`, `water_to_render`.

Instance methods: `AddAngleVelocity`, `AddFlags`, `AddGameFlag`, `AddSolidFlags`, `ApplyForceCenter`, `ApplyForceOffset`, `BuildFromTriangles`, `Distance`, `DontDeleteOnRemove`, `Dot`, `DrawModel`, `DrawShadow`, `EmitSound`, `EnableCustomCollisions`, `EnableDrag`, `EnableMotion`, `Extinguish`, `Forward`, `GetAngleVelocity`, `GetButtons`, `GetClass`, `GetFloat`, `GetForwardSpeed`, `GetGravity`, `GetMass`, `GetMaterial`, `GetMaxSpeed`, `GetModel`, `GetMoveAngles`, `GetNormalized`, `GetPhysicsObject`, `GetPos`, `GetSideSpeed`, `GetUpSpeed`, `GetVelocity`, `InVehicle`, `IsAsleep`, `IsOnFire`, `IsOnGround`, `IsPlayer`, `IsPlayerHolding`, `IsValid`, `Length`, `LocalToWorld`, `OBBMaxs`, `OBBMins`, `PhysicsDestroy`, `PhysicsFromMesh`, `Remove`, `Right`, `SetAngles`, `SetCollisionGroup`, `SetColor`, `SetDSP`, `SetMass`, `SetModel`, `SetModelScale`, `SetMoveType`, `SetName`, `SetNoDraw`, `SetOwner`, `SetPos`, `SetRenderBounds`, `SetRenderMode`, `SetScale`, `SetSolid`, `SetTranslation`, `SetUseType`, `SetVelocity`, `Sleep`, `Spawn`, `Up`, `UpdateCollision`, `WithinAABox`.

Per file:

- `lua/autorun/platttt.lua`
  - globals: `SERVER`, `SOLID_VPHYSICS`, `Vector`, `ents`, `game`, `hook`, `math`
  - direct calls: `Vector(`, `ents.Create`, `game.GetMap`, `hook.Add`, `math.random`
  - instance methods: `SetModel`, `SetPos`, `SetSolid`, `Spawn`
- `lua/entities/vicecity_collider.lua`
  - globals: `AddCSLuaFile`, `Angle`, `ENT`, `FL_STATICPROP`, `FSOLID_FORCE_WORLD_ALIGNED`, `FVPHYSICS_CONSTRAINT_STATIC`, `FVPHYSICS_NO_SELF_COLLISIONS`, `InfMap`, `MOVETYPE_NONE`, `Matrix`, `SOLID_VPHYSICS`, `SafeRemoveEntity`, `Vector`, `util`
  - direct calls: `AddCSLuaFile(`, `Angle(`, `Matrix(`, `SafeRemoveEntity(`, `Vector(`, `ENT.Initialize`, `ENT.UpdateCollision`, `InfMap.unlocalize_vector`, `util.GetModelMeshes`
  - fields: `ENT.Author`, `ENT.Base`, `ENT.Category`, `ENT.Instructions`, `ENT.PrintName`, `ENT.Purpose`, `ENT.Spawnable`, `ENT.Type`, `InfMap.disable_pickup`, `InfMap.filter`
  - instance methods: `AddFlags`, `AddGameFlag`, `AddSolidFlags`, `DrawShadow`, `EnableCustomCollisions`, `EnableMotion`, `GetModel`, `GetPhysicsObject`, `PhysicsDestroy`, `PhysicsFromMesh`, `SetAngles`, `SetMass`, `SetMoveType`, `SetNoDraw`, `SetScale`, `SetSolid`, `SetTranslation`, `UpdateCollision`
- `lua/entities/vicecity_water.lua`
  - globals: `AddCSLuaFile`, `COLLISION_GROUP_WORLD`, `Color`, `DEFINE_BASECLASS`, `ENT`, `FVPHYSICS_NO_PLAYER_PICKUP`, `IsValid`, `MOVETYPE_NONE`, `NULL`, `RENDERMODE_TRANSCOLOR`, `SERVER`, `SOLID_BBOX`, `ents`, `hook`
  - direct calls: `AddCSLuaFile(`, `Color(`, `DEFINE_BASECLASS(`, `IsValid(`, `ENT.Initialize`, `ENT.OnRemove`, `ENT.Spawn`, `ents.Create`, `hook.Add`
  - fields: `ENT.AdminOnly`, `ENT.Author`, `ENT.Base`, `ENT.Category`, `ENT.Editable`, `ENT.Information`, `ENT.Model`, `ENT.PrintName`, `ENT.Spawnable`, `ENT.Type`
  - instance methods: `AddGameFlag`, `EnableDrag`, `EnableMotion`, `GetClass`, `GetPhysicsObject`, `GetPos`, `SetCollisionGroup`, `SetColor`, `SetModel`, `SetMoveType`, `SetOwner`, `SetPos`, `SetRenderMode`, `SetSolid`, `Spawn`
- `lua/entities/vicecity_waypoint.lua`
  - globals: `AddCSLuaFile`, `COLLISION_GROUP_WORLD`, `DEFINE_BASECLASS`, `ENT`, `FVPHYSICS_NO_PLAYER_PICKUP`, `IsValid`, `MOVETYPE_NONE`, `NULL`, `SERVER`, `SIMPLE_USE`, `SOLID_BBOX`, `ents`, `ipairs`, `math`, `table`
  - direct calls: `AddCSLuaFile(`, `DEFINE_BASECLASS(`, `IsValid(`, `ipairs(`, `ENT.Initialize`, `ENT.OnRemove`, `ENT.Spawn`, `ENT.Use`, `ents.Create`, `ents.GetAll`, `table.insert`
  - fields: `ENT.AdminOnly`, `ENT.Author`, `ENT.Base`, `ENT.Category`, `ENT.Editable`, `ENT.Information`, `ENT.Model`, `ENT.PrintName`, `ENT.Spawnable`, `ENT.TargetName`, `ENT.Type`, `math.huge`
  - instance methods: `AddGameFlag`, `Distance`, `DontDeleteOnRemove`, `EnableDrag`, `EnableMotion`, `GetClass`, `GetPhysicsObject`, `GetPos`, `GetVelocity`, `IsPlayer`, `SetCollisionGroup`, `SetModel`, `SetMoveType`, `SetOwner`, `SetPos`, `SetSolid`, `SetUseType`, `SetVelocity`, `Spawn`
- `lua/infmap/gm_infmap_vicecity/cl_rendermap.lua`
  - globals: `EyeVector`, `InfMap`, `IsValid`, `LocalPlayer`, `RunConsoleCommand`, `Vector`, `chunks_to_render`, `ents`, `hook`, `ipairs`, `lod_to_render`, `render`, `string`, `table`
  - assigns: `chunks_to_render`, `lod_to_render`, `water_to_render`
  - direct calls: `EyeVector(`, `IsValid(`, `LocalPlayer(`, `RunConsoleCommand(`, `Vector(`, `ipairs(`, `InfMap.unlocalize_vector`, `ents.FindByClass`, `hook.Add`, `render.EnableClipping`, `render.PopCustomClipPlane`, `render.PushCustomClipPlane`, `string.Explode`, `string.len`, `string.sub`, `table.Empty`, `table.insert`
  - instance methods: `Dot`, `DrawModel`, `GetModel`, `GetPos`, `SetNoDraw`, `SetRenderBounds`, `WithinAABox`
- `lua/infmap/gm_infmap_vicecity/loadobject.lua`
  - globals: `Angle`, `InfMap`, `Matrix`, `Vector`
  - direct calls: `Angle(`, `Matrix(`, `Vector(`, `InfMap.clear_parsed_objects`, `InfMap.parse_obj`
  - fields: `InfMap.chunk_size`
  - instance methods: `SetAngles`, `SetScale`, `SetTranslation`
- `lua/infmap/gm_infmap_vicecity/sh_collision.lua`
  - globals: `CLIENT`, `InfMap`, `PlaceCollidersChunks`, `convertToThreeDigits`, `engine`, `ents`, `game`, `hook`, `ipairs`, `player`, `print`, `string`, `tostring`, `util`
  - assigns: `PlaceCollidersChunks`, `convertToThreeDigits`
  - direct calls: `PlaceCollidersChunks(`, `convertToThreeDigits(`, `ipairs(`, `print(`, `tostring(`, `InfMap.prop_update_chunk`, `engine.ActiveGamemode`, `ents.Create`, `ents.FindByClass`, `game.SinglePlayer`, `hook.Add`, `player.GetAll`, `string.len`, `util.IsValidModel`
  - instance methods: `IsPlayer`, `Remove`, `SetModel`, `Spawn`
- `lua/infmap/gm_infmap_vicecity/sh_terrain_water.lua`
  - globals: `ACT_MP_SWIM`, `CHAN_BODY`, `CLIENT`, `DrawMaterialOverlay`, `EyePos`, `FrameTime`, `GetConVar`, `IN_JUMP`, `InfMap`, `IsValid`, `LocalPlayer`, `Material`, `Matrix`, `Mesh`, `SERVER`, `Vector`, `bit`, `ents`, `hook`, `ipairs`, `math`, `util`
  - direct calls: `DrawMaterialOverlay(`, `EyePos(`, `FrameTime(`, `GetConVar(`, `LocalPlayer(`, `Material(`, `Matrix(`, `Mesh(`, `Vector(`, `ipairs(`, `InfMap.unlocalize_vector`, `bit.band`, `ents.FindByClass`, `hook.Add`, `math.abs`, `math.min`, `util.TraceHull`
  - fields: `InfMap.water_height`, `InfMap.water_material`
  - instance methods: `AddAngleVelocity`, `ApplyForceCenter`, `ApplyForceOffset`, `BuildFromTriangles`, `EmitSound`, `Extinguish`, `Forward`, `GetAngleVelocity`, `GetButtons`, `GetClass`, `GetFloat`, `GetForwardSpeed`, `GetGravity`, `GetMass`, `GetMaterial`, `GetMaxSpeed`, `GetMoveAngles`, `GetNormalized`, `GetPhysicsObject`, `GetPos`, `GetSideSpeed`, `GetUpSpeed`, `GetVelocity`, `InVehicle`, `IsAsleep`, `IsOnFire`, `IsOnGround`, `IsPlayerHolding`, `IsValid`, `Length`, `LocalToWorld`, `OBBMaxs`, `OBBMins`, `Right`, `SetDSP`, `SetScale`, `SetVelocity`, `Sleep`, `Up`
- `lua/infmap/gm_infmap_vicecity/sv_skycameraplace.lua`
  - globals: `Angle`, `Vector`, `ents`, `hook`
  - direct calls: `Angle(`, `Vector(`, `ents.Create`, `hook.Add`
  - instance methods: `SetAngles`, `SetModel`, `SetModelScale`, `SetName`, `SetPos`, `Spawn`

### `cube_ws_3801628931`

Path: `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/addons/cube_ws_3801628931`. Title: Cube extracted WS 3801628931. Lua files: **0**.

No `.lua` files. Nothing to boot. Contents are maps and/or materials only.

### `vrmod_climbing`

Path: `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/addons/vrmod_climbing`. Title: [VRMod] Brush Climbing. Lua files: **7**.
Entry symlink → `/home/voldemar/Dev/GMod/vrmod_climbing`.

Boot subset (66 external globals):

Libraries / tables:

- `bit` — methods `band`, `bor`
- `cam` — methods `End3D2D`, `Start3D2D`
- `concommand` — methods `Add`
- `cvars` — methods `AddChangeCallback`, `RemoveChangeCallback`
- `draw` — methods `SimpleTextOutlined`
- `file` — methods `CreateDir`, `Delete`, `Exists`, `Find`, `Read`, `Write`
- `hook` — methods `Add`, `Remove`
- `math` — methods `Clamp`, `abs`, `max`, `random`; fields `huge`
- `net` — methods `ReadBool`, `ReadEntity`, `ReadFloat`, `ReadVector`, `Receive`, `SendToServer`, `Start`, `WriteBool`, `WriteEntity`, `WriteFloat`, `WriteVector`
- `notification` — methods `AddLegacy`; also referenced without calling `AddLegacy`
- `render` — methods `DrawLine`, `DrawWireframeBox`, `DrawWireframeSphere`, `SetColorMaterial`
- `spawnmenu` — methods `AddToolCategory`, `AddToolMenuOption`
- `string` — methods `StartWith`, `Trim`, `find`, `format`, `gsub`, `lower`, `match`, `sub`
- `surface` — methods `PlaySound`; also referenced without calling `PlaySound`
- `table` — methods `sort`
- `timer` — methods `Simple`
- `util` — methods `AddNetworkString`, `JSONToTable`, `PointContents`, `TableToJSON`, `TraceHull`, `TraceLine`
- `vgui` — methods `Create`

Functions and constructors: `AddCSLuaFile`, `Angle`, `Color`, `CreateClientConVar`, `CreateConVar`, `CreateSound`, `CurTime`, `Derma_Query`, `Derma_StringRequest`, `EyePos`, `FrameNumber`, `FrameTime`, `GetConVar`, `IsValid`, `LerpVector`, `LocalPlayer`, `LocalToWorld`, `RunConsoleCommand`, `Vector`, `include`, `isfunction`, `isstring`, `istable`, `pairs`, `tobool`, `tostring`.

Constants and uppercase names: `CHAN_AUTO`, `CONTENTS_LADDER`, `FCVAR_ARCHIVE`, `FCVAR_NOTIFY`, `FCVAR_REPLICATED`, `FILL`, `FL_ONGROUND`, `IN_DUCK`, `IN_JUMP`, `MASK_PLAYERSOLID`, `MASK_SOLID`, `MOVETYPE_NOCLIP`, `MOVETYPE_NONE`, `MOVETYPE_OBSERVER`, `NOTIFY_ERROR`, `NOTIFY_GENERIC`, `NOTIFY_HINT`, `SERVER`, `SOLID_NONE`, `TEXT_ALIGN_CENTER`, `TOP`.

Other reads: `vrWallRunWants`.

Defined in this addon: `g_VR`, `vrmod`.

Host tables `g_VR`, `vrmod` are assigned with `Name = Name or {}`. A missing global does not throw on that line. Later `Name.Method` calls error unless VRMod already created the real tables. Do not treat those assignments as this addon implementing VRMod.

`vrWallRunWants` is read inside `vrmod.climbing.GetState` before `local vrWallRunWants` is declared later in the same chunk. Lua does not retroactively bind that local, so the early read is a real global. It is an addon scoping bug, not an engine symbol to implement.

Instance methods: `AddChoice`, `AddItem`, `AddSheet`, `Alive`, `Angle`, `Center`, `ChangeVolume`, `CheckBox`, `ChooseOptionID`, `Clear`, `ClearControls`, `ControlHelp`, `Cross`, `DistToSqr`, `Dock`, `DockMargin`, `Dot`, `EmitSound`, `EyeAngles`, `Fire`, `Forward`, `GetBool`, `GetButtons`, `GetChecked`, `GetClass`, `GetDefault`, `GetFlags`, `GetFloat`, `GetForwardSpeed`, `GetHull`, `GetHullDuck`, `GetInt`, `GetModel`, `GetMoveType`, `GetNormalized`, `GetOptionData`, `GetPos`, `GetSelectedID`, `GetSolid`, `GetString`, `GetText`, `GetValue`, `GetVelocity`, `Help`, `InVehicle`, `IsAdmin`, `IsOnGround`, `IsPlaying`, `KeyPressed`, `Length`, `LengthSqr`, `MakePopup`, `Normalize`, `NumSlider`, `PlayEx`, `SetButtons`, `SetColor`, `SetDecimals`, `SetForwardSpeed`, `SetInt`, `SetLocalVelocity`, `SetMax`, `SetMin`, `SetName`, `SetPos`, `SetSideSpeed`, `SetSize`, `SetString`, `SetTall`, `SetText`, `SetTitle`, `SetTooltip`, `SetUpSpeed`, `SetValue`, `SetVelocity`, `SetVisible`, `SizeToContents`, `Stop`, `setEnsureSlideLoopSound`, `setStopSlideLoopSound`, `setStopWallRunSignal`, `setUpdateSlide`, `setUpdateWallRun`.

Per file:

- `lua/autorun/vrmod_climbing.lua`
  - globals: `AddCSLuaFile`, `SERVER`, `include`
  - direct calls: `AddCSLuaFile(`, `include(`
  - literal includes: `AddCSLuaFile` `vrmod_climbing/client/core.lua`, `AddCSLuaFile` `vrmod_climbing/client/wallrun.lua`, `AddCSLuaFile` `vrmod_climbing/client/slide.lua`, `AddCSLuaFile` `vrmod_climbing/client/ui.lua`, `AddCSLuaFile` `vrmod_climbing/client/presets.lua`, `include` `vrmod_climbing/server.lua`, `include` `vrmod_climbing/client/core.lua`, `include` `vrmod_climbing/client/ui.lua`
- `lua/vrmod_climbing/client/core.lua`
  - globals: `Angle`, `CHAN_AUTO`, `CONTENTS_LADDER`, `Color`, `CreateClientConVar`, `CurTime`, `EyePos`, `FrameNumber`, `GetConVar`, `IsValid`, `LerpVector`, `LocalPlayer`, `LocalToWorld`, `MASK_PLAYERSOLID`, `MASK_SOLID`, `MOVETYPE_NONE`, `RunConsoleCommand`, `SOLID_NONE`, `TEXT_ALIGN_CENTER`, `Vector`, `bit`, `cam`, `cvars`, `draw`, `g_VR`, `hook`, `include`, `isfunction`, `isstring`, `math`, `net`, `pairs`, `render`, `string`, `surface`, `timer`, `tobool`, `tostring`, `util`, `vrmod`
  - assigns: `g_VR`, `vrmod`
  - direct calls: `Angle(`, `Color(`, `CreateClientConVar(`, `CurTime(`, `EyePos(`, `FrameNumber(`, `GetConVar(`, `IsValid(`, `LerpVector(`, `LocalPlayer(`, `LocalToWorld(`, `RunConsoleCommand(`, `Vector(`, `include(`, `isfunction(`, `isstring(`, `pairs(`, `tobool(`, `tostring(`, `bit.band`, `cam.End3D2D`, `cam.Start3D2D`, `cvars.AddChangeCallback`, `cvars.RemoveChangeCallback`, `draw.SimpleTextOutlined`, `hook.Add`, `hook.Remove`, `math.Clamp`, `math.max`, `math.random`, `net.SendToServer`, `net.Start`, `net.WriteBool`, `net.WriteEntity`, `net.WriteFloat`, `net.WriteVector`, `render.DrawLine`, `render.DrawWireframeBox`, `render.DrawWireframeSphere`, `render.SetColorMaterial`, `string.StartWith`, `string.find`, `string.format`, `string.lower`, `string.sub`, `surface.PlaySound`, `timer.Simple`, `util.PointContents`, `util.TraceHull`, `util.TraceLine`, `vrmod.StartLocomotion`, `vrmod.StopLocomotion`, `vrmod.climbing`
  - fields: `g_VR.active`, `g_VR.input`, `g_VR.origin`, `g_VR.tracking`, `math.huge`, `vrmod.StartLocomotion`, `vrmod.StopLocomotion`, `vrmod.climbing`
  - instance methods: `Angle`, `DistToSqr`, `EmitSound`, `Forward`, `GetBool`, `GetClass`, `GetFloat`, `GetHull`, `GetHullDuck`, `GetInt`, `GetModel`, `GetMoveType`, `GetNormalized`, `GetPos`, `GetSolid`, `InVehicle`, `IsAdmin`, `IsOnGround`, `Length`, `LengthSqr`, `Normalize`, `SetPos`
  - literal includes: `include` `vrmod_climbing/client/wallrun.lua`, `include` `vrmod_climbing/client/slide.lua`
- `lua/vrmod_climbing/client/presets.lua`
  - globals: —
- `lua/vrmod_climbing/client/slide.lua`
  - globals: `CreateSound`, `FrameTime`, `IsValid`, `LocalPlayer`, `Vector`, `g_VR`, `isfunction`, `istable`, `math`, `net`, `util`
  - direct calls: `CreateSound(`, `FrameTime(`, `IsValid(`, `LocalPlayer(`, `Vector(`, `isfunction(`, `istable(`, `math.max`, `net.SendToServer`, `net.Start`, `net.WriteBool`, `net.WriteVector`, `util.TraceLine`
  - fields: `g_VR.active`, `g_VR.origin`, `g_VR.tracking`
  - instance methods: `ChangeVolume`, `Dot`, `GetBool`, `GetFloat`, `GetNormalized`, `GetPos`, `GetVelocity`, `IsPlaying`, `Length`, `LengthSqr`, `Normalize`, `PlayEx`, `SetLocalVelocity`, `Stop`, `setEnsureSlideLoopSound`, `setStopSlideLoopSound`, `setUpdateSlide`
- `lua/vrmod_climbing/client/ui.lua`
  - globals: `Color`, `CreateClientConVar`, `Derma_Query`, `Derma_StringRequest`, `FILL`, `GetConVar`, `IsValid`, `LocalPlayer`, `NOTIFY_ERROR`, `NOTIFY_GENERIC`, `NOTIFY_HINT`, `RunConsoleCommand`, `SERVER`, `TOP`, `concommand`, `file`, `hook`, `include`, `isstring`, `istable`, `math`, `notification`, `pairs`, `spawnmenu`, `string`, `surface`, `table`, `timer`, `tostring`, `util`, `vgui`, `vrmod`
  - direct calls: `Color(`, `CreateClientConVar(`, `Derma_Query(`, `Derma_StringRequest(`, `GetConVar(`, `IsValid(`, `LocalPlayer(`, `RunConsoleCommand(`, `include(`, `isstring(`, `istable(`, `pairs(`, `tostring(`, `concommand.Add`, `file.CreateDir`, `file.Delete`, `file.Exists`, `file.Find`, `file.Read`, `file.Write`, `hook.Add`, `math.abs`, `notification.AddLegacy`, `spawnmenu.AddToolCategory`, `spawnmenu.AddToolMenuOption`, `string.Trim`, `string.gsub`, `string.lower`, `string.match`, `string.sub`, `surface.PlaySound`, `table.sort`, `timer.Simple`, `util.JSONToTable`, `util.TableToJSON`, `vgui.Create`, `vrmod.AddInGameMenuItem`
  - fields: `notification.AddLegacy`, `surface.PlaySound`, `vrmod.AddInGameMenuItem`
  - instance methods: `AddChoice`, `AddItem`, `AddSheet`, `Center`, `CheckBox`, `ChooseOptionID`, `Clear`, `ClearControls`, `ControlHelp`, `Dock`, `DockMargin`, `GetBool`, `GetChecked`, `GetDefault`, `GetFloat`, `GetInt`, `GetOptionData`, `GetSelectedID`, `GetString`, `GetText`, `GetValue`, `Help`, `IsAdmin`, `MakePopup`, `NumSlider`, `SetColor`, `SetDecimals`, `SetInt`, `SetMax`, `SetMin`, `SetName`, `SetPos`, `SetSize`, `SetString`, `SetTall`, `SetText`, `SetTitle`, `SetTooltip`, `SetValue`, `SetVisible`, `SizeToContents`
  - literal includes: `include` `vrmod_climbing/client/presets.lua`
- `lua/vrmod_climbing/client/wallrun.lua`
  - globals: `CreateClientConVar`, `CurTime`, `FrameTime`, `IsValid`, `LocalPlayer`, `LocalToWorld`, `MASK_SOLID`, `SOLID_NONE`, `Vector`, `g_VR`, `isfunction`, `istable`, `math`, `net`, `pairs`, `util`
  - direct calls: `CreateClientConVar(`, `CurTime(`, `FrameTime(`, `IsValid(`, `LocalPlayer(`, `LocalToWorld(`, `Vector(`, `isfunction(`, `istable(`, `pairs(`, `math.Clamp`, `math.abs`, `net.SendToServer`, `net.Start`, `net.WriteBool`, `net.WriteVector`, `util.TraceLine`
  - fields: `g_VR.tracking`
  - instance methods: `Dot`, `EyeAngles`, `Forward`, `GetBool`, `GetFloat`, `GetInt`, `GetNormalized`, `GetSolid`, `InVehicle`, `IsOnGround`, `Length`, `LengthSqr`, `Normalize`, `setStopWallRunSignal`, `setUpdateWallRun`
- `lua/vrmod_climbing/server.lua`
  - globals: `AddCSLuaFile`, `CreateConVar`, `CurTime`, `FCVAR_ARCHIVE`, `FCVAR_NOTIFY`, `FCVAR_REPLICATED`, `FL_ONGROUND`, `FrameTime`, `IN_DUCK`, `IN_JUMP`, `IsValid`, `LerpVector`, `MASK_PLAYERSOLID`, `MOVETYPE_NOCLIP`, `MOVETYPE_OBSERVER`, `SERVER`, `Vector`, `bit`, `concommand`, `hook`, `math`, `net`, `tostring`, `util`, `vrWallRunWants`, `vrmod`
  - assigns: `vrmod`
  - direct calls: `AddCSLuaFile(`, `CreateConVar(`, `CurTime(`, `FrameTime(`, `IsValid(`, `LerpVector(`, `Vector(`, `tostring(`, `bit.band`, `bit.bor`, `concommand.Add`, `hook.Add`, `math.Clamp`, `math.max`, `math.random`, `net.ReadBool`, `net.ReadEntity`, `net.ReadFloat`, `net.ReadVector`, `net.Receive`, `util.AddNetworkString`, `util.TraceHull`, `vrmod.NetReceiveLimited`, `vrmod.climbing`
  - fields: `vrmod.NetReceiveLimited`, `vrmod.climbing`
  - instance methods: `Alive`, `Cross`, `DistToSqr`, `Dot`, `EmitSound`, `EyeAngles`, `Fire`, `Forward`, `GetBool`, `GetButtons`, `GetClass`, `GetFlags`, `GetFloat`, `GetForwardSpeed`, `GetHull`, `GetHullDuck`, `GetMoveType`, `GetNormalized`, `GetPos`, `GetVelocity`, `IsAdmin`, `IsOnGround`, `KeyPressed`, `Length`, `LengthSqr`, `Normalize`, `SetButtons`, `SetForwardSpeed`, `SetPos`, `SetSideSpeed`, `SetString`, `SetUpSpeed`, `SetVelocity`

## 4. What this does not prove

This is a static scan, not proof the addon runs. Nothing here was executed in Garry's Mod, on a client, or on a server. A file can parse and still error at runtime because a global is the wrong type, a method is missing, a realm branch (`SERVER` / `CLIENT`) was not taken, an `include` path is built at runtime, or a hook order differs. The one Wire Expression 2 file is not Lua the stock VM will load. Map-only workshop folders can still fail in-game for reasons that do not appear in Lua. Detours, `local alias = alias` copies, and unbound names such as `pos` are called out in section 3; naming them is still not a run. Symlinked copies are the same bytes as their targets; counting them once does not mean Steam and the repo can diverge later without this inventory noticing.

