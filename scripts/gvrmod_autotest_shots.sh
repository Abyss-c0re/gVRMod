#!/usr/bin/env bash
# gVRMod autotest with Quest adb screenshots + log scrape.
# Usage: ./scripts/gvrmod_autotest_shots.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GAME_DIR="${GMOD_DIR:-$HOME/.local/share/Steam/steamapps/common/GarrysMod}"
QUEST="${QUEST_ADB:-192.168.8.186:5555}"
SHOT_DIR="$ROOT/.scratch/autotest_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$SHOT_DIR"
echo "SHOT_DIR=$SHOT_DIR" | tee "$SHOT_DIR/meta.txt"

# Deploy current build
if [[ -f "$ROOT/install/GarrysMod/garrysmod/lua/bin/gmcl_vrmod_xr_linux64.dll" ]]; then
  cp -f "$ROOT/install/GarrysMod/garrysmod/lua/bin/gmcl_vrmod_xr_linux64.dll" \
    "$GAME_DIR/garrysmod/lua/bin/"
fi
if [[ -d "$ROOT/addon/vrmod-x64/lua" ]]; then
  rsync -a --exclude '.git' --exclude 'state' --exclude 'docs' \
    "$ROOT/addon/vrmod-x64/" "$GAME_DIR/garrysmod/addons/vrmod-x64/"
fi

mkdir -p "$GAME_DIR/garrysmod/cfg"
cat > "$GAME_DIR/garrysmod/cfg/vrmod_autotest.cfg" <<'CFG'
mat_queue_mode 1
vrmod_prefer_backend openxr
vrmod_desktopview 1
vrmod_autoshot 1
vrmod_autoshot_interval 2
vrmod_require_window_focus 0
engine_no_focus_sleep 0
gmod_mcore_test 0
vrmod_autostart 1
vrmod_start force
CFG

# Kill gmod by exact name only
for pid in $(pgrep -x gmod 2>/dev/null || true); do kill -TERM "$pid" 2>/dev/null || true; done
sleep 1
for pid in $(pgrep -x gmod 2>/dev/null || true); do kill -9 "$pid" 2>/dev/null || true; done
sleep 0.5

: > "$GAME_DIR/vrmod_debug.log" || true
: > "$GAME_DIR/garrysmod/console.log" || true

export XR_RUNTIME_JSON="${XR_RUNTIME_JSON:-/usr/share/openxr/1/openxr_wivrn.json}"
steam -applaunch 4000 -console -condebug +con_logfile "console.log" \
  -novid -windowed -w 1280 -h 720 \
  +map gm_construct +exec vrmod_autotest.cfg +con_enable 1 +sv_cheats 1

GMOD_PID=""
for i in $(seq 1 70); do
  GMOD_PID=$(pgrep -x gmod | head -1 || true)
  if [[ -n "$GMOD_PID" ]]; then
    echo "gmod=$GMOD_PID at ${i}s" | tee -a "$SHOT_DIR/meta.txt"
    break
  fi
  sleep 1
done
[[ -n "$GMOD_PID" ]] || { echo FAIL_NO_GMOD | tee -a "$SHOT_DIR/meta.txt"; exit 1; }

for i in $(seq 1 80); do
  if grep -qE 'SBS-only|session begun|Submit SBS|legacy SBS' "$GAME_DIR/vrmod_debug.log" 2>/dev/null; then
    echo "VR_OK at ${i}s" | tee -a "$SHOT_DIR/meta.txt"
    break
  fi
  sleep 1
done

if command -v xdotool >/dev/null 2>&1; then
  WID=$(xdotool search --name 'Garry' 2>/dev/null | head -1 || true)
  if [[ -n "$WID" ]]; then
    xdotool windowactivate --sync "$WID" 2>/dev/null || true
    sleep 0.3
    xdotool key --window "$WID" grave 2>/dev/null || true
    sleep 0.2
    xdotool type --delay 10 --window "$WID" 'vrmod_start force; vrmod_autoshot 1; jpeg' 2>/dev/null || true
    xdotool key --window "$WID" Return 2>/dev/null || true
    xdotool key --window "$WID" grave 2>/dev/null || true
    echo "console inject ok" | tee -a "$SHOT_DIR/meta.txt"
  fi
fi

for n in 1 2 3 4 5 6 7 8; do
  sleep 3
  adb -s "$QUEST" shell screencap -p /sdcard/gvrmod_auto.png >/dev/null 2>&1 || true
  adb -s "$QUEST" pull /sdcard/gvrmod_auto.png "$SHOT_DIR/quest_${n}.png" >/dev/null 2>&1 || true
  if [[ -d "$GAME_DIR/garrysmod/screenshots" ]]; then
    find "$GAME_DIR/garrysmod/screenshots" -name '*.jpg' -mmin -8 -exec cp -n {} "$SHOT_DIR/" \; 2>/dev/null || true
  fi
  echo "round $n files=$(ls "$SHOT_DIR" 2>/dev/null | wc -l)"
done

grep -E 'SBS-only|PER-EYE|BLIT eye|shouldRender|WARN|legacy SBS' \
  "$GAME_DIR/vrmod_debug.log" 2>/dev/null | tail -40 > "$SHOT_DIR/log_tail.txt" || true

# Score non-black fraction on quest shots
python3 - <<PY | tee -a "$SHOT_DIR/meta.txt"
from pathlib import Path
try:
    from PIL import Image
    import numpy as np
except Exception as e:
    print("score_skip", e)
    raise SystemExit(0)
d = Path("$SHOT_DIR")
scores = []
for p in sorted(d.glob("quest_*.png")):
    im = np.array(Image.open(p).convert("RGB"))
    non = float((im.max(axis=2) > 12).mean())
    scores.append(non)
    print(f"score {p.name} nonblack={non:.3f}")
if scores:
    m = sum(scores) / len(scores)
    print(f"score_mean={m:.3f} PASS={m >= 0.25}")
PY

echo "DONE=$SHOT_DIR" | tee -a "$SHOT_DIR/meta.txt"
ls -la "$SHOT_DIR"
