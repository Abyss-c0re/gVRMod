# Reversed GMod WebUI → Native Cube OpenXR Launcher

**Law:** The launcher is **not** Garry's Mod. It is a native OpenXR app that
reimplements the **stock WebUI New Game contract**, then spawns GMod only on
`StartGame`.

## Stock sources (GMod install)

| Asset | Role |
|-------|------|
| `garrysmod/html/menu.html` | Shell: NavBar, routes, gamemode picker |
| `garrysmod/html/template/newgame.html` | New Game layout (categories · maps · settings · Start) |
| `garrysmod/html/js/menu/control.NewGame.js` | Controller: SelectMap, StartGame, MaxPlayers |
| `garrysmod/html/css/menu/NewGame.css` | Visual structure (icons, gamesettings column) |
| `garrysmod/lua/menu/getmaps.lua` | Map → category patterns (`gm_` → Sandbox, …) |
| `garrysmod/lua/menu/mainmenu.lua` | Lua bridge `UpdateServerSettings`, favourites |

## WebUI New Game contract (reversed)

### State

```
CurrentCategory : string   // default "Sandbox" or "Favourites"
Map             : string   // e.g. gm_construct / gm_flatgrass
MaxPlayers      : 1|2|4|8|16|32|64|128
ServerSettings  :
  hostname, sv_lan, p2p_enabled, p2p_friendsonly
  + gamemode Text/Numeric/CheckBox rows (phase 2)
SearchText      : string
MapList[]       : { category, maps[], order }
```

### Actions (from control.NewGame.js)

| UI | JS | Native |
|----|-----|--------|
| Category click | `SwitchCategory(cat)` | select category |
| Map click | `SelectMap(m)` | select map |
| Map double-click | `DblClickMap` → `StartGame` | start |
| Max players | `UpdateMaxPlayers` | cycle / pick |
| Start button | `StartGame()` | spawn GMod |
| Fav | `ToggleFavourite` | phase 2 |

### StartGame sequence (exact reverse of JS)

```
1. SaveLastMap(map, category)          // optional local JSON
2. progress_enable
3. disconnect                          // n/a (GMod not running yet)
4. maxplayers N
5. sv_cheats 0
6. apply ServerSettings cvars
7. hostname / p2p_* / sv_lan
8. maxplayers N again
9. map <Map>
```

Native translation → process spawn:

```
steam -applaunch 4000
  -windowed -w 720 -h 480
  +maxplayers N +sv_lan L +hostname H
  +map MAP
  +exec gvrmod_cube          // OpenXR autostart inside GMod
```

Plus `data/vrmod/openxr_launch.txt` written **before** spawn so in-game VR boots.

## Layout (newgame.html reverse)

```
┌────────────┬──────────────────────────┬─────────────────┐
│ Categories │ Map grid (icons/names)   │ Game settings   │
│ 200px      │ scrollable               │ maxplayers      │
│            │                          │ hostname (MP)   │
│            │                          │ LAN / P2P       │
│            │                          │ [ START GAME ]  │
└────────────┴──────────────────────────┴─────────────────┘
```

Rendered as a **stereo OpenGL panel** in OpenXR (not CEF, not GMod VGUI).

## Product flow

```
Desktop icon → cube_webui_launcher (OpenXR native)

## Controls (headset)
- **Trigger** — click UI under laser
- **Grip / squeeze** — grab and move the **world-locked** menu
- **Menu / A** — reset panel pose to `cube_webui.conf` defaults
- **Thumbstick** — navigate when not grabbing

## World-locked panel (default)
`view_lock=0` — panel is seeded in front of you, then **frozen in LOCAL space**.
It does **not** follow head turn/walk; grip repositions it. Optional `view_lock=1` is HUD head-follow.

## Seamless StartGame (no black gap)
Cube keeps the OpenXR session and shows a **STARTING GMOD** panel after Start.
Lua writes `garrysmod/data/vrmod/cube_handoff.txt` (`phase=take_xr`) before claiming XR;
native exits the session only then so GMod can take over without an early void.

## Config (no recompile)
Project defaults: `native_launcher/cube_webui.conf` (copied to `install/native/`).
User override: `~/.config/gvrmod/cube_webui.conf`.

| key | meaning |
|-----|---------|
| `panel_dist` / `panel_w` / `panel_h` | size & depth (m) |
| `panel_x` / `panel_y` / `panel_z` | seed offsets |
| `view_lock` | `0` world (default), `1` head-follow |
| `passthrough` | prefer ALPHA_BLEND / FB passthrough |
| `panel_alpha` | UI opacity over real world |
| `grab_thresh` | squeeze threshold to grab |

Env: `CUBE_PANEL_*`, `CUBE_PASSTHROUGH`, `CUBE_VIEW_LOCK`.
                    │
                    ├─ browse maps (scan garrysmod/maps/*.bsp)
                    ├─ pick players / LAN
                    └─ START GAME
                           │
                           ▼
                    spawn GMod + map + OpenXR autostart
                    (Facepunch load only after user chose)
```

## Addons manager (reversed control.Addons.js)

| Stock | Native |
|-------|--------|
| `html/template/addon_list.html` | ADDONS page tab |
| `control.Addons.js` subscribed list | scan workshop + local |
| `steamworks.SetShouldMountAddon(id,false)` | write `cfg/addonnomount.txt` VDF |
| Local folder enable | rename `addons/X` ↔ `addons/X.disabled` |

### State

```
addons[] : { id, title, kind=workshop|local, enabled }
nomount  : set of workshop ids (addonnomount.txt)
```

### Actions

| UI | Effect |
|----|--------|
| Select addon | highlight |
| Trigger / click / toggle | enable ↔ disable |
| Tab NEW GAME ↔ ADDONS | page switch |

Mount changes apply on next GMod Start Game (engine reads addonnomount at boot).

## Non-goals

- Embedding Awesomium/CEF in the native launcher (v1)
- Projecting stock GameUI into GMod first
- Live Steam Workshop subscribe API (v1 lists downloaded only)
- Requiring console commands to enter VR
