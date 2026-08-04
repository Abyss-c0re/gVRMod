#!/usr/bin/env bash
# Print the polish-loop agent brief path + cycle stamp (for operators / logs).
# The Grok scheduler runs the agent prompt; this script is a human entrypoint.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STATE="$ROOT/state/polish_loop"
echo "gvrmod polish loop state: $STATE"
echo "brief: $STATE/AGENT_PROMPT.md"
if [[ -f "$STATE/LOOP_STATE.json" ]]; then
  echo "--- LOOP_STATE.json ---"
  cat "$STATE/LOOP_STATE.json"
fi
echo "--- next focus (from GLOGIC_GAPS / LOOP_STATE) ---"
grep -E '"next_focus"|^\| G' "$STATE/LOOP_STATE.json" "$STATE/GLOGIC_GAPS.md" 2>/dev/null | head -20 || true
