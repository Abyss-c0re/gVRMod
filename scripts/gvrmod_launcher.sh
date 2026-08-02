#!/usr/bin/env bash
# =============================================================================
# gVRMod OpenXR NATIVE launcher  (HL2VR-inspired)
#
# HL2VR always loads a background BSP under GameUI — never pure pre-map void.
# Default: map_background gm_construct + OpenXR + auto VR + freefloat menu.
#
# Usage:
#   ./scripts/gvrmod_launcher.sh                 # bg map gm_construct + menu VR
#   ./scripts/gvrmod_launcher.sh --map gm_flatgrass
#   ./scripts/gvrmod_launcher.sh --play-map       # full +map (not background)
#   ./scripts/gvrmod_launcher.sh --hub            # hub after full map
#   ./scripts/gvrmod_launcher.sh -- %command%     # Steam Launch Options wrapper
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

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

GMOD="$(resolve_gmod)" || {
  echo "[gVRMod] ERROR: Garry's Mod not found (need hl2.sh). Set GMOD_DIR=..." >&2
  exit 1
}

APPID=4000
# HL2VR-style: always have a world under the menu (default construct as bg)
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
    --no-wivrn) NO_WIVRN=1; shift ;;
    --gmod-dir) GMOD="$2"; shift 2 ;;
    --) shift; EXTRA_ARGS+=("$@"); WRAPPER=1; break ;;
    -h|--help) sed -n '2,18p' "$0"; exit 0 ;;
    *) EXTRA_ARGS+=("$1"); shift ;;
  esac
done

# Menu mode always wants a bg map unless user forced --no-map
if [[ "$MODE" == "menu" && "$MAP_MODE" != "none" && -z "$MAP" ]]; then
  MAP="gm_construct"
  USE_MAP=1
  MAP_MODE="background"
fi
if [[ "$MODE" == "hub" ]]; then
  MAP_MODE="full"
  MAP="${MAP:-gm_construct}"
  USE_MAP=1
fi

# ── OpenXR runtime (WiVRn default) ──────────────────────────────────────────
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

# ── Library path: module + OpenXR loader + WiVRn + engine ───────────────────
LIB_PATHS=()
[[ -d /usr/lib/wivrn ]] && LIB_PATHS+=("/usr/lib/wivrn")
[[ -d /usr/lib64/wivrn ]] && LIB_PATHS+=("/usr/lib64/wivrn")
LIB_PATHS+=("$GMOD/garrysmod/lua/bin")
LIB_PATHS+=("$GMOD/bin/linux64")
LIB_PATHS+=("$GMOD/bin")
[[ -d /usr/lib ]] && LIB_PATHS+=("/usr/lib")
[[ -d /usr/lib64 ]] && LIB_PATHS+=("/usr/lib64")

export LD_LIBRARY_PATH="$(IFS=:; echo "${LIB_PATHS[*]}"):${LD_LIBRARY_PATH:-}"
export SteamAppId="$APPID"
export SteamGameId="$APPID"
export SteamOverlayGameId="$APPID"
echo "$APPID" > "$GMOD/steam_appid.txt" 2>/dev/null || true

# ── WiVRn server (best effort) ──────────────────────────────────────────────
if [[ "$NO_WIVRN" != "1" ]]; then
  if ! pgrep -x wivrn-server >/dev/null 2>&1; then
    if command -v wivrn-server >/dev/null 2>&1; then
      echo "[gVRMod] starting wivrn-server…"
      wivrn-server >/tmp/gvrmod-wivrn.log 2>&1 &
      sleep 0.8
    elif systemctl --user list-unit-files wivrn-server.service &>/dev/null; then
      systemctl --user start wivrn-server.service 2>/dev/null || true
      sleep 0.5
    else
      echo "[gVRMod] WARN: wivrn-server not found — start WiVRn dashboard"
    fi
  else
    echo "[gVRMod] wivrn-server already running"
  fi
fi

