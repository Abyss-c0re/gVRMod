#!/usr/bin/env bash
# =============================================================================
# gVRMod — GMod process helper (spawned AFTER native WebUI StartGame)
#
# Preferred product entry is scripts/cube_webui_launcher.sh (OpenXR first).
# This script only boots GMod with OpenXR autostart — not the menu itself.
#
# Product law:
#   Desktop is a tiny mirror. HMD is the real display after map load.
#   Stock GMod main menu / GameUI is never the product surface.
#
# What this script does (automatic):
#   1. Sync monorepo Lua → Steam addon (never hard-reset your local work)
#   2. Pin OpenXR active_runtime (WiVRn/Monado)
#   3. Start wivrn-server if needed
#   4. Write cfg + openxr_launch marker (Lua boots VR + Cube hub)
#   5. Launch GMod on construct, tiny window, +exec cube wrapper cfg
#
# Usage:
#   ./scripts/gvrmod_launcher.sh              # Cube VR wrapper (default)
#   ./scripts/gvrmod_launcher.sh --no-pull    # skip lua rsync
#   ./scripts/gvrmod_launcher.sh --pull       # optional GitHub fetch (no hard reset)
#   ./scripts/gvrmod_launcher.sh --native     # hl2.sh instead of steam
#   ./scripts/gvrmod_launcher.sh --map NAME
#   ./scripts/gvrmod_launcher.sh --res 720x480
#   ./scripts/gvrmod_launcher.sh -- %command% # Steam Launch Options wrapper
#
# Env:
#   GMOD_DIR  GVMOD_NO_PULL=1  GVMOD_SDL_VIDEODRIVER  XR_RUNTIME_JSON
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

LUA_REPO="${GVMOD_LUA_REPO:-https://github.com/Abyss-c0re/vrmod-x64.git}"
LUA_BRANCH="${GVMOD_LUA_BRANCH:-main}"
LUA_CACHE="${GVMOD_LUA_DIR:-$HOME/.cache/gvrmod/vrmod-x64}"
# Default: sync monorepo → Steam only. GitHub pull is opt-in (--pull).
PULL_LUA=0
SYNC_LUA=1
[[ "${GVMOD_NO_PULL:-0}" == "1" ]] && SYNC_LUA=0

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

resolve_ssl_libdir() {
  local base lib
  for base in \
    "$HOME/.local/share/Steam/ubuntu12_32/steam-runtime" \
    "$HOME/.steam/steam/ubuntu12_32/steam-runtime" \
    "$HOME/.steam/root/ubuntu12_32/steam-runtime" \
    "$HOME/.local/share/Steam/steamapps/common/SteamLinuxRuntime/steam-runtime"
  do
    lib="$base/lib/x86_64-linux-gnu"
    if [[ -f "$lib/libssl.so.1.0.0" && -f "$lib/libcrypto.so.1.0.0" ]]; then
      echo "$lib"
      return 0
    fi
  done
  return 1
}

stage_ssl_only() {
  local src="$1" dest="$2"
  mkdir -p "$dest"
  ln -sfn "$src/libssl.so.1.0.0" "$dest/libssl.so.1.0.0"
  ln -sfn "$src/libcrypto.so.1.0.0" "$dest/libcrypto.so.1.0.0"
  [[ -e "$src/libssl.so.1.0" ]] && ln -sfn "$src/libssl.so.1.0" "$dest/libssl.so.1.0" || true
  [[ -e "$src/libcrypto.so.1.0" ]] && ln -sfn "$src/libcrypto.so.1.0" "$dest/libcrypto.so.1.0" || true
  echo "$dest"
}

GMOD="$(resolve_gmod)" || {
  echo "[gVRMod] ERROR: Garry's Mod not found (need hl2.sh). Set GMOD_DIR=..." >&2
  exit 1
}

