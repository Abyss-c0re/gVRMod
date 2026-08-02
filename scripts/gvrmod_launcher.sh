#!/usr/bin/env bash
# =============================================================================
# gVRMod standalone launcher — OpenXR env + GMod + auto VR hub
#
# Usage:
#   ./scripts/gvrmod_launcher.sh                 # default: gm_construct + hub
#   ./scripts/gvrmod_launcher.sh --map gm_flatgrass
#   ./scripts/gvrmod_launcher.sh --no-map         # desktop main menu (no auto map)
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
MAP="gm_construct"
USE_MAP=1
EXTRA_ARGS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --map) MAP="${2:-gm_construct}"; shift 2 ;;
    --no-map) USE_MAP=0; shift ;;
    --gmod-dir) GMOD="$2"; shift 2 ;;
    --) shift; EXTRA_ARGS+=("$@"); break ;;
    -h|--help)
      sed -n '2,16p' "$0"
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
cat > "$CFG_DIR/gvrmod_hub.cfg" <<'CFG'
// gVRMod hub mode — written by gvrmod_launcher.sh
vrmod_prefer_backend openxr
vrmod_autostart 1
vrmod_hub 1
vrmod_require_window_focus 0
echo "[gVRMod] hub cfg loaded — VR will auto-start after map"
CFG

echo "[gVRMod] launcher"
echo "  GMOD=$GMOD"
echo "  XR_RUNTIME_JSON=$XR_RUNTIME_JSON"
echo "  map=$MAP (use_map=$USE_MAP)"

# Steam wrapper path: remaining args are the game command
if [[ ${#EXTRA_ARGS[@]} -gt 0 ]]; then
  # Inject +exec if not already present
  exec env \
    XR_RUNTIME_JSON="$XR_RUNTIME_JSON" \
    LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
    "${EXTRA_ARGS[@]}" +exec gvrmod_hub
fi

# Direct launch via Steam
LAUNCH=(steam -applaunch "$APPID" -novid +exec gvrmod_hub)
if [[ "$USE_MAP" == "1" ]]; then
  LAUNCH+=(+map "$MAP")
fi

exec "${LAUNCH[@]}"
