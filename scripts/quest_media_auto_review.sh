#!/usr/bin/env bash
# Auto-review Quest / KDE Connect media dropped into Downloads (or watch dirs).
# Any com.oculus.* / metacam / vrshell share is framed + summarized for agents.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WATCH_DIRS=(
  "${QUEST_MEDIA_DIR:-$HOME/Downloads}"
  "$HOME/Desktop"
)
OUT_ROOT="$ROOT/.scratch/quest_review"
LOG="$OUT_ROOT/auto_review.log"
STATE="$OUT_ROOT/seen.sha"
mkdir -p "$OUT_ROOT"
touch "$STATE" "$LOG"

patterns='com.oculus|metacam|vrshell|gvrmod|quest.*\.(mp4|webm|mkv|jpg|jpeg|png)$'

log() { echo "[$(date -Iseconds)] $*" | tee -a "$LOG"; }

review_one() {
  local f="$1"
  [[ -f "$f" ]] || return 0
  local base sha out
  base="$(basename "$f")"
  # skip tiny / incomplete
  local sz
  sz=$(stat -c%s "$f" 2>/dev/null || echo 0)
  [[ "$sz" -gt 1024 ]] || return 0
  sha="$(sha256sum "$f" | awk '{print $1}')"
  if grep -qx "$sha" "$STATE" 2>/dev/null; then
    return 0
  fi
  out="$OUT_ROOT/$(date +%Y%m%d_%H%M%S)_${base%.*}"
  mkdir -p "$out"
  cp -a "$f" "$out/source.bin" 2>/dev/null || cp -a "$f" "$out/"
  log "REVIEW begin $base ($sz bytes) → $out"

  local ext="${f##*.}"
  ext=$(echo "$ext" | tr 'A-Z' 'a-z')
  local nframes=0
  if [[ "$ext" =~ ^(mp4|webm|mkv|mov)$ ]]; then
    ffprobe -hide_banner "$f" >"$out/ffprobe.txt" 2>&1 || true
    ffmpeg -y -i "$f" -vf "fps=2,scale=960:-1" "$out/f_%04d.jpg" >"$out/ffmpeg.log" 2>&1 || true
    nframes=$(ls "$out"/f_*.jpg 2>/dev/null | wc -l)
    # contact sheet of first/mid/last
    if [[ "$nframes" -gt 0 ]]; then
      local mid=$(( (nframes + 1) / 2 ))
      printf -v mids "f_%04d.jpg" "$mid"
      printf -v lasts "f_%04d.jpg" "$nframes"
      {
        echo "# Quest media auto-review"
        echo
        echo "- source: \`$f\`"
        echo "- size: $sz"
        echo "- frames: $nframes @ 2fps"
        echo "- time: $(date -Iseconds)"
        echo
        echo "## Agent action"
        echo "Read frames in \`$out\` (f_0001, mid, last). Incorporate UX feedback into gVRMod native_launcher:"
        echo "palette contrast, laser hit, grab thrash, dual-hand trigger, panel size."
        echo
        if [[ -f "$out/f_0001.jpg" ]]; then echo "- first: $out/f_0001.jpg"; fi
        if [[ -f "$out/$mids" ]]; then echo "- mid: $out/$mids"; fi
        if [[ -f "$out/$lasts" ]]; then echo "- last: $out/$lasts"; fi
      } >"$out/REVIEW.md"
    fi
  elif [[ "$ext" =~ ^(jpg|jpeg|png|webp)$ ]]; then
    cp -a "$f" "$out/f_0001.jpg" 2>/dev/null || true
    {
      echo "# Quest image auto-review"
      echo
      echo "- source: \`$f\`"
      echo "- size: $sz"
      echo "- time: $(date -Iseconds)"
      echo
      echo "Read \`$out/f_0001.jpg\` and fix gVRMod UX accordingly."
    } >"$out/REVIEW.md"
    nframes=1
  fi

  echo "$sha" >>"$STATE"
  # Signal for polish agents / humans
  echo "$out" >"$OUT_ROOT/LATEST_REVIEW_DIR.txt"
  date -Iseconds >"$OUT_ROOT/LATEST_REVIEW_AT.txt"
  log "REVIEW done $base frames=$nframes dir=$out"

  # Optional KDE notify if tool exists
  if command -v kdeconnect-cli >/dev/null 2>&1; then
    kdeconnect-cli --ping-msg "gVRMod reviewed Quest media: $base ($nframes frames)" 2>/dev/null || true
  fi
}

scan_once() {
  local d
  for d in "${WATCH_DIRS[@]}"; do
    [[ -d "$d" ]] || continue
    # Recent files only (7 days) matching Quest/KDE share names
    find "$d" -maxdepth 1 -type f \( \
      -iname 'com.oculus*' -o -iname '*metacam*' -o -iname '*vrshell*' \
      -o -iname '*gvrmod*' -o -iname '*quest* -o -iname '*wivrn*'' \
    \) -mtime -7 2>/dev/null | while read -r f; do
      case "$f" in
        *.mp4|*.webm|*.mkv|*.mov|*.jpg|*.jpeg|*.png|*.MP4|*.JPG|*.PNG) review_one "$f" ;;
      esac
    done
  done
}

# One-shot mode
if [[ "${1:-}" == "--once" ]]; then
  scan_once
  exit 0
fi

# Continuous: poll every 8s (no inotify required; robust on NFS/KDE share)
log "watcher start dirs=${WATCH_DIRS[*]}"
# Catch up immediately
scan_once
while true; do
  sleep 8
  scan_once
done
