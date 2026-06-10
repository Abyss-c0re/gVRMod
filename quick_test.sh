#!/bin/bash
set -euo pipefail

# Tools needed for reliable auto-focus and sending console commands to the game window.
# Without them, the game window must be manually focused, and vrmod_start may not run automatically.
if ! command -v xdotool >/dev/null 2>&1 || ! command -v wmctrl >/dev/null 2>&1; then
    echo "ERROR: Required tools for auto window focus / command injection are missing."
    echo "Please install them:"
    echo "  Debian/Ubuntu:  sudo apt install xdotool wmctrl"
    echo "  Arch:           sudo pacman -S xdotool wmctrl"
    echo "  Fedora:         sudo dnf install xdotool wmctrl"
    echo ""
    echo "After installing, re-run this script."
    exit 1
fi

echo "=== gVRMod Quick Test Cycle ==="
echo "Building..."
./build.sh

GAME_DIR="$HOME/.local/share/Steam/steamapps/common/GarrysMod"
LIVE_BIN="$GAME_DIR/garrysmod/lua/bin/gmcl_vrmod_linux64.dll"
LIVE_CFG_DIR="$GAME_DIR/garrysmod/cfg"
DEV_DLL="install/GarrysMod/garrysmod/lua/bin/gmcl_vrmod_linux64.dll"

# Clean logs on each start (as requested). Truncate module debug log and engine console.log
# so each quick test cycle starts with a fresh, easy-to-read log.
echo "Cleaning logs for this test run (vrmod_debug.log + console.log)..."
> "$GAME_DIR/vrmod_debug.log" 2>/dev/null || true
> "$GAME_DIR/garrysmod/console.log" 2>/dev/null || true
echo "Logs cleaned."

if [[ ! -f "$DEV_DLL" ]]; then
  echo "ERROR: $DEV_DLL not found after build"
  exit 1
fi

# Backup previous dll with timestamp
if [[ -f "$LIVE_BIN" ]]; then
  TS=$(date +%Y%m%d-%H%M%S)
  cp -f "$LIVE_BIN" "${LIVE_BIN}.bak-${TS}"
  echo "Backed up previous dll to ${LIVE_BIN}.bak-${TS}"
fi

echo "Running proper installer (this bundles the OpenXR loader + all its runtime deps into bin/linux64/, which is required for the Steam Linux Runtime container)."
./install.sh --skip-build --yes
echo "Install complete (module + OpenXR bundle deployed)."

# Write a minimal test cfg.
# We only set desktopview here. We send "vrmod_start" via xdotool *after* we have focused the window.
mkdir -p "$LIVE_CFG_DIR"
cat > "$LIVE_CFG_DIR/vrmod_quicktest.cfg" << 'CFG'
vrmod_desktopview 2
echo "=== VRMOD_QUICKTEST: desktopview set, waiting for focused vrmod_start ==="
CFG
echo "Wrote $LIVE_CFG_DIR/vrmod_quicktest.cfg"

echo "Killing any running GMod..."
# Modern Steam Linux Runtime launches GMod as bin/linux64/gmod (inside pressure-vessel),
# not as hl2_linux. Match on the actual binary and launch wrappers.
pkill -f 'bin/linux64/gmod' 2>/dev/null || true
pkill -f 'GarrysMod/hl2.sh' 2>/dev/null || true
pkill -9 -f 'bin/linux64/gmod' 2>/dev/null || true
sleep 0.8

echo "Launching Garry's Mod -> gm_construct with auto vrmod_start..."
# -console +map +exec our test cfg. 
# -condebug +con_logfile are required for console.log to be written (used by the map-load wait below).
# Without them the wait loop often never sees "Map: ..." and the script appears stuck before it can open the console and send vrmod_start.
steam -applaunch 4000 -console -condebug +con_logfile "console.log" +map gm_construct +exec vrmod_quicktest.cfg +con_enable 1 +sv_cheats 1 &
LAUNCH_BG_PID=$!

echo ""
echo "Game launching in background."

