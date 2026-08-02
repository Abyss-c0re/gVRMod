#!/usr/bin/env bash
# Install gVRMod hooks so vrmod-x64 stays synced on every relevant commit.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HOOK_DIR="$ROOT/.git/hooks"
# When gVRMod itself is a submodule of something else, .git may be a file
if [[ -f "$ROOT/.git" ]]; then
  GITDIR=$(sed -n 's/^gitdir: //p' "$ROOT/.git")
  HOOK_DIR="$GITDIR/hooks"
fi
mkdir -p "$HOOK_DIR"
cp -f "$ROOT/scripts/hooks/pre-commit" "$HOOK_DIR/pre-commit"
chmod +x "$HOOK_DIR/pre-commit"
chmod +x "$ROOT/scripts/sync_vrmod_x64.sh"
echo "[+] Installed pre-commit → $HOOK_DIR/pre-commit"
echo "    On commits that touch addon/gvrmod/{lua,materials,models}, vrmod-x64 is synced."
echo "    Manual: ./scripts/sync_vrmod_x64.sh"
echo "    Check:  ./scripts/sync_vrmod_x64.sh --check"
