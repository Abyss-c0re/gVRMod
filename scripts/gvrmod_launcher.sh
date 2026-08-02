#!/usr/bin/env bash
# =============================================================================
# gVRMod OpenXR launcher  (HL2VR-inspired: bg map + auto VR)
#
# Crash fixes:
#   • NEVER put garrysmod/lua/bin on LD_LIBRARY_PATH (install.sh's private
#     libc.so.6 there → SIGSEGV at SDL/GL init).
#   • Native hl2.sh needs Steam Runtime x86_64 libs for libssl.so.1.0.0.
#   • OpenXR: XR_RUNTIME_JSON + ~/.config/openxr/1/active_runtime.json
#   • Lua force-start: data/vrmod/openxr_launch.txt marker
#
# Usage:
#   ./scripts/gvrmod_launcher.sh                 # default: native hl2.sh
#   ./scripts/gvrmod_launcher.sh --steam         # steam -applaunch (reliable)
#   ./scripts/gvrmod_launcher.sh --map gm_flatgrass
#   ./scripts/gvrmod_launcher.sh --play-map      # full +map (not background)
#   ./scripts/gvrmod_launcher.sh --hub
#   ./scripts/gvrmod_launcher.sh -- %command%    # Steam Launch Options wrapper
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

resolve_gmod() {
  local c
  for c in \
    "${GMOD_DIR:-}" \
    "$HOME/.steam/steam/steamapps/common/GarrysMod" \
    "$HOME/.local/share/Steam/steamapps/common/GarrysMod" \
    "$HOME/.steam/root/steamapps/common/GarrysMod" \
    "$HOME/Steam/steamapps/common/GarrysMod"
  do
    [[ -n "$c" && -x "$c/hl2.sh" ]] && { echo "$c"; return 0; }
  done
  return 1
}

resolve_steam_runtime_x64() {
  local base lib usr
  for base in \
    "$HOME/.local/share/Steam/ubuntu12_32/steam-runtime" \
    "$HOME/.steam/steam/ubuntu12_32/steam-runtime" \
    "$HOME/.steam/root/ubuntu12_32/steam-runtime" \
    "$HOME/.local/share/Steam/steamapps/common/SteamLinuxRuntime/steam-runtime"
  do
    lib="$base/lib/x86_64-linux-gnu"
    usr="$base/usr/lib/x86_64-linux-gnu"
    if [[ -f "$lib/libssl.so.1.0.0" || -f "$usr/libssl.so.1.0.0" ]]; then
      echo "$lib:$usr"
      return 0
    fi
  done
  return 1
}

GMOD="$(resolve_gmod)" || {
  echo "[gVRMod] ERROR: Garry's Mod not found (need hl2.sh). Set GMOD_DIR=..." >&2
  exit 1
}

APPID=4000
MAP="gm_construct"
USE_MAP=1
MAP_MODE="background"   # background | full | none
MODE="menu"             # menu | hub
FORCE_STEAM=0
NO_WIVRN=0
EXTRA_ARGS=()
WRAPPER=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --map) MAP="${2:-gm_construct}"; USE_MAP=1; shift 2 ;;
    --no-map) USE_MAP=0; MAP=""; MAP_MODE="none"; shift ;;
    --play-map|--full-map) MAP_MODE="full"; USE_MAP=1; MAP="${MAP:-gm_construct}"; shift ;;
    --background|--bg) MAP_MODE="background"; USE_MAP=1; MAP="${MAP:-gm_construct}"; shift ;;
    --hub)
      MODE="hub"
      MAP_MODE="full"
      USE_MAP=1
      MAP="${MAP:-gm_construct}"
      shift
      ;;
    --menu) MODE="menu"; MAP_MODE="background"; shift ;;
    --steam) FORCE_STEAM=1; shift ;;
    --native) FORCE_STEAM=0; shift ;;
    --no-wivrn) NO_WIVRN=1; shift ;;
    --gmod-dir) GMOD="$2"; shift 2 ;;
    --) shift; EXTRA_ARGS+=("$@"); WRAPPER=1; break ;;
    -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
    *) EXTRA_ARGS+=("$1"); shift ;;
  esac
done

if [[ "$MODE" == "menu" && "$MAP_MODE" != "none" && -z "$MAP" ]]; then
  MAP="gm_construct"; USE_MAP=1; MAP_MODE="background"
fi
if [[ "$MODE" == "hub" ]]; then
  MAP_MODE="full"; MAP="${MAP:-gm_construct}"; USE_MAP=1
fi

# ── OpenXR ──────────────────────────────────────────────────────────────────
XR_JSON="${XR_RUNTIME_JSON:-}"
if [[ -z "$XR_JSON" ]]; then
  for c in \
    /usr/share/openxr/1/openxr_wivrn.json \
    /usr/local/share/openxr/1/openxr_wivrn.json \
    /usr/share/openxr/1/openxr_monado.json
  do
    [[ -f "$c" ]] && { XR_JSON="$c"; break; }
  done
