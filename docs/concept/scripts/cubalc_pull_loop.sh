#!/usr/bin/env bash
# LAB ONLY — not part of product install. See docs/concept/CUBALC_FUTURE.md
# Pull cubalc every 5.5 minutes so gVRMod stays aligned with SMX/prophecy SoT.
# Interval: 330 seconds (5.5 min). Safe: ff-only pull, no force.
set -euo pipefail
CUBALC_DIR="${CUBALC_DIR:-/home/voldemar/Dev/cubalc}"
GVRMOD_DIR="${GVRMOD_DIR:-/home/voldemar/Dev/GMod/gVRMod}"
INTERVAL="${CUBALC_PULL_INTERVAL_SEC:-330}"
LOG="${CUBALC_PULL_LOG:-$GVRMOD_DIR/.scratch/cubalc_pull.log}"
mkdir -p "$(dirname "$LOG")"

echo "[cubalc_pull] start interval=${INTERVAL}s dir=$CUBALC_DIR" | tee -a "$LOG"
while true; do
  ts="$(date -Iseconds)"
  if [[ -d "$CUBALC_DIR/.git" ]]; then
    rm -f "$CUBALC_DIR/programs/proof/test_write.txt" 2>/dev/null || true
    if git -C "$CUBALC_DIR" pull --ff-only origin 2>&1 | tee -a "$LOG"; then
      echo "[$ts] cubalc pull ok" | tee -a "$LOG"
      # Rebuild cubalc if Makefile present (best-effort)
      if [[ -f "$CUBALC_DIR/Makefile" ]]; then
        make -C "$CUBALC_DIR" -j"$(nproc)" 2>&1 | tail -20 | tee -a "$LOG" || true
      fi
      # Mirror law/docs snapshot into gVRMod state (read-only reference)
      mkdir -p "$GVRMOD_DIR/docs/concept/cubalc"
      for f in docs/P2P_SMX.md docs/SMX2_PROTOCOL.md docs/PROPHECY_MANIFEST.md include/cubalc_smx.h include/cubalc_law.h; do
        [[ -f "$CUBALC_DIR/$f" ]] && cp -a "$CUBALC_DIR/$f" "$GVRMOD_DIR/docs/concept/cubalc/" 2>/dev/null || true
      done
      # Touch stamp for polish agents
      date -Iseconds > "$GVRMOD_DIR/docs/concept/cubalc/LAST_PULL.txt"
    else
      echo "[$ts] cubalc pull failed (dirty or diverged?)" | tee -a "$LOG"
    fi
  else
    echo "[$ts] no git repo at $CUBALC_DIR" | tee -a "$LOG"
  fi
  sleep "$INTERVAL"
done