APPID=4000
MAP="gm_construct"
USE_MAP=1
MAP_MODE="full"
# Always Cube native launcher (hub). "menu" mode is an alias for hub.
MODE="hub"
WIN_W=720
WIN_H=480
USE_STEAM=1
NO_WIVRN=0
EXTRA_ARGS=()
WRAPPER=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --map) MAP="${2:-gm_construct}"; USE_MAP=1; shift 2 ;;
    --no-map) USE_MAP=0; MAP=""; MAP_MODE="none"; shift ;;
    --play-map|--full-map) MAP_MODE="full"; USE_MAP=1; MAP="${MAP:-gm_construct}"; shift ;;
    --background|--bg) MAP_MODE="background"; USE_MAP=1; MAP="${MAP:-gm_construct}"; shift ;;
    --hub|--cube|--launcher) MODE="hub"; MAP_MODE="full"; USE_MAP=1; MAP="${MAP:-gm_construct}"; shift ;;
    --menu) MODE="hub"; MAP_MODE="full"; shift ;; # menu = hub (no stock GameUI)
    --res)
      if [[ "${2:-}" =~ ^([0-9]+)x([0-9]+)$ ]]; then
        WIN_W="${BASH_REMATCH[1]}"; WIN_H="${BASH_REMATCH[2]}"
      fi
      shift 2
      ;;
    --steam) USE_STEAM=1; shift ;;
    --native) USE_STEAM=0; shift ;;
    --no-wivrn) NO_WIVRN=1; shift ;;
    --no-pull|--no-sync) SYNC_LUA=0; PULL_LUA=0; shift ;;
    --pull) PULL_LUA=1; SYNC_LUA=1; shift ;;
    --sync) SYNC_LUA=1; shift ;;
    --gmod-dir) GMOD="$2"; shift 2 ;;
    --) shift; EXTRA_ARGS+=("$@"); WRAPPER=1; break ;;
    -h|--help)
      cat <<'HELP'
gVRMod native VR wrapper — Cube experience

  Desktop icon / this script = OpenXR + Cube hub. No console.

  ./scripts/gvrmod_launcher.sh
  ./scripts/gvrmod_launcher.sh --map gm_flatgrass
  ./scripts/gvrmod_launcher.sh --native
  ./scripts/gvrmod_launcher.sh --no-sync
HELP
      exit 0
      ;;
    *) EXTRA_ARGS+=("$1"); shift ;;
  esac
done

MAP="${MAP:-gm_construct}"
USE_MAP=1
MAP_MODE="full"
MODE="hub"

# ── Lua: monorepo → Steam (product). GitHub hard-reset is forbidden. ────────
resolve_lua_src() {
  local sub="$ROOT/addon/vrmod-x64"
  if [[ -d "$sub/lua" ]]; then
    echo "$sub"
    return 0
  fi
  if [[ -d "$LUA_CACHE/lua" ]]; then
    echo "$LUA_CACHE"
    return 0
  fi
  return 1
}

sync_lua_to_gmod() {
  if [[ "$SYNC_LUA" != "1" ]]; then
    echo "[gVRMod] Lua sync skipped"
    return 0
  fi

  local src
  src="$(resolve_lua_src || true)"
  if [[ -z "$src" ]]; then
    if [[ "$PULL_LUA" == "1" ]] && command -v git >/dev/null 2>&1; then
      echo "[gVRMod] cloning Lua addon → $LUA_CACHE"
      mkdir -p "$(dirname "$LUA_CACHE")"
      git clone --depth 1 --branch "$LUA_BRANCH" "$LUA_REPO" "$LUA_CACHE" \
        || git clone --depth 1 "$LUA_REPO" "$LUA_CACHE"
      src="$LUA_CACHE"
    else
      echo "[gVRMod] WARN: no monorepo lua at $ROOT/addon/vrmod-x64 — run install.sh" >&2
      return 0
    fi
  fi

  # Optional soft fetch only — never reset --hard (wipes Cube work)
  if [[ "$PULL_LUA" == "1" ]] && command -v git >/dev/null 2>&1; then
    env -u LD_LIBRARY_PATH bash -c '
      set +e
      src="$1"; branch="$2"; repo="$3"
      cd "$src" || exit 0
      git rev-parse --is-inside-work-tree >/dev/null 2>&1 || exit 0
      git remote set-url origin "$repo" 2>/dev/null || true
      git fetch --depth 1 origin "$branch" 2>/dev/null || true
      if git diff --quiet 2>/dev/null && git diff --cached --quiet 2>/dev/null; then
        git merge --ff-only "origin/$branch" 2>/dev/null \
          || git pull --ff-only origin "$branch" 2>/dev/null || true
      else
        echo "  keep local changes (no hard reset) HEAD=$(git rev-parse --short HEAD 2>/dev/null)"
      fi
    ' bash "$src" "$LUA_BRANCH" "$LUA_REPO"
  fi

  if [[ ! -d "$src/lua" ]]; then
    echo "[gVRMod] WARN: no lua/ under $src" >&2
    return 0
  fi

  echo "[gVRMod] Cube Lua → GMod addons (native VR wrapper)"
  echo "  src=$src"
  local dest g
  for g in \
    "$GMOD" \
    "$HOME/.steam/steam/steamapps/common/GarrysMod" \
    "$HOME/.local/share/Steam/steamapps/common/GarrysMod"
  do
    [[ -d "$g/garrysmod/addons" ]] || continue
    dest="$g/garrysmod/addons/vrmod-x64"
    mkdir -p "$dest"
    if command -v rsync >/dev/null 2>&1; then
      rsync -a --delete \
        --exclude '.git/' \
        --exclude 'state/' \
        --exclude 'docs/' \
        --exclude '.github/' \
        --exclude '.workshop_id' \
        "$src/" "$dest/"
    else
      rm -rf "$dest"
      mkdir -p "$dest"
      cp -a "$src/." "$dest/"
      rm -rf "$dest/.git" "$dest/state" "$dest/docs" "$dest/.github" 2>/dev/null || true
    fi
    echo "  rsync → $dest"
  done
}