# ── cfg + DATA marker ───────────────────────────────────────────────────────
CFG_DIR="$GMOD/garrysmod/cfg"
DATA_DIR="$GMOD/garrysmod/data/vrmod"
mkdir -p "$CFG_DIR" "$DATA_DIR"

# HL2VR-like soft pins (safe; never thrash mat_queue mid-session from here)
HLVR_PINS=$'// soft pins (HL2VR-inspired)
engine_no_focus_sleep 0
snd_mute_losefocus 0
fps_max 0
'

if [[ "$MODE" == "hub" ]]; then
  cat > "$CFG_DIR/gvrmod_hub.cfg" <<CFG
// gVRMod hub — OpenXR native launcher
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
// default: map_background gm_construct
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

# Append map args helper
append_map_args() {
  local -n arr=$1
  if [[ "$USE_MAP" != "1" || -z "$MAP" ]]; then
    return
  fi
  if [[ "$MAP_MODE" == "background" ]]; then
    # Source: map as main-menu background (HL2VR ChapterBackgrounds path)
    arr+=(+map_background "$MAP")
  else
    arr+=(+map "$MAP")
  fi
}

echo "[gVRMod] OpenXR NATIVE launcher (HL2VR-style bg map)"
echo "  GMOD=$GMOD"
echo "  XR_RUNTIME_JSON=$XR_RUNTIME_JSON"
echo "  mode=$MODE map=$MAP map_mode=$MAP_MODE"
echo "  marker=$DATA_DIR/openxr_launch.txt"

if [[ ! -f "$GMOD/garrysmod/lua/bin/gmcl_vrmod_xr_linux64.dll" ]]; then
  echo "[gVRMod] WARN: gmcl_vrmod_xr_linux64.dll missing — run ./install.sh" >&2
fi
if [[ ! -f "$GMOD/garrysmod/maps/${MAP}.bsp" && ! -f "$GMOD/garrysmod/maps/${MAP}.bsp.bz2" ]]; then
  echo "[gVRMod] WARN: maps/${MAP}.bsp not found — download map or pick another --map" >&2
fi

# ── Steam Launch Options wrapper ────────────────────────────────────────────
if [[ "$WRAPPER" == "1" || ${#EXTRA_ARGS[@]} -gt 0 ]]; then
  CMD=("${EXTRA_ARGS[@]}")
  has_exec=0
  for a in "${CMD[@]+"${CMD[@]}"}"; do
    [[ "$a" == "+exec" || "$a" == *gvrmod_* ]] && has_exec=1
  done
  if [[ "$has_exec" == "0" ]]; then
    CMD+=(+exec "$EXEC_CFG")
  fi
  append_map_args CMD
  echo "[gVRMod] exec wrapper: ${CMD[*]}"
  exec env \
    XR_RUNTIME_JSON="$XR_RUNTIME_JSON" \
    LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
    SteamAppId="$APPID" \
    SteamGameId="$APPID" \
    "${CMD[@]}"
fi

# ── Direct native launch (preferred) — HL2VR: windowed mirror ───────────────
if [[ "$FORCE_STEAM" != "1" && -x "$GMOD/hl2.sh" ]]; then
  cd "$GMOD"
  # hlvr.bat: -w 1280 -h 720 -windowed -novid
  NATIVE=(./hl2.sh -game garrysmod -novid -windowed -w 1280 -h 720 +exec "$EXEC_CFG")
  append_map_args NATIVE
  NATIVE+=(+vrmod_prefer_backend openxr)
  echo "[gVRMod] native: ${NATIVE[*]}"
  exec env \
    XR_RUNTIME_JSON="$XR_RUNTIME_JSON" \
    LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
    SteamAppId="$APPID" \
    SteamGameId="$APPID" \
    "${NATIVE[@]}"
fi

# ── Fallback: steam applaunch ───────────────────────────────────────────────
echo "[gVRMod] falling back to steam -applaunch (env may not stick; marker written)"
LAUNCH=(steam -applaunch "$APPID" -novid +exec "$EXEC_CFG")
append_map_args LAUNCH
exec "${LAUNCH[@]}"
