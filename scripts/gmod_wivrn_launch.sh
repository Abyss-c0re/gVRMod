#!/bin/bash
# Launch Garry's Mod with WiVRn OpenXR so gVRMod can see the Quest.
set -euo pipefail
export XR_RUNTIME_JSON="${XR_RUNTIME_JSON:-/usr/share/openxr/1/openxr_wivrn.json}"
GMOD="${GMOD_DIR:-$HOME/.local/share/Steam/steamapps/common/GarrysMod}"
export LD_LIBRARY_PATH="$GMOD/garrysmod/lua/bin:$GMOD/bin/linux64:${LD_LIBRARY_PATH:-}"
# Steam Runtime sometimes hides host /usr/lib/wivrn — make it explicit if present
if [[ -d /usr/lib/wivrn ]]; then
  export LD_LIBRARY_PATH="/usr/lib/wivrn:$LD_LIBRARY_PATH"
fi
# Ensure user active runtime (WiVRn)
mkdir -p "$HOME/.config/openxr/1"
if [[ ! -e "$HOME/.config/openxr/1/active_runtime.json" ]]; then
  ln -sfn /usr/share/openxr/1/openxr_wivrn.json "$HOME/.config/openxr/1/active_runtime.json"
fi
echo "[gVRMod] XR_RUNTIME_JSON=$XR_RUNTIME_JSON"
echo "[gVRMod] LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
# If invoked as Steam %command% wrapper: remaining args are the game command
if [[ $# -gt 0 ]]; then
  exec "$@"
fi
# Direct native launch (no Proton)
exec steam -applaunch 4000