# Capture the actual game process PID(s). On current Steam Linux Runtime this is
# the 'gmod' binary (bin/linux64/gmod), launched via hl2.sh + reaper + pressure-vessel.
# We poll briefly because the containerized processes take a moment to appear.
echo "Capturing GMod game PID(s)..."
GAME_PIDS=""
for _ in $(seq 1 20); do
    P=$(pgrep -f 'bin/linux64/gmod' 2>/dev/null | head -1 || true)
    if [ -z "$P" ]; then
        P=$(pgrep -f 'GarrysMod/hl2.sh' 2>/dev/null | head -1 || true)
    fi
    if [ -n "$P" ]; then
        GAME_PIDS="$P"
        break
    fi
    sleep 0.4
done
if [ -n "$GAME_PIDS" ]; then
    echo "Captured GMod PID(s): $GAME_PIDS"
else
    echo "WARNING: Could not capture GMod PID yet (will rely on pkill -f later)."
fi
echo ""
echo ">>> IMPORTANT FOR AUTO TEST: The GMod window MUST be focused <<<"
echo "    Unfocused Source engine windows often skip full ticks, pause Lua timers,"
echo "    or don't properly process vrmod_start / rendering."
echo ""

# Wait for the map to load before key injection.
# We now use two signals:
#   1. The string "gm_construct" appearing in console.log (enabled by -condebug +con_logfile above)
#   2. A visible "Garry's Mod" window (very reliable once the map is far enough along)
# This prevents the script from appearing stuck at the wait message and never reaching the console open + vrmod_start part.
echo "Waiting for map to load (console.log or game window)..."
for i in $(seq 1 60); do
  loaded=0
  if [ -f "$GAME_DIR/garrysmod/console.log" ] && grep -qi "gm_construct" "$GAME_DIR/garrysmod/console.log" 2>/dev/null; then
    loaded=1
  fi
  # Window visible is a strong secondary signal that we're past the main menu / into the map.
  if [ "$loaded" -eq 0 ]; then
    if xdotool search --onlyvisible --name "Garry" 2>/dev/null | grep -q . || \
       xdotool search --onlyvisible --class "gmod" 2>/dev/null | grep -q . || \
       wmctrl -l 2>/dev/null | grep -qi 'garry'; then
      loaded=1
    fi
  fi
  if [ "$loaded" -eq 1 ]; then
    echo "Map appears loaded (or game window is up)"
    break
  fi
  if [ $(( i % 10 )) -eq 0 ]; then
    echo "  ... still waiting for map/window ($i/60) ..."
  fi
  sleep 1
done
if ! [ -f "$GAME_DIR/garrysmod/console.log" ] || ! grep -qi "gm_construct" "$GAME_DIR/garrysmod/console.log" 2>/dev/null; then
  echo "Timeout waiting for exact map string in log (proceeding anyway - window focus + key injection will still be attempted)."
fi

# Give Steam/GMod time to actually create the visible window before we search
sleep 6

echo "Trying to auto-focus the GMod window (xdotool + wmctrl fallback)..."

FOCUSED=0
for i in $(seq 1 90); do
    # Multiple search strategies (name, class, pid of the game process).
    # Note: on modern Steam Linux Runtime the binary is bin/linux64/gmod (via hl2.sh), not hl2_linux.
    WIN=$(xdotool search --onlyvisible --name "Garry" 2>/dev/null | head -1 || true)
    if [ -z "$WIN" ]; then
        WIN=$(xdotool search --onlyvisible --class "hl2_linux" 2>/dev/null | head -1 || true)
        if [ -z "$WIN" ]; then
            WIN=$(xdotool search --onlyvisible --class "gmod" 2>/dev/null | head -1 || true)
        fi
    fi
    if [ -z "$WIN" ]; then
        PID=$(pgrep -f 'bin/linux64/gmod\|GarrysMod/hl2.sh' 2>/dev/null | head -1 || true)
        if [ -n "$PID" ]; then
            WIN=$(xdotool search --onlyvisible --pid "$PID" 2>/dev/null | head -1 || true)
        fi
    fi
    if [ -z "$WIN" ]; then
        # wmctrl fallback (often works better with Steam runtime windows)
        WIN=$(wmctrl -l 2>/dev/null | grep -iE 'garry|hl2|source' | head -1 | awk '{print $1}' || true)
    fi

    if [ -n "$WIN" ]; then
        xdotool windowactivate --sync "$WIN" 2>/dev/null || true
        xdotool windowraise "$WIN" 2>/dev/null || true
        xdotool windowfocus --sync "$WIN" 2>/dev/null || true
        if [[ "$WIN" == 0x* ]]; then
            wmctrl -i -a "$WIN" 2>/dev/null || true
        fi
        echo "Auto-focused GMod window (ID: $WIN)"
        FOCUSED=1
        break
    fi
    sleep 0.5
