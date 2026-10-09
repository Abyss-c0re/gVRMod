#!/bin/sh
# Render left.png and right.png, then check disparity with OpenCV.
set -e
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
mkdir -p "$ROOT/qa/out"
export ENGINE_ROOT="$ROOT"
export ENGINE_MODE=stereo
export ENGINE_CAPTURE=1
LOVR="$ROOT/.cache/lovr.AppImage"
if [ ! -x "$LOVR" ]; then
	echo "missing $LOVR" >&2
	exit 1
fi
timeout 40 "$LOVR" --simulator "$ROOT/game"
g++ -O2 -std=c++17 -I/usr/include/opencv5 \
	"$ROOT/qa/eye_diff.cpp" -o "$ROOT/qa/out/eye_diff" \
	-lopencv_core -lopencv_imgcodecs -lopencv_imgproc
"$ROOT/qa/out/eye_diff" \
	"$ROOT/qa/out/left.png" \
	"$ROOT/qa/out/right.png" \
	"$ROOT/qa/out/stereo.txt"
