#!/usr/bin/env bash
# =============================================================================
# gVRMod OpenXR NATIVE launcher
#
# Does NOT use "steam -applaunch" alone (that drops XR env + often drops +exec).
# Launches GMod via hl2.sh with OpenXR runtime, WiVRn libs, and a DATA marker
# so Lua forces VR start even if +exec is late or ignored.
#
# Usage:
#   ./scripts/gvrmod_launcher.sh                 # native OpenXR + menu-first VR
#   ./scripts/gvrmod_launcher.sh --map gm_construct
#   ./scripts/gvrmod_launcher.sh --hub            # hub after map
#   ./scripts/gvrmod_launcher.sh --steam          # force steam -applaunch (legacy)
#   ./scripts/gvrmod_launcher.sh -- %command%     # Steam Launch Options wrapper
#
# Steam → Launch Options (recommended wrapper form):
#   /path/to/gVRMod/scripts/gvrmod_launcher.sh -- %command%
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
MAP=""
USE_MAP=0
MODE="menu"      # menu | hub
FORCE_STEAM=0
NO_WIVRN=0
EXTRA_ARGS=()
WRAPPER=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --map) MAP="${2:-gm_construct}"; USE_MAP=1; shift 2 ;;
    --no-map) USE_MAP=0; MAP=""; shift ;;
    --hub)
      MODE="hub"
      if [[ "$USE_MAP" == "0" && -z "$MAP" ]]; then USE_MAP=1; MAP="gm_construct"; fi
      shift
      ;;
    --menu) MODE="menu"; shift ;;
    --steam) FORCE_STEAM=1; shift ;;
    --no-wivrn) NO_WIVRN=1; shift ;;
    --gmod-dir) GMOD="$2"; shift 2 ;;
    --) shift; EXTRA_ARGS+=("$@"); WRAPPER=1; break ;;
    -h|--help) sed -n '2,22p' "$0"; exit 0 ;;
    *) EXTRA_ARGS+=("$1"); shift ;;
  esac
done

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
# Host OpenXR loader if not only inside GMod tree
[[ -d /usr/lib ]] && LIB_PATHS+=("/usr/lib")
[[ -d /usr/lib64 ]] && LIB_PATHS+=("/usr/lib64")

export LD_LIBRARY_PATH="$(IFS=:; echo "${LIB_PATHS[*]}"):${LD_LIBRARY_PATH:-}"
export SteamAppId="$APPID"
export SteamGameId="$APPID"
export SteamOverlayGameId="$APPID"
# Source needs appid next to binary cwd
echo "$APPID" > "$GMOD/steam_appid.txt" 2>/dev/null || true

# ── WiVRn server (best effort) ──────────────────────────────────────────────
if [[ "$NO_WIVRN" != "1" ]]; then
  if ! pgrep -x wivrn-server >/dev/null 2>&1; then
    if command -v wivrn-server >/dev/null 2>&1; then
      echo "[gVRMod] starting wivrn-server…"
      # non-fatal if already starting via dashboard
      wivrn-server >/tmp/gvrmod-wivrn.log 2>&1 &
      sleep 0.8
    elif systemctl --user list-unit-files wivrn-server.service &>/dev/null; then
      systemctl --user start wivrn-server.service 2>/dev/null || true
      sleep 0.5
    else
      echo "[gVRMod] WARN: wivrn-server not found — put on headset / start WiVRn dashboard"
    fi
  else
    echo "[gVRMod] wivrn-server already running"
  fi
fi

# ── cfg + DATA marker (Lua reads this even if +exec is late) ────────────────
CFG_DIR="$GMOD/garrysmod/cfg"
DATA_DIR="$GMOD/garrysmod/data/vrmod"
mkdir -p "$CFG_DIR" "$DATA_DIR"

if [[ "$MODE" == "hub" ]]; then
  cat > "$CFG_DIR/gvrmod_hub.cfg" <<'CFG'
// gVRMod hub — OpenXR native launcher
vrmod_prefer_backend openxr
vrmod_autostart 1
vrmod_hub 1
vrmod_menu_vr 0
vrmod_require_window_focus 0
echo "[gVRMod] hub cfg loaded"
CFG
  EXEC_CFG="gvrmod_hub"
  MARKER_MODE="hub"
