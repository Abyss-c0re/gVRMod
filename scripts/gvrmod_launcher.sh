#!/usr/bin/env bash
# =============================================================================
# gVRMod OpenXR launcher  (HL2VR-inspired: bg map + auto VR)
#
# Default: steam -applaunch (correct GPU/GLX under Wayland/X11).
# Native hl2.sh is --native only (fragile; needs ssl-only path, no RT GL libs).
#
# Crash lessons:
#   • Never put garrysmod/lua/bin on LD_LIBRARY_PATH (private libc → SIGSEGV).
#   • Never put steam-runtime/usr/lib on LD_LIBRARY_PATH (old GLX → no visual).
#   • Forced SDL_VIDEODRIVER=x11 on Wayland → "Couldn't find matching GLX visual".
#   • OpenXR works via active_runtime.json even when Steam drops env.
#   • Lua force-start via data/vrmod/openxr_launch.txt marker.
#
# Usage:
#   ./scripts/gvrmod_launcher.sh                 # pull Lua from GitHub + steam launch
#   ./scripts/gvrmod_launcher.sh --no-pull       # skip git pull / rsync
#   ./scripts/gvrmod_launcher.sh --native        # hl2.sh (advanced)
#   ./scripts/gvrmod_launcher.sh --map gm_flatgrass
#   ./scripts/gvrmod_launcher.sh --background    # map_background (menu under world)
#   ./scripts/gvrmod_launcher.sh --hub
#   ./scripts/gvrmod_launcher.sh -- %command%    # Steam Launch Options wrapper
#
# Env:
#   GVMOD_LUA_REPO   default https://github.com/Abyss-c0re/vrmod-x64.git
#   GVMOD_LUA_BRANCH default main
#   GVMOD_LUA_DIR    optional override checkout path
#   GVMOD_NO_PULL=1  same as --no-pull
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

LUA_REPO="${GVMOD_LUA_REPO:-https://github.com/Abyss-c0re/vrmod-x64.git}"
LUA_BRANCH="${GVMOD_LUA_BRANCH:-main}"
LUA_CACHE="${GVMOD_LUA_DIR:-$HOME/.cache/gvrmod/vrmod-x64}"
PULL_LUA=1
[[ "${GVMOD_NO_PULL:-0}" == "1" ]] && PULL_LUA=0

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

# Only the directory that holds OpenSSL 1.0 — NOT usr/lib (breaks GLX)
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

# Stage only ssl/crypto into GMod data so we never pull RT libGL/libX11
stage_ssl_only() {
  local src="$1" dest="$2"
  mkdir -p "$dest"
  ln -sfn "$src/libssl.so.1.0.0" "$dest/libssl.so.1.0.0"
  ln -sfn "$src/libcrypto.so.1.0.0" "$dest/libcrypto.so.1.0.0"
  # common soname aliases some loaders probe
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
# Full map by default — map_background leaves desktop stuck on CEF main menu.
MAP_MODE="full"
MODE="menu"
# Tiny desktop mirror (HMD is primary)
WIN_W=720
WIN_H=480
# Default STEAM: reliable display stack. --native for hl2.sh.
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
    --hub)
      MODE="hub"; MAP_MODE="full"; USE_MAP=1; MAP="${MAP:-gm_construct}"; shift ;;
    --menu) MODE="menu"; MAP_MODE="full"; shift ;;
    --res)
      # e.g. --res 720x480
      if [[ "${2:-}" =~ ^([0-9]+)x([0-9]+)$ ]]; then
        WIN_W="${BASH_REMATCH[1]}"; WIN_H="${BASH_REMATCH[2]}"
      fi
      shift 2
      ;;
    --steam) USE_STEAM=1; shift ;;
    --native) USE_STEAM=0; shift ;;
    --no-wivrn) NO_WIVRN=1; shift ;;
    --no-pull) PULL_LUA=0; shift ;;
    --pull) PULL_LUA=1; shift ;;
    --gmod-dir) GMOD="$2"; shift 2 ;;
    --) shift; EXTRA_ARGS+=("$@"); WRAPPER=1; break ;;
    -h|--help) sed -n '2,28p' "$0"; exit 0 ;;
    *) EXTRA_ARGS+=("$1"); shift ;;
  esac