sync_lua_to_gmod

# ── OpenXR active runtime ───────────────────────────────────────────────────
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
  echo "[gVRMod] ERROR: no OpenXR runtime (install WiVRn or Monado)." >&2
  exit 1
fi
export XR_RUNTIME_JSON="$XR_JSON"
mkdir -p "$HOME/.config/openxr/1"
ln -sfn "$XR_RUNTIME_JSON" "$HOME/.config/openxr/1/active_runtime.json"

if [[ -n "${LD_LIBRARY_PATH:-}" ]]; then
  case ":$LD_LIBRARY_PATH:" in
    *"/garrysmod/lua/bin:"*|*"steam-runtime/usr/lib"*)
      echo "[gVRMod] stripping unsafe LD_LIBRARY_PATH"
      unset LD_LIBRARY_PATH
      ;;
  esac
fi

SSL_LIBDIR="$(resolve_ssl_libdir || true)"
SSL_STAGE=""
if [[ -n "$SSL_LIBDIR" ]]; then
  SSL_STAGE="$(stage_ssl_only "$SSL_LIBDIR" "$GMOD/garrysmod/data/vrmod/native_ssl")"
fi
NATIVE_LD=""
[[ -n "$SSL_STAGE" ]] && NATIVE_LD="$SSL_STAGE"

export SteamAppId="$APPID"
export SteamGameId="$APPID"
export SteamOverlayGameId="$APPID"
echo "$APPID" > "$GMOD/steam_appid.txt" 2>/dev/null || true

if [[ -n "${GVMOD_SDL_VIDEODRIVER:-}" ]]; then
  export SDL_VIDEODRIVER="$GVMOD_SDL_VIDEODRIVER"
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

# ── Cube wrapper cfg + launch marker (Lua auto-starts VR + hub) ─────────────
CFG_DIR="$GMOD/garrysmod/cfg"
DATA_DIR="$GMOD/garrysmod/data/vrmod"
mkdir -p "$CFG_DIR" "$DATA_DIR"

# Source engine cfg only — no shell echo lines inside
cat > "$CFG_DIR/gvrmod_cube.cfg" <<'CFG'
// gVRMod native VR wrapper — Cube experience
// HMD primary. Desktop = tiny borderless mirror. Auto OpenXR + Cube hub.
vrmod_prefer_backend openxr
vrmod_autostart 1
vrmod_hub 1
vrmod_menu_vr 0
vrmod_require_window_focus 0
vrmod_laserpointer 1
vrmod_desktopview 1
engine_no_focus_sleep 0
snd_mute_losefocus 0
fps_max 0
sv_pausable 0
sv_lan 1
// Kick VR start as soon as client cfg runs (before CreateMove)
vrmod_start force
CFG

# Back-compat names still exec'd by older markers
cp -f "$CFG_DIR/gvrmod_cube.cfg" "$CFG_DIR/gvrmod_hub.cfg"
cp -f "$CFG_DIR/gvrmod_cube.cfg" "$CFG_DIR/gvrmod_menu.cfg"
EXEC_CFG="gvrmod_cube"

# Marker: Lua OpenXR handshake — not a concommand the user types
cat > "$DATA_DIR/openxr_launch.txt" <<EOF
mode=hub
prefer_backend=openxr
autostart=1
menu_vr=0
hub=1
bg_map=${MAP}
map_mode=${MAP_MODE}
native_wrapper=1
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

