#!/usr/bin/env bash
# CSSVRMod host — find CSS, LD_PRELOAD hook, keep Steam runtime out of the host.
set +e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="${CSSVR_BIN:-$ROOT/install/cssvrmod/CSSVR}"
HOOK="${CSSVR_HOOK:-$ROOT/install/cssvrmod/libcssvrmod_hook.so}"
export CSSVR_ROOT="$ROOT"
export CSSVR_HOOK="$HOOK"

unset LD_LIBRARY_PATH
unset STEAM_RUNTIME
unset STEAM_RUNTIME_LIBRARY_PATH
export DISPLAY="${DISPLAY:-:0}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
# togl is GLX. Host sdl2-compat + Wayland is what killed -dx9 here.
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11}"
FAKE="$ROOT/.scratch/cssvrmod/fakelibs"
if [[ -d "$FAKE" ]]; then export CSSVR_LIBDIR="${CSSVR_LIBDIR:-$FAKE}"; fi
if [[ -z "${XR_RUNTIME_JSON:-}" ]]; then
  for c in /usr/share/openxr/1/openxr_wivrn.json /usr/local/share/openxr/1/openxr_wivrn.json \
           /usr/share/openxr/1/openxr_monado.json; do
    if [[ -r "$c" ]]; then export XR_RUNTIME_JSON="$c"; break; fi
  done
fi

if [[ ! -x "$BIN" ]]; then
  echo "cssvr: missing $BIN — cmake --build cssvrmod/build --target CSSVR" >&2
  exit 1
fi
exec "$BIN" "$@"