done

if [[ "$MODE" == "menu" && "$MAP_MODE" != "none" && -z "$MAP" ]]; then
  MAP="gm_construct"; USE_MAP=1; MAP_MODE="full"
fi
if [[ "$MODE" == "hub" ]]; then
  MAP_MODE="full"; MAP="${MAP:-gm_construct}"; USE_MAP=1
fi

# ── OpenXR Lua: pull GitHub sources + rsync into live GMod addon ────────────
# Prefer monorepo submodule; else cache clone of Abyss-c0re/vrmod-x64.
resolve_lua_src() {
  local sub="$ROOT/addon/vrmod-x64"
  if [[ -d "$sub/lua" && -e "$sub/.git" ]]; then
    echo "$sub"
    return 0
  fi
  if [[ -d "$LUA_CACHE/lua" ]]; then
    echo "$LUA_CACHE"
    return 0
  fi
  return 1
}

pull_and_sync_lua() {
  if [[ "$PULL_LUA" != "1" ]]; then
    echo "[gVRMod] Lua pull skipped (--no-pull / GVMOD_NO_PULL)"
    return 0
  fi
  if ! command -v git >/dev/null 2>&1; then
    echo "[gVRMod] WARN: git missing — cannot pull Lua sources" >&2
    return 0
  fi

  local src
  src="$(resolve_lua_src || true)"
  if [[ -z "$src" ]]; then
    echo "[gVRMod] cloning Lua addon → $LUA_CACHE"
    mkdir -p "$(dirname "$LUA_CACHE")"
    git clone --depth 1 --branch "$LUA_BRANCH" "$LUA_REPO" "$LUA_CACHE" \
      || git clone --depth 1 "$LUA_REPO" "$LUA_CACHE"
    src="$LUA_CACHE"
  fi

  echo "[gVRMod] OpenXR Lua sync from GitHub"
  echo "  src=$src"
  echo "  branch=$LUA_BRANCH repo=$LUA_REPO"

  # Never leave a polluted LD_LIBRARY_PATH for git (steam-runtime breaks libcurl)
  (
    export -n LD_LIBRARY_PATH 2>/dev/null || true
    unset LD_LIBRARY_PATH
    cd "$src"
    # Fetch latest
    if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
      git remote set-url origin "$LUA_REPO" 2>/dev/null || \
        git remote add origin "$LUA_REPO" 2>/dev/null || true
      git fetch --depth 1 origin "$LUA_BRANCH" 2>/dev/null \
        || git fetch origin "$LUA_BRANCH" 2>/dev/null \
        || git fetch origin 2>/dev/null || true
      # Prefer ff-only; if dirty local, still hard-reset to origin (launcher SoT = GitHub)
      local head_before head_after
      head_before="$(git rev-parse --short HEAD 2>/dev/null || echo '?')"
      if git show-ref --verify --quiet "refs/remotes/origin/$LUA_BRANCH"; then
        if ! git diff --quiet 2>/dev/null || ! git diff --cached --quiet 2>/dev/null; then
          echo "  WARN: local changes in $src — resetting to origin/$LUA_BRANCH (GitHub wins)"
        fi
        git checkout -B "$LUA_BRANCH" "origin/$LUA_BRANCH" 2>/dev/null \
          || git reset --hard "origin/$LUA_BRANCH" 2>/dev/null \
          || git pull --ff-only origin "$LUA_BRANCH" 2>/dev/null || true
      else
        git pull --ff-only 2>/dev/null || git pull 2>/dev/null || true
      fi
      head_after="$(git rev-parse --short HEAD 2>/dev/null || echo '?')"
      echo "  HEAD $head_before → $head_after ($(git log -1 --oneline 2>/dev/null || echo '?'))"
    fi
  )

  if [[ ! -d "$src/lua" ]]; then
    echo "[gVRMod] WARN: no lua/ under $src — skip rsync" >&2
    return 0
  fi

  # Sync into every known GMod addons tree (Steam often has two install paths)
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

pull_and_sync_lua

# ── OpenXR (works without process env if active_runtime is set) ─────────────
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

# ── LD_LIBRARY_PATH: minimal (native only). Never lua/bin, never RT usr/lib ─
# Clear any inherited garbage from a previous failed launch in this shell.
if [[ -n "${LD_LIBRARY_PATH:-}" ]]; then
  case ":$LD_LIBRARY_PATH:" in
    *"/garrysmod/lua/bin:"*|*"steam-runtime/usr/lib"*)
      echo "[gVRMod] stripping unsafe paths from inherited LD_LIBRARY_PATH"
      unset LD_LIBRARY_PATH
      ;;
  esac