append_window_args() {
  local -n arr=$1
  # Tiny borderless desktop mirror (HMD is the product). Source: -noborder.
  arr+=(-windowed -w "$WIN_W" -h "$WIN_H" -noborder)
  # Faster SP bring-up (less time stuck on Facepunch "Initializing Serverside")
  arr+=(+maxplayers 1 +sv_lan 1)
}

echo ""
echo "╔══════════════════════════════════════════════════════════╗"
echo "║  gVRMod — NATIVE VR WRAPPER (Cube)                       ║"
echo "║  Put on headset. Desktop is only a mirror.               ║"
echo "╚══════════════════════════════════════════════════════════╝"
echo "  GMOD=$GMOD"
echo "  OpenXR=$XR_RUNTIME_JSON"
echo "  map=$MAP  desktop=${WIN_W}x${WIN_H}"
echo "  marker=$DATA_DIR/openxr_launch.txt"
echo "  cfg=+exec $EXEC_CFG"
echo ""

if [[ ! -f "$GMOD/garrysmod/lua/bin/gmcl_vrmod_xr_linux64.dll" ]]; then
  echo "[gVRMod] WARN: gmcl_vrmod_xr_linux64.dll missing — run ./install.sh" >&2
fi

rm -f /tmp/source_engine_*.lock 2>/dev/null || true

# ── Steam Launch Options wrapper ────────────────────────────────────────────
if [[ "$WRAPPER" == "1" || ${#EXTRA_ARGS[@]} -gt 0 ]]; then
  CMD=("${EXTRA_ARGS[@]}")
  has_exec=0
  for a in "${CMD[@]+"${CMD[@]}"}"; do
    [[ "$a" == "+exec" || "$a" == *gvrmod_* ]] && has_exec=1
  done
  [[ "$has_exec" == "0" ]] && CMD+=(+exec "$EXEC_CFG")
  append_window_args CMD
  append_map_args CMD
  echo "[gVRMod] wrapper: ${CMD[*]}"
  exec env -u LD_LIBRARY_PATH \
    XR_RUNTIME_JSON="$XR_RUNTIME_JSON" \
    ${SDL_VIDEODRIVER:+SDL_VIDEODRIVER="$SDL_VIDEODRIVER"} \
    SteamAppId="$APPID" \
    SteamGameId="$APPID" \
    "${CMD[@]}"
fi

if [[ "$USE_STEAM" == "1" ]]; then
  if ! command -v steam >/dev/null 2>&1; then
    echo "[gVRMod] steam not in PATH — native hl2.sh" >&2
    USE_STEAM=0
  fi
fi

if [[ "$USE_STEAM" == "1" ]]; then
  if ! pgrep -x steam >/dev/null 2>&1 && ! pgrep -f 'steam\.sh|ubuntu12_32/steam' >/dev/null 2>&1; then
    echo "[gVRMod] starting Steam…"
    steam -silent >/tmp/gvrmod-steam.log 2>&1 &
    sleep 3
  fi
  LAUNCH=(steam -applaunch "$APPID" -novid)
  append_window_args LAUNCH
  LAUNCH+=(+exec "$EXEC_CFG")
  append_map_args LAUNCH
  echo "[gVRMod] ${LAUNCH[*]}"
  exec env -u LD_LIBRARY_PATH \
    XR_RUNTIME_JSON="$XR_RUNTIME_JSON" \
    "${LAUNCH[@]}"
fi

if [[ ! -x "$GMOD/hl2.sh" ]]; then
  echo "[gVRMod] ERROR: no hl2.sh" >&2
  exit 1
fi

cd "$GMOD"
NATIVE=(./hl2.sh -game garrysmod -novid)
append_window_args NATIVE
NATIVE+=(+exec "$EXEC_CFG")
append_map_args NATIVE
echo "[gVRMod] native: ${NATIVE[*]}"

exec env -u LD_LIBRARY_PATH \
  XR_RUNTIME_JSON="$XR_RUNTIME_JSON" \
  ${NATIVE_LD:+LD_LIBRARY_PATH="$NATIVE_LD"} \
  ${SDL_VIDEODRIVER:+SDL_VIDEODRIVER="$SDL_VIDEODRIVER"} \
  SteamAppId="$APPID" \
  SteamGameId="$APPID" \
  SteamOverlayGameId="$APPID" \
  "${NATIVE[@]}"
