#!/usr/bin/env bash
# =============================================================================
# Cube WebUI Native VR Launcher entry
#
# Product: OpenXR app first (reversed GMod New Game WebUI).
# GMod is only spawned when you press START GAME in the headset.
#
# Compile policy (desktop launch):
#   - Skip build when install/native/cube_webui_launcher is present and
#     newer/equal than all native sources (default — not every launch).
#   - Rebuild only when binary missing or sources newer (real update).
#   - GVMOD_FORCE_BUILD=1 / CUBE_FORCE_BUILD=1 → always rebuild
#   - GVMOD_NO_BUILD=1 / CUBE_NO_BUILD=1       → never rebuild (fail if no binary)
#   - GVMOD_NO_SYNC=1                          → skip monorepo pull
# =============================================================================
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/install/native/cube_webui_launcher"
BUILD_DIR="$ROOT/native_launcher/build"
SRC_DIR="$ROOT/native_launcher"
SHARED_OX="$ROOT/shared/openxr"

# Keep monorepo + addon tracking remotes (never hard-reset). Opt out: GVMOD_NO_SYNC=1
if [[ "${GVMOD_NO_SYNC:-0}" != "1" && -x "$ROOT/scripts/sync_repos.sh" ]]; then
  # pull-only is cheap; does not compile Cube
  env -u LD_LIBRARY_PATH "$ROOT/scripts/sync_repos.sh" --pull-only 2>/dev/null || true
fi

force_build=0
no_build=0
[[ "${GVMOD_FORCE_BUILD:-0}" == "1" || "${CUBE_FORCE_BUILD:-0}" == "1" ]] && force_build=1
[[ "${GVMOD_NO_BUILD:-0}" == "1" || "${CUBE_NO_BUILD:-0}" == "1" ]] && no_build=1

SHARED_CUBE_UI="$ROOT/shared/cube_ui"

# Newest mtime (integer seconds) among Cube native inputs that affect the binary.
# Only these trees — never rebuild because monorepo pull touched unrelated files.
newest_src_mtime() {
  local newest=0 t
  while IFS= read -r t; do
    [[ -z "$t" ]] && continue
    t=${t%%.*}
    [[ "$t" =~ ^[0-9]+$ ]] || continue
    (( t > newest )) && newest=$t
  done < <(
    find "$SRC_DIR/src" -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) -printf '%T@\n' 2>/dev/null
    [[ -f "$SRC_DIR/CMakeLists.txt" ]] && stat -c '%Y' "$SRC_DIR/CMakeLists.txt"
    if [[ -d "$SHARED_OX" ]]; then
      find "$SHARED_OX" -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) -printf '%T@\n' 2>/dev/null
    fi
    if [[ -d "$SHARED_CUBE_UI" ]]; then
      find "$SHARED_CUBE_UI" -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) -printf '%T@\n' 2>/dev/null
    fi
  )
  echo "$newest"
}

need_build=0
if [[ "$force_build" == "1" ]]; then
  need_build=1
  echo "[cube_webui] force build requested (GVMOD_FORCE_BUILD/CUBE_FORCE_BUILD)"
elif [[ "$no_build" == "1" ]]; then
  need_build=0
elif [[ ! -x "$BIN" ]]; then
  need_build=1
  echo "[cube_webui] binary missing — building once"
else
  bin_mtime=$(stat -c '%Y' "$BIN" 2>/dev/null || echo 0)
  src_mtime=$(newest_src_mtime)
  if (( src_mtime > bin_mtime )); then
    need_build=1
    echo "[cube_webui] sources newer than binary — rebuilding (src=$src_mtime bin=$bin_mtime)"
  else
    echo "[cube_webui] binary up to date — skip compile (no source change)"
  fi
fi

if [[ "$need_build" == "1" ]]; then
  if [[ "$no_build" == "1" ]]; then
    echo "[cube_webui] ERROR: GVMOD_NO_BUILD set but binary needs build: $BIN" >&2
    exit 1
  fi
  echo "[cube_webui] building native launcher → $BIN"
  mkdir -p "$BUILD_DIR" "$ROOT/install/native"
  # Reconfigure only when cache missing or CMakeLists newer than cache
  need_cmake=0
  if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    need_cmake=1
  else
    cl_m=$(stat -c '%Y' "$SRC_DIR/CMakeLists.txt" 2>/dev/null || echo 0)
    cc_m=$(stat -c '%Y' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null || echo 0)
    (( cl_m > cc_m )) && need_cmake=1
  fi
  if [[ "$need_cmake" == "1" ]]; then
    cmake -S "$SRC_DIR" -B "$BUILD_DIR"
  fi
  # Incremental: ninja/make no-ops object files that did not change
  cmake --build "$BUILD_DIR" -j"$(nproc)" --target cube_webui_launcher
fi

if [[ ! -x "$BIN" ]]; then
  echo "[cube_webui] ERROR: $BIN missing after build" >&2
  exit 1
fi

# Ensure project defaults sit next to the binary (copy only if different / missing)
if [[ -f "$SRC_DIR/cube_webui.conf" ]]; then
  mkdir -p "$ROOT/install/native"
  if [[ ! -f "$ROOT/install/native/cube_webui.conf" ]] || \
     ! cmp -s "$SRC_DIR/cube_webui.conf" "$ROOT/install/native/cube_webui.conf" 2>/dev/null; then
    cp "$SRC_DIR/cube_webui.conf" "$ROOT/install/native/cube_webui.conf"
  fi
fi

echo "╔════════════════════════════════════════════════════════════╗"
echo "║  Cube native WebUI launcher (OpenXR)                       ║"
echo "║  TRIGGER click (no hover) · GRIP move · MENU reset pose        ║"
echo "║  passthrough + flexible panel (cube_webui.conf)            ║"
echo "║  Host: echo start|reset|click|addons >/tmp/cube_webui_cmd  ║"
echo "╚════════════════════════════════════════════════════════════╝"

exec "$BIN" "$@"