fi
if [[ -z "$XR_JSON" || ! -f "$XR_JSON" ]]; then
  echo "[gVRMod] ERROR: no OpenXR runtime JSON (install WiVRn/Monado)." >&2
  exit 1
fi
export XR_RUNTIME_JSON="$XR_JSON"
mkdir -p "$HOME/.config/openxr/1"
ln -sfn "$XR_RUNTIME_JSON" "$HOME/.config/openxr/1/active_runtime.json"

# ── LD_LIBRARY_PATH: steam openssl ONLY — never lua/bin, never bare /usr/lib ─
# lua/bin has private libc → SIGSEGV. OpenXR runtime is found via JSON path.
STEAM_RT_X64="$(resolve_steam_runtime_x64 || true)"
LIB_PATHS=()
# Only WiVRn dir if needed for non-JSON side deps (loader still uses JSON path)
[[ -d /usr/lib/wivrn ]] && LIB_PATHS+=("/usr/lib/wivrn")
[[ -d /usr/lib64/wivrn ]] && LIB_PATHS+=("/usr/lib64/wivrn")
if [[ -n "$STEAM_RT_X64" ]]; then
  # append so bin/linux64 (from hl2.sh) still wins for game libs
  LIB_PATHS+=("${STEAM_RT_X64//:/ }")
  # re-split properly
  LIB_PATHS=()
  [[ -d /usr/lib/wivrn ]] && LIB_PATHS+=("/usr/lib/wivrn")
  [[ -d /usr/lib64/wivrn ]] && LIB_PATHS+=("/usr/lib64/wivrn")
  IFS=':' read -r -a _sr <<< "$STEAM_RT_X64"
  for p in "${_sr[@]}"; do
    [[ -d "$p" ]] && LIB_PATHS+=("$p")
  done
fi

# Strip any caller-supplied bad paths
CLEAN_LD="${LD_LIBRARY_PATH:-}"
if [[ -n "$CLEAN_LD" ]]; then
  CLEAN_LD="$(echo ":$CLEAN_LD:" | sed \
    -e "s|:$GMOD/garrysmod/lua/bin:|:|g" \
    -e "s|:${GMOD//\//\\/}/garrysmod/lua/bin:|:|g" \
    -e 's|^:||;s|:$||;s|::*| :|g' | tr -s ':' | sed 's/^://;s/:$//')"
fi