fi

SSL_LIBDIR="$(resolve_ssl_libdir || true)"
SSL_STAGE=""
if [[ -n "$SSL_LIBDIR" ]]; then
  SSL_STAGE="$(stage_ssl_only "$SSL_LIBDIR" "$GMOD/garrysmod/data/vrmod/native_ssl")"
fi

# Native-only path: ssl stage only (OpenXR JSON finds wivrn without LD path)
NATIVE_LD=""
if [[ -n "$SSL_STAGE" ]]; then
  NATIVE_LD="$SSL_STAGE"
fi

export SteamAppId="$APPID"
export SteamGameId="$APPID"
export SteamOverlayGameId="$APPID"
echo "$APPID" > "$GMOD/steam_appid.txt" 2>/dev/null || true

# Do NOT force SDL_VIDEODRIVER=x11 — that caused "Couldn't find matching GLX visual"
# under Wayland. Let SDL/Steam pick (wayland or x11). Override: GVMOD_SDL_VIDEODRIVER=x11
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

# ── cfg + marker ────────────────────────────────────────────────────────────
CFG_DIR="$GMOD/garrysmod/cfg"
DATA_DIR="$GMOD/garrysmod/data/vrmod"
mkdir -p "$CFG_DIR" "$DATA_DIR"

HLVR_PINS=$'// soft pins (HL2VR-inspired) — never SP-pause while in VR
engine_no_focus_sleep 0
snd_mute_losefocus 0
fps_max 0
sv_pausable 0
'

if [[ "$MODE" == "hub" ]]; then
  cat > "$CFG_DIR/gvrmod_hub.cfg" <<CFG
// gVRMod hub — OpenXR launcher
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
// gVRMod launcher — full map + auto VR + hub (New Game / Settings)
// Desktop: tiny window; HMD is primary. Not map_background (that stuck CEF menu).
vrmod_prefer_backend openxr
vrmod_autostart 1
vrmod_menu_vr 1
vrmod_hub 1
vrmod_require_window_focus 0
${HLVR_PINS}
echo "[gVRMod] launcher cfg — map + auto VR + hub"
CFG
  EXEC_CFG="gvrmod_menu"
  MARKER_MODE="menu"
fi

# Also pin into autoexec so Steam dropping +exec still loads menu cfg once
if [[ ! -f "$CFG_DIR/autoexec.cfg" ]] || ! grep -q 'gvrmod_menu\|gvrmod_hub' "$CFG_DIR/autoexec.cfg" 2>/dev/null; then
  # Don't permanently force — use a one-shot companion file executed by marker only
  :
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

append_map_args() {
  local -n arr=$1
  if [[ "$USE_MAP" != "1" || -z "$MAP" ]]; then return; fi
  if [[ "$MAP_MODE" == "background" ]]; then
    arr+=(+map_background "$MAP")
  else
    arr+=(+map "$MAP")
  fi
}

