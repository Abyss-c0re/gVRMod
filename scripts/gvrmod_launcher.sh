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
#   ./scripts/gvrmod_launcher.sh                 # steam + bg map gm_construct
#   ./scripts/gvrmod_launcher.sh --native        # hl2.sh (advanced)
#   ./scripts/gvrmod_launcher.sh --map gm_flatgrass
#   ./scripts/gvrmod_launcher.sh --play-map
#   ./scripts/gvrmod_launcher.sh --hub
#   ./scripts/gvrmod_launcher.sh -- %command%    # Steam Launch Options wrapper
# =============================================================================
set -euo pipefail

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
MAP_MODE="background"
MODE="menu"
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
    --menu) MODE="menu"; MAP_MODE="background"; shift ;;
    --steam) USE_STEAM=1; shift ;;
    --native) USE_STEAM=0; shift ;;
    --no-wivrn) NO_WIVRN=1; shift ;;
    --gmod-dir) GMOD="$2"; shift 2 ;;
    --) shift; EXTRA_ARGS+=("$@"); WRAPPER=1; break ;;
    -h|--help) sed -n '2,22p' "$0"; exit 0 ;;
    *) EXTRA_ARGS+=("$1"); shift ;;
  esac
done

if [[ "$MODE" == "menu" && "$MAP_MODE" != "none" && -z "$MAP" ]]; then
  MAP="gm_construct"; USE_MAP=1; MAP_MODE="background"
fi
if [[ "$MODE" == "hub" ]]; then
  MAP_MODE="full"; MAP="${MAP:-gm_construct}"; USE_MAP=1
fi

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

HLVR_PINS=$'// soft pins (HL2VR-inspired)
engine_no_focus_sleep 0
snd_mute_losefocus 0
fps_max 0
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
// gVRMod menu-first — HL2VR-style bg map under GameUI
// default: +map_background gm_construct
vrmod_prefer_backend openxr
vrmod_autostart 1
vrmod_menu_vr 1
vrmod_hub 0
vrmod_require_window_focus 0
${HLVR_PINS}
echo "[gVRMod] menu-first — bg map + freefloat MainMenu"
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

echo "[gVRMod] OpenXR launcher"
echo "  GMOD=$GMOD"
echo "  XR_RUNTIME_JSON=$XR_RUNTIME_JSON"
echo "  mode=$MODE map=$MAP map_mode=$MAP_MODE"
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
  LAUNCH=(steam -applaunch "$APPID" -novid +exec "$EXEC_CFG")
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
# No fixed -w/-h (can break GLX visual). Let engine choose.
NATIVE=(./hl2.sh -game garrysmod -novid +exec "$EXEC_CFG")
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