done

if [ "$FOCUSED" -eq 0 ]; then
    echo ""
    echo ">>> ACTION REQUIRED <<<"
    echo "Could not auto-focus GMod window (xdotool/wmctrl often can't see Steam container windows)."
    echo "Please manually click/focus the GMod window NOW (within the next ~12 seconds)"
    echo "so we can send the vrmod_start command while the window is focused."
    echo ""
    echo "Tip: sudo apt install xdotool wmctrl   (or equivalent) can help in future runs."
    sleep 12

    # Re-attempt to find the window after user was asked to focus/click it.
    WIN=""
    for i in $(seq 1 10); do
        WIN=$(xdotool search --onlyvisible --name "Garry" 2>/dev/null | head -1 || true)
        if [ -z "$WIN" ]; then
            WIN=$(xdotool search --onlyvisible --class "hl2_linux" 2>/dev/null | head -1 || true)
        fi
        if [ -z "$WIN" ]; then
            WIN=$(xdotool search --onlyvisible --class "gmod" 2>/dev/null | head -1 || true)
        fi
        if [ -z "$WIN" ]; then
            PID=$(pgrep -f 'bin/linux64/gmod\|GarrysMod/hl2.sh' 2>/dev/null | head -1 || true)
            if [ -n "$PID" ]; then
                WIN=$(xdotool search --onlyvisible --pid "$PID" 2>/dev/null | head -1 || true)
            fi
        fi
        if [ -z "$WIN" ]; then
            WIN=$(wmctrl -l 2>/dev/null | grep -iE 'garry|hl2|source' | head -1 | awk '{print $1}' || true)
        fi
        if [ -n "$WIN" ]; then
            echo "Detected focused window after manual action (ID: $WIN)"
            break
        fi
        sleep 0.5
    done
fi

# Re-detect a fresh, currently valid WIN right before the key injection.
# This avoids using a stale window ID (which often triggers BadWindow during xdotool send
# on Steam container windows) and greatly improves reliability of the direct-grave console open
# without adding any Esc key before the ~ (per your constraint).
WIN=""
for _ in $(seq 1 8); do
    WIN=$(xdotool search --onlyvisible --name "Garry" 2>/dev/null | head -1 || true)
    if [ -z "$WIN" ]; then
        WIN=$(xdotool search --onlyvisible --class "gmod" 2>/dev/null | head -1 || true)
    fi
    if [ -z "$WIN" ]; then
        PID=$(pgrep -f 'bin/linux64/gmod\|GarrysMod/hl2.sh' 2>/dev/null | head -1 || true)
        if [ -n "$PID" ]; then
            WIN=$(xdotool search --onlyvisible --pid "$PID" 2>/dev/null | head -1 || true)
        fi
    fi
    if [ -z "$WIN" ]; then
        WIN=$(wmctrl -l 2>/dev/null | grep -iE 'garry|hl2|source' | head -1 | awk '{print $1}' || true)
    fi
    if [ -n "$WIN" ]; then
        break
    fi
    sleep 0.3
done

