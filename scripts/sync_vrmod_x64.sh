#!/usr/bin/env bash
# Commit / push / bump the single Lua addon submodule.
#
# There is ONE client addon tree in this monorepo:
#   addon/vrmod-x64/   ← git submodule (Workshop package SoT)
#
# Edit Lua under addon/vrmod-x64/, then run this script (or the pre-commit hook).
#
# Usage:
#   ./scripts/sync_vrmod_x64.sh              # commit dirty submodule + bump parent pointer
#   ./scripts/sync_vrmod_x64.sh --no-push    # commit only, no remote push
#   ./scripts/sync_vrmod_x64.sh --check      # exit 1 if submodule has uncommitted changes
#   ./scripts/sync_vrmod_x64.sh --status
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SUB="$ROOT/addon/vrmod-x64"
PUSH=1
CHECK=0
STATUS=0
MSG="${SYNC_MSG:-chore: update addon Truth Matrix}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-push) PUSH=0; shift ;;
    --check) CHECK=1; shift ;;
    --status) STATUS=1; shift ;;
    -h|--help)
      sed -n '2,18p' "$0"
      exit 0
      ;;
    *)
      echo "Unknown arg: $1" >&2
      exit 2
      ;;
  esac
done

if [[ ! -d "$SUB" ]]; then
  echo "error: missing $SUB — run: git submodule update --init --recursive" >&2
  exit 1
fi
if [[ ! -e "$SUB/.git" ]]; then
  echo "error: $SUB is not a git submodule checkout" >&2
  exit 1
fi

if [[ $STATUS -eq 1 ]]; then
  echo "Addon SoT: addon/vrmod-x64 (submodule)"
  echo "Install:   ./install.sh  →  garrysmod/addons/vrmod-x64"
  (cd "$SUB" && git status -sb && git log -1 --oneline)
  (cd "$ROOT" && git submodule status)
  exit 0
fi

if [[ $CHECK -eq 1 ]]; then
  if (cd "$SUB" && { ! git diff --quiet || ! git diff --cached --quiet; }); then
    echo "DIRTY: addon/vrmod-x64 has uncommitted changes" >&2
    (cd "$SUB" && git status -sb) >&2
    exit 1
  fi
  echo "OK: addon/vrmod-x64 clean"
  exit 0
fi

pushd "$SUB" >/dev/null
git add -A
if ! git diff --cached --quiet 2>/dev/null; then
  git status -sb
  git commit -m "$MSG"
  echo "[+] committed in submodule addon/vrmod-x64"
  if [[ $PUSH -eq 1 ]]; then
    if git rev-parse --abbrev-ref --symbolic-full-name @{u} >/dev/null 2>&1; then
      git push || echo "[!] submodule push failed — push manually" >&2
    else
      git push -u origin HEAD || echo "[!] submodule push failed — push manually" >&2
    fi
  fi
else
  echo "[i] submodule already clean (nothing to commit)"
fi
popd >/dev/null

cd "$ROOT"
git add addon/vrmod-x64 .gitmodules 2>/dev/null || git add addon/vrmod-x64
if ! git diff --cached --quiet 2>/dev/null; then
  git commit -m "submodule: bump addon/vrmod-x64 ($MSG)" || true
  echo "[+] parent committed submodule pointer"
else
  echo "[i] parent pointer already current"
fi

echo "Done."
