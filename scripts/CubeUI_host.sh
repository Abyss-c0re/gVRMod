#!/usr/bin/env bash
# =============================================================================
# CubeUI host launch (from GMod return bridge / desktop)
#
# - Never rebuilds (use CubeUI.sh for dev builds)
# - Strips Steam pressure-vessel library paths
# - Waits briefly for OpenXR to free after GMod VR exit
# - Always exits 0 after starting so KDE does not show "crashed"
# =============================================================================
set +e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${GVRMOD_CUBE_BIN:-$ROOT/install/native/CubeUI}"
LOG="${CUBEUI_HOST_LOG:-/tmp/CubeUI_return.log}"

{
  echo "[cube-host] $(date -Iseconds) start bin=$BIN"
  echo "[cube-host] DISPLAY=${DISPLAY:-} XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-} XR=${XR_RUNTIME_JSON:-}"
} >>"$LOG" 2>&1

if [[ ! -x "$BIN" ]]; then
  echo "[cube-host] ERROR: missing binary $BIN" >>"$LOG"
  # Still exit 0 so desktop doesn't spam "crashed" — user sees log
  exit 0
fi

# Clean Steam / game runtime leakage (breaks host GLX + OpenXR)
unset LD_LIBRARY_PATH
unset LD_PRELOAD
unset STEAM_RUNTIME
unset STEAM_RUNTIME_LIBRARY_PATH
unset PRESSURE_VESSEL_FILESYSTEMS_RW
export DISPLAY="${DISPLAY:-:0}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
if [[ -z "${XR_RUNTIME_JSON:-}" ]]; then
  for c in /usr/share/openxr/1/openxr_wivrn.json /usr/local/share/openxr/1/openxr_wivrn.json \
           /usr/share/openxr/1/openxr_monado.json; do
    if [[ -r "$c" ]]; then export XR_RUNTIME_JSON="$c"; break; fi
  done
fi

# If another CubeUI is already up, do nothing
if pgrep -x CubeUI >/dev/null 2>&1; then
  echo "[cube-host] CubeUI already running — ok" >>"$LOG"
  exit 0
fi

# Always wait a few seconds after VR exit so OpenXR is free.
# Temp return keeps GMod (hl2) running — do NOT wait for process death.
echo "[cube-host] waiting 3s for OpenXR drain after GMod VR exit..." >>"$LOG"
sleep 3

cd "$ROOT" || true
# Run in foreground of this process group when launched by setsid/nohup from GMod
exec "$BIN" >>"$LOG" 2>&1
