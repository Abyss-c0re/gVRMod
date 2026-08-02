#!/usr/bin/env bash
# =============================================================================
# gVRMod standalone launcher — OpenXR env + GMod + menu-first VR
#
# Default: true menu-first (no forced map) — freefloat real MainMenu in headset.
# Worth showing: real GameUI cinema plane, not a hub after construct.
#
# Usage:
#   ./scripts/gvrmod_launcher.sh                 # menu-first (default)
#   ./scripts/gvrmod_launcher.sh --map gm_construct
#   ./scripts/gvrmod_launcher.sh --hub            # legacy hub after map
#   ./scripts/gvrmod_launcher.sh --hub --map gm_flatgrass
#   ./scripts/gvrmod_launcher.sh -- %command%     # Steam launch options wrapper
#
# Steam → Launch Options:
#   /path/to/gVRMod/scripts/gvrmod_launcher.sh -- %command%
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

GMOD="${GMOD_DIR:-$HOME/.local/share/Steam/steamapps/common/GarrysMod}"
[[ -d "$GMOD" ]] || GMOD="$HOME/.steam/steam/steamapps/common/GarrysMod"
APPID=4000
MAP=""
USE_MAP=0
MODE="menu"   # menu | hub
EXTRA_ARGS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --map)
      MAP="${2:-gm_construct}"
      USE_MAP=1
      shift 2
      ;;
    --no-map)
      USE_MAP=0
      MAP=""
      shift
      ;;
    --hub)
      MODE="hub"
      # Hub historically needed a map so autostart + player model work
      if [[ "$USE_MAP" == "0" && -z "$MAP" ]]; then
        USE_MAP=1
        MAP="gm_construct"
      fi
      shift
      ;;
    --menu)
      MODE="menu"
      shift
      ;;
    --gmod-dir)
      GMOD="$2"
      shift 2
      ;;
    --)
      shift
      EXTRA_ARGS+=("$@")
      break
      ;;
    -h|--help)
      sed -n '2,20p' "$0"
      exit 0
      ;;
    *)
      EXTRA_ARGS+=("$1"); shift ;;
  esac
done

# ── OpenXR (WiVRn default; override with XR_RUNTIME_JSON) ───────────────────
export XR_RUNTIME_JSON="${XR_RUNTIME_JSON:-/usr/share/openxr/1/openxr_wivrn.json}"
export LD_LIBRARY_PATH="$GMOD/garrysmod/lua/bin:$GMOD/bin/linux64:${LD_LIBRARY_PATH:-}"
if [[ -d /usr/lib/wivrn ]]; then
  export LD_LIBRARY_PATH="/usr/lib/wivrn:$LD_LIBRARY_PATH"
fi
mkdir -p "$HOME/.config/openxr/1"
if [[ -f /usr/share/openxr/1/openxr_wivrn.json ]]; then
  ln -sfn /usr/share/openxr/1/openxr_wivrn.json "$HOME/.config/openxr/1/active_runtime.json" 2>/dev/null || true
fi

# ── cfg written into GMod so auto-start survives ─────────────────────────────
CFG_DIR="$GMOD/garrysmod/cfg"
mkdir -p "$CFG_DIR"

if [[ "$MODE" == "hub" ]]; then
  cat > "$CFG_DIR/gvrmod_hub.cfg" <<'CFG'
// gVRMod hub mode — written by gvrmod_launcher.sh --hub
vrmod_prefer_backend openxr
vrmod_autostart 1
vrmod_hub 1
vrmod_menu_vr 0
vrmod_require_window_focus 0
echo "[gVRMod] hub cfg — VR auto-start after map + hub surface"
CFG
  EXEC_CFG="gvrmod_hub"
else
  cat > "$CFG_DIR/gvrmod_menu.cfg" <<'CFG'
// gVRMod menu-first — freefloat real MainMenu / GameUI in VR
// written by gvrmod_launcher.sh (default)
vrmod_prefer_backend openxr
vrmod_autostart 1
vrmod_menu_vr 1
vrmod_hub 0
vrmod_require_window_focus 0
echo "[gVRMod] menu-first — VR auto-start + freefloat MainMenu cinema"
CFG
  EXEC_CFG="gvrmod_menu"
fi

echo "[gVRMod] launcher"
echo "  GMOD=$GMOD"
echo "  XR_RUNTIME_JSON=$XR_RUNTIME_JSON"
echo "  mode=$MODE map=${MAP:-none} use_map=$USE_MAP exec=+$EXEC_CFG"

# Steam wrapper path: remaining args are the game command
if [[ ${#EXTRA_ARGS[@]} -gt 0 ]]; then
  exec env \
    XR_RUNTIME_JSON="$XR_RUNTIME_JSON" \
    LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
    "${EXTRA_ARGS[@]}" +exec "$EXEC_CFG"
fi

# Direct launch via Steam
LAUNCH=(steam -applaunch "$APPID" -novid +exec "$EXEC_CFG")
if [[ "$USE_MAP" == "1" && -n "$MAP" ]]; then
  LAUNCH+=(+map "$MAP")
fi

exec "${LAUNCH[@]}"