# Tiny desktop window (HMD primary) — Source: -windowed -w -h
append_window_args() {
  local -n arr=$1
  arr+=(-windowed -w "$WIN_W" -h "$WIN_H")
}

echo "[gVRMod] OpenXR launcher"
echo "  GMOD=$GMOD"
echo "  XR_RUNTIME_JSON=$XR_RUNTIME_JSON"
echo "  mode=$MODE map=$MAP map_mode=$MAP_MODE"
echo "  desktop=${WIN_W}x${WIN_H} windowed"
echo "  launch=$( [[ "$USE_STEAM" == "1" ]] && echo steam || echo native )"
echo "  marker=$DATA_DIR/openxr_launch.txt"
echo "  SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-<auto>}"
echo "  ssl_stage=${SSL_STAGE:-none}"

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
  echo "[gVRMod] exec wrapper: ${CMD[*]}"
  # Wrapper: only XR env, no polluted LD_LIBRARY_PATH
  exec env -u LD_LIBRARY_PATH \
    XR_RUNTIME_JSON="$XR_RUNTIME_JSON" \
    ${SDL_VIDEODRIVER:+SDL_VIDEODRIVER="$SDL_VIDEODRIVER"} \
    SteamAppId="$APPID" \
    SteamGameId="$APPID" \
    "${CMD[@]}"
fi

# ── Default: steam -applaunch (correct GLX / Wayland / drivers) ─────────────
if [[ "$USE_STEAM" == "1" ]]; then
  if ! command -v steam >/dev/null 2>&1; then
    echo "[gVRMod] steam not in PATH — trying native" >&2
    USE_STEAM=0
  fi
fi

if [[ "$USE_STEAM" == "1" ]]; then
  echo "[gVRMod] steam -applaunch (OpenXR via active_runtime.json; VR via marker)"
  # Ensure Steam is up so applaunch doesn't hang silently
  if ! pgrep -x steam >/dev/null 2>&1 && ! pgrep -f 'steam\.sh|ubuntu12_32/steam' >/dev/null 2>&1; then
    echo "[gVRMod] starting Steam…"
    steam -silent >/tmp/gvrmod-steam.log 2>&1 &
    sleep 3
  fi
  LAUNCH=(steam -applaunch "$APPID" -novid)
  append_window_args LAUNCH
  LAUNCH+=(+exec "$EXEC_CFG")
  append_map_args LAUNCH
  echo "[gVRMod] cmd: ${LAUNCH[*]}"
  # Do not pass LD_LIBRARY_PATH into Steam (corrupts client + game)
  exec env -u LD_LIBRARY_PATH \
    XR_RUNTIME_JSON="$XR_RUNTIME_JSON" \
    "${LAUNCH[@]}"
fi

# ── Native hl2.sh (--native) ────────────────────────────────────────────────
if [[ ! -x "$GMOD/hl2.sh" ]]; then
  echo "[gVRMod] ERROR: no hl2.sh and steam path failed" >&2
  exit 1
fi

cd "$GMOD"
NATIVE=(./hl2.sh -game garrysmod -novid)
append_window_args NATIVE
NATIVE+=(+exec "$EXEC_CFG")
append_map_args NATIVE
echo "[gVRMod] native: ${NATIVE[*]}"
echo "[gVRMod] native LD_LIBRARY_PATH=${NATIVE_LD:-<empty>}"

exec env -u LD_LIBRARY_PATH \
  XR_RUNTIME_JSON="$XR_RUNTIME_JSON" \
  ${NATIVE_LD:+LD_LIBRARY_PATH="$NATIVE_LD"} \
  ${SDL_VIDEODRIVER:+SDL_VIDEODRIVER="$SDL_VIDEODRIVER"} \
  SteamAppId="$APPID" \
  SteamGameId="$APPID" \
  SteamOverlayGameId="$APPID" \
  "${NATIVE[@]}"
