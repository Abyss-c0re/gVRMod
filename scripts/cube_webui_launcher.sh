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

# Keep monorepo + addon tracking remotes (never hard-reset). Opt out: GVMOD_NO_SYNC=1
if [[ "${GVMOD_NO_SYNC:-0}" != "1" && -x "$ROOT/scripts/sync_repos.sh" ]]; then
  # status-only is fast; full push on every launch is noisy — fetch/ff when clean
  env -u LD_LIBRARY_PATH "$ROOT/scripts/sync_repos.sh" --pull-only 2>/dev/null || true
fi

need_build=0
if [[ ! -x "$BIN" ]]; then
  need_build=1
else
  # Rebuild when any native source is newer than the installed binary
  newest_src=$(find "$ROOT/native_launcher/src" -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) -printf '%T@\n' 2>/dev/null | sort -n | tail -1)
  bin_mtime=$(stat -c '%Y' "$BIN" 2>/dev/null || echo 0)
  if [[ -n "${newest_src:-}" ]]; then
    src_i=${newest_src%.*}
    if (( src_i > bin_mtime )); then
      need_build=1
      echo "[cube_webui] sources newer than binary — rebuilding"
    fi
  fi
fi

if [[ "$need_build" == "1" ]]; then
  echo "[cube_webui] building native launcher → $BIN"
  mkdir -p "$ROOT/native_launcher/build" "$ROOT/install/native"
  cmake -S "$ROOT/native_launcher" -B "$ROOT/native_launcher/build"
  cmake --build "$ROOT/native_launcher/build" -j"$(nproc)"
fi

if [[ ! -x "$BIN" ]]; then
  echo "[cube_webui] ERROR: $BIN missing after build" >&2
  exit 1
fi

# Ensure project defaults sit next to the binary
if [[ -f "$ROOT/native_launcher/cube_webui.conf" ]]; then
  mkdir -p "$ROOT/install/native"
  cp -n "$ROOT/native_launcher/cube_webui.conf" "$ROOT/install/native/cube_webui.conf" 2>/dev/null || \
    cp "$ROOT/native_launcher/cube_webui.conf" "$ROOT/install/native/cube_webui.conf"
fi

echo "╔════════════════════════════════════════════════════════════╗"
echo "║  Cube native WebUI launcher (OpenXR)                       ║"
echo "║  TRIGGER click · GRIP move menu · MENU reset pose          ║"
echo "║  passthrough + flexible panel (cube_webui.conf)            ║"
echo "║  Host: echo start|reset|click|addons >/tmp/cube_webui_cmd  ║"
echo "╚════════════════════════════════════════════════════════════╝"

exec "$BIN" "$@"
