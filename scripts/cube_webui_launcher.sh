#!/usr/bin/env bash
# =============================================================================
# Cube WebUI Native VR Launcher entry
#
# Product: OpenXR app first (reversed GMod New Game WebUI).
# GMod is only spawned when you press START GAME in the headset.
# =============================================================================
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/install/native/cube_webui_launcher"

if [[ ! -x "$BIN" ]]; then
  echo "[cube_webui] building native launcher…"
  mkdir -p "$ROOT/native_launcher/build"
  cmake -S "$ROOT/native_launcher" -B "$ROOT/native_launcher/build"
  cmake --build "$ROOT/native_launcher/build" -j"$(nproc)"
fi

if [[ ! -x "$BIN" ]]; then
  echo "[cube_webui] ERROR: $BIN missing" >&2
  exit 1
fi

echo "╔════════════════════════════════════════════════════════════╗"
echo "║  Cube native WebUI launcher (OpenXR)                       ║"
echo "║  GMod starts only after START GAME in the headset          ║"
echo "║  Tabs: NEW GAME · ADDONS (manager / mount toggle)          ║"
echo "║  Host: echo start|addons|newgame|up|down|left|right|toggle ║"
echo "║        > /tmp/cube_webui_cmd                               ║"
echo "╚════════════════════════════════════════════════════════════╝"

exec "$BIN" "$@"