# If we have a window (auto-focused or user manually focused it), do the vrmod_start injection,
# let it run for 15 seconds after the command, then terminate GMod.
if [ -n "${WIN:-}" ]; then
    echo 'Window focused. Pausing with Esc, opening console with `, sending vrmod_start, then unpausing...'
    sleep 3

    # Re-focus before each key to avoid BadWindow / lost focus during injection (common with Steam container windows).
    # Direct grave for console (no Esc before ~, per constraint). Re-activate/focus helps the xdotool send succeed.
    sleep 0.6
    xdotool windowactivate --sync "$WIN" 2>/dev/null || true
    xdotool windowfocus --sync "$WIN" 2>/dev/null || true
    xdotool key --window "$WIN" grave
    sleep 0.6

    # Type and execute vrmod_start
    xdotool windowactivate --sync "$WIN" 2>/dev/null || true
    xdotool windowfocus --sync "$WIN" 2>/dev/null || true
    xdotool type --window "$WIN" "vrmod_start"
    sleep 0.3
    xdotool windowactivate --sync "$WIN" 2>/dev/null || true
    xdotool windowfocus --sync "$WIN" 2>/dev/null || true
    xdotool key --window "$WIN" Return
    sleep 0.5

    # Unpause / close menu
    xdotool windowactivate --sync "$WIN" 2>/dev/null || true
    xdotool windowfocus --sync "$WIN" 2>/dev/null || true
    xdotool key --window "$WIN" Escape
    sleep 0.3

    echo 'vrmod_start sent via Esc-pause / ` console / unpause.'
    echo "Letting GMod run for 15 seconds after vrmod_start..."
    sleep 15

    echo "Terminating GMod (15s after vrmod_start)..."

    # Re-focus the window before trying console exit (VR/fullscreen runs can steal or lose focus).
    xdotool windowactivate --sync "$WIN" 2>/dev/null || true
    xdotool windowfocus --sync "$WIN" 2>/dev/null || true
    if [[ "$WIN" == 0x* ]]; then
        wmctrl -i -a "$WIN" 2>/dev/null || true
    fi
    sleep 0.3

    # Try clean exit via console first (best for logs).
    xdotool key --window "$WIN" grave 2>/dev/null || true
    sleep 0.3
    xdotool type --window "$WIN" "exit" 2>/dev/null || true
    sleep 0.2
    xdotool key --window "$WIN" Return 2>/dev/null || true
    sleep 1.5

    # === Hard terminate (this is what actually works on Steam Linux Runtime) ===
    echo "Current GMod-related processes before kill:"
    ps -eo pid,ppid,comm,args | grep -E 'gmod|hl2.sh|GarrysMod|pressure-vessel' | grep -v grep || echo "  (none visible)"
    pgrep -a -f 'bin/linux64/gmod' || pgrep -a -f 'hl2.sh' || echo "  (no pgrep matches)"

    # Kill by the PID(s) we captured right after launch (most reliable).
    if [ -n "${GAME_PIDS:-}" ]; then
        for pid in $GAME_PIDS; do
            echo "  kill -TERM $pid"
            kill -TERM "$pid" 2>/dev/null || true
        done
        sleep 0.8
        for pid in $GAME_PIDS; do
            echo "  kill -9 $pid"
            kill -9 "$pid" 2>/dev/null || true
        done
    fi

    # Fallbacks using patterns that match the actual current launch layout
    # (bin/linux64/gmod inside the pressure-vessel container, not hl2_linux).
    pkill -f 'bin/linux64/gmod' 2>/dev/null || true
    pkill -f 'GarrysMod/hl2.sh' 2>/dev/null || true
    sleep 0.6
    pkill -9 -f 'bin/linux64/gmod' 2>/dev/null || true
    pkill -9 -f 'GarrysMod/hl2.sh' 2>/dev/null || true

    echo "GMod terminated (15s after vrmod_start)."
else
    echo "No window handle available after focus step; skipping vrmod_start injection + 10s run."
    # Still make sure any previous instance is dead.
    pkill -f 'bin/linux64/gmod' 2>/dev/null || true
    pkill -f 'GarrysMod/hl2.sh' 2>/dev/null || true
    pkill -9 -f 'bin/linux64/gmod' 2>/dev/null || true
fi

# Always ensure nothing is left running at the end of the quick test cycle.
pkill -f 'bin/linux64/gmod' 2>/dev/null || true
pkill -f 'GarrysMod/hl2.sh' 2>/dev/null || true
pkill -9 -f 'bin/linux64/gmod' 2>/dev/null || true

echo ""
echo "The vrmod_quicktest.cfg sets desktopview 2. vrmod_start was injected via console (if focused)."
echo "Logs are being monitored by the active log tails."
echo ""
echo "To force-kill the game between tests: pkill -f 'bin/linux64/gmod'   (or pkill -f 'GarrysMod/hl2.sh')"
echo "Then just run this script again after you make code changes."