NEW_LD=""
if [[ ${#LIB_PATHS[@]} -gt 0 ]]; then
  NEW_LD="$(IFS=:; echo "${LIB_PATHS[*]}")"
fi
if [[ -n "$CLEAN_LD" ]]; then
  NEW_LD="${NEW_LD:+$NEW_LD:}$CLEAN_LD"
fi
export LD_LIBRARY_PATH="${NEW_LD:-}"

export SteamAppId="$APPID"
export SteamGameId="$APPID"
export SteamOverlayGameId="$APPID"
echo "$APPID" > "$GMOD/steam_appid.txt" 2>/dev/null || true

# Wayland SDL often crashes Source — prefer X11
if [[ "${GVMOD_ALLOW_WAYLAND:-0}" != "1" ]]; then
  if [[ "${XDG_SESSION_TYPE:-}" == "wayland" || -n "${WAYLAND_DISPLAY:-}" ]]; then
    export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11}"
  fi
fi

# ── WiVRn ───────────────────────────────────────────────────────────────────
if [[ "$NO_WIVRN" != "1" ]]; then
  if ! pgrep -x wivrn-server >/dev/null 2>&1; then
    if command -v wivrn-server >/dev/null 2>&1; then
      echo "[gVRMod] starting wivrn-server…"
      wivrn-server >/tmp/gvrmod-wivrn.log 2>&1 &
      sleep 0.8
    elif systemctl --user list-unit-files wivrn-server.service &>/dev/null; then
      systemctl --user start wivrn-server.service 2>/dev/null || true
      sleep 0.5
    fi
  else
    echo "[gVRMod] wivrn-server already running"
  fi
fi

# ── cfg + marker ────────────────────────────────────────────────────────────
CFG_DIR="$GMOD/garrysmod/cfg"
DATA_DIR="$GMOD/garrysmod/data/vrmod"
mkdir -p "$CFG_DIR" "$DATA_DIR"

HLVR_PINS=$'// soft pins (HL2VR-inspired)
engine_no_focus_sleep 0
snd_mute_losefocus 0
fps_max 0
'

if [[ "$MODE" == "hub" ]]; then
  cat > "$CFG_DIR/gvrmod_hub.cfg" <<CFG
// gVRMod hub — OpenXR launcher
vrmod_prefer_backend openxr
vrmod_autostart 1
vrmod_hub 1
vrmod_menu_vr 0
vrmod_require_window_focus 0
${HLVR_PINS}
echo "[gVRMod] hub cfg — full map + hub"
CFG
  EXEC_CFG="gvrmod_hub"
  MARKER_MODE="hub"
else
  cat > "$CFG_DIR/gvrmod_menu.cfg" <<CFG
// gVRMod menu-first — HL2VR-style bg map under GameUI
// default: +map_background gm_construct
vrmod_prefer_backend openxr
vrmod_autostart 1
vrmod_menu_vr 1
vrmod_hub 0
vrmod_require_window_focus 0
${HLVR_PINS}
echo "[gVRMod] menu-first — bg map + freefloat MainMenu"
CFG
  EXEC_CFG="gvrmod_menu"
  MARKER_MODE="menu"
fi

cat > "$DATA_DIR/openxr_launch.txt" <<EOF
mode=${MARKER_MODE}
prefer_backend=openxr
autostart=1
menu_vr=$([ "$MARKER_MODE" = "menu" ] && echo 1 || echo 0)
hub=$([ "$MARKER_MODE" = "hub" ] && echo 1 || echo 0)
bg_map=${MAP:-gm_construct}
map_mode=${MAP_MODE}
ts=$(date +%s)
pid=$$
EOF

append_map_args() {
  local -n arr=$1
  if [[ "$USE_MAP" != "1" || -z "$MAP" ]]; then return; fi
  if [[ "$MAP_MODE" == "background" ]]; then
    arr+=(+map_background "$MAP")
  else
    arr+=(+map "$MAP")
  fi
}

echo "[gVRMod] OpenXR launcher"
echo "  GMOD=$GMOD"
echo "  XR_RUNTIME_JSON=$XR_RUNTIME_JSON"
echo "  mode=$MODE map=$MAP map_mode=$MAP_MODE"
echo "  marker=$DATA_DIR/openxr_launch.txt"
echo "  LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-<empty>}"
echo "  SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-<default>}"
echo "  steam_rt_ssl=${STEAM_RT_X64:-MISSING — native may fail to load engine}"

if [[ ! -f "$GMOD/garrysmod/lua/bin/gmcl_vrmod_xr_linux64.dll" ]]; then
  echo "[gVRMod] WARN: gmcl_vrmod_xr_linux64.dll missing — run ./install.sh" >&2
fi

# Drop stale Source lock from prior crashes
rm -f /tmp/source_engine_*.lock 2>/dev/null || true

# ── Steam Launch Options wrapper ────────────────────────────────────────────
if [[ "$WRAPPER" == "1" || ${#EXTRA_ARGS[@]} -gt 0 ]]; then
  CMD=("${EXTRA_ARGS[@]}")
  has_exec=0
  for a in "${CMD[@]+"${CMD[@]}"}"; do
    [[ "$a" == "+exec" || "$a" == *gvrmod_* ]] && has_exec=1
  done
  [[ "$has_exec" == "0" ]] && CMD+=(+exec "$EXEC_CFG")
  append_map_args CMD
  echo "[gVRMod] exec wrapper: ${CMD[*]}"
  exec env \
    XR_RUNTIME_JSON="$XR_RUNTIME_JSON" \
    ${LD_LIBRARY_PATH:+LD_LIBRARY_PATH="$LD_LIBRARY_PATH"} \
    ${SDL_VIDEODRIVER:+SDL_VIDEODRIVER="$SDL_VIDEODRIVER"} \
    SteamAppId="$APPID" \
    SteamGameId="$APPID" \
    "${CMD[@]}"
fi

# ── Native hl2.sh ───────────────────────────────────────────────────────────
if [[ "$FORCE_STEAM" != "1" && -x "$GMOD/hl2.sh" ]]; then
  if [[ -z "$STEAM_RT_X64" ]]; then
    echo "[gVRMod] WARN: no Steam Runtime libssl.so.1.0.0 — falling back to steam -applaunch" >&2
    FORCE_STEAM=1
  else
    cd "$GMOD"
    NATIVE=(./hl2.sh -game garrysmod -novid -windowed -w 1280 -h 720 +exec "$EXEC_CFG")
    append_map_args NATIVE
    echo "[gVRMod] native: ${NATIVE[*]}"
    exec env \
      XR_RUNTIME_JSON="$XR_RUNTIME_JSON" \
      LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
      ${SDL_VIDEODRIVER:+SDL_VIDEODRIVER="$SDL_VIDEODRIVER"} \
      SteamAppId="$APPID" \
      SteamGameId="$APPID" \
      SteamOverlayGameId="$APPID" \
      "${NATIVE[@]}"
  fi
fi

# ── steam -applaunch (marker + active_runtime still drive OpenXR/VR) ────────
echo "[gVRMod] steam -applaunch (OpenXR via active_runtime.json; VR via marker)"
LAUNCH=(steam -applaunch "$APPID" -novid +exec "$EXEC_CFG")
append_map_args LAUNCH
# Steam often ignores env; XR still works via active_runtime symlink
exec "${LAUNCH[@]}"
