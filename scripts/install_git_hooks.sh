#!/usr/bin/env bash
# Install gVRMod hooks for the single addon submodule (addon/vrmod-x64).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HOOK_DIR="$ROOT/.git/hooks"
if [[ -f "$ROOT/.git" ]]; then
  GITDIR=$(sed -n 's/^gitdir: //p' "$ROOT/.git")
  HOOK_DIR="$GITDIR/hooks"
fi
mkdir -p "$HOOK_DIR"
cp -f "$ROOT/scripts/hooks/pre-commit" "$HOOK_DIR/pre-commit"
chmod +x "$HOOK_DIR/pre-commit"
chmod +x "$ROOT/scripts/sync_vrmod_x64.sh"
echo "[+] Installed pre-commit → $HOOK_DIR/pre-commit"
echo "    Single Lua addon SoT: addon/vrmod-x64/ (submodule)"
echo "    Manual: ./scripts/sync_vrmod_x64.sh"
echo "    Check:  ./scripts/sync_vrmod_x64.sh --check"
echo "    Status: ./scripts/sync_vrmod_x64.sh --status"