else
  cat > "$CFG_DIR/gvrmod_menu.cfg" <<'CFG'
// gVRMod menu-first — OpenXR native launcher
vrmod_prefer_backend openxr
vrmod_autostart 1
vrmod_menu_vr 1
vrmod_hub 0
vrmod_require_window_focus 0
echo "[gVRMod] menu-first cfg loaded"
CFG
  EXEC_CFG="gvrmod_menu"
  MARKER_MODE="menu"
fi

# One-shot session flag consumed by Lua on client load
cat > "$DATA_DIR/openxr_launch.txt" <<EOF
mode=${MARKER_MODE}
prefer_backend=openxr
autostart=1
menu_vr=$([ "$MARKER_MODE" = "menu" ] && echo 1 || echo 0)
hub=$([ "$MARKER_MODE" = "hub" ] && echo 1 || echo 0)
ts=$(date +%s)
pid=$$
EOF

echo "[gVRMod] OpenXR NATIVE launcher"
echo "  GMOD=$GMOD"
echo "  XR_RUNTIME_JSON=$XR_RUNTIME_JSON"
echo "  mode=$MODE map=${MAP:-none} use_map=$USE_MAP"
echo "  marker=$DATA_DIR/openxr_launch.txt"
echo "  LD_LIBRARY_PATH (head)=${LD_LIBRARY_PATH:0:120}…"

# Verify OpenXR module present
if [[ ! -f "$GMOD/garrysmod/lua/bin/gmcl_vrmod_xr_linux64.dll" ]]; then
  echo "[gVRMod] WARN: gmcl_vrmod_xr_linux64.dll missing — run ./install.sh" >&2
fi

# ── Steam Launch Options wrapper: re-exec game binary with our env ──────────
if [[ "$WRAPPER" == "1" || ${#EXTRA_ARGS[@]} -gt 0 ]]; then
  # When Steam passes %command%, first tokens are the real game argv.
  # Prepend nothing; inject +exec and ensure env is already exported.
  CMD=("${EXTRA_ARGS[@]}")
  # Append cfg exec if not already in args
  has_exec=0
  for a in "${CMD[@]+"${CMD[@]}"}"; do
    [[ "$a" == "+exec" || "$a" == *gvrmod_* ]] && has_exec=1
  done
  if [[ "$has_exec" == "0" ]]; then
    CMD+=(+exec "$EXEC_CFG")
  fi
  if [[ "$USE_MAP" == "1" && -n "$MAP" ]]; then
    CMD+=(+map "$MAP")
  fi
  echo "[gVRMod] exec wrapper: ${CMD[*]}"
  exec env \
    XR_RUNTIME_JSON="$XR_RUNTIME_JSON" \
    LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
    SteamAppId="$APPID" \
    SteamGameId="$APPID" \
    "${CMD[@]}"
fi

# ── Direct native launch (preferred) ────────────────────────────────────────
if [[ "$FORCE_STEAM" != "1" && -x "$GMOD/hl2.sh" ]]; then
  cd "$GMOD"
  NATIVE=(./hl2.sh -game garrysmod -novid +exec "$EXEC_CFG")
  if [[ "$USE_MAP" == "1" && -n "$MAP" ]]; then
    NATIVE+=(+map "$MAP")
  fi
  # Belt-and-suspenders console commands (Lua may create convars later; marker is truth)
  NATIVE+=(+vrmod_prefer_backend openxr)
  echo "[gVRMod] native: ${NATIVE[*]}"
  exec env \
    XR_RUNTIME_JSON="$XR_RUNTIME_JSON" \
    LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
    SteamAppId="$APPID" \
    SteamGameId="$APPID" \
    "${NATIVE[@]}"
fi

# ── Fallback: steam applaunch (env may be lost — marker still saves us) ─────
echo "[gVRMod] falling back to steam -applaunch (env may not stick; marker written)"
LAUNCH=(steam -applaunch "$APPID" -novid +exec "$EXEC_CFG")
if [[ "$USE_MAP" == "1" && -n "$MAP" ]]; then
  LAUNCH+=(+map "$MAP")
fi
exec "${LAUNCH[@]}"
