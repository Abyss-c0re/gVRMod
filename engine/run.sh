#!/bin/sh
# Flat window by default. ENGINE_MODE=stereo renders two eyes into the window.
set -e
ROOT=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
LOVR="$ROOT/.cache/lovr.AppImage"
if [ ! -x "$LOVR" ]; then
	echo "missing $LOVR" >&2
	echo "fetch https://github.com/bjornbytes/lovr/releases/download/v0.18.0/lovr-v0.18.0-x86_64.AppImage" >&2
	exit 1
fi
export ENGINE_ROOT="$ROOT"
MODE=${ENGINE_MODE:-flat}
# The flag selects LÖVR's desktop simulator. It does not start WiVRn.
exec "$LOVR" --simulator "$ROOT/game" "$@"
