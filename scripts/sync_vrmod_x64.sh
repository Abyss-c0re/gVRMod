#!/usr/bin/env bash
# Sync shared client addon between gVRMod (OpenXR monorepo) and vrmod-x64 (submodule / Workshop).
#
# Layout:
#   gVRMod/addon/gvrmod/{lua,materials,models}  ↔  vrmod-x64/{lua,materials,models}
#
# Default direction: gVRMod → submodule (Truth Matrix SoT lives in monorepo when editing here).
# Use --from-submodule to pull Workshop tree into addon/gvrmod (does not delete OpenXR-only files).
#
# Usage:
#   ./scripts/sync_vrmod_x64.sh                 # push monorepo → submodule, commit both if dirty
#   ./scripts/sync_vrmod_x64.sh --dry-run
#   ./scripts/sync_vrmod_x64.sh --from-submodule
#   ./scripts/sync_vrmod_x64.sh --no-commit     # copy only
#   ./scripts/sync_vrmod_x64.sh --check         # exit 1 if out of sync
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SUB="$ROOT/vrmod-x64"
SRC_ADDON="$ROOT/addon/gvrmod"
DIR_TO_SUB=1
DRY=0
COMMIT=1
CHECK=0
MSG="${SYNC_MSG:-sync: Truth Matrix from gVRMod monorepo}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --from-submodule) DIR_TO_SUB=0; MSG="${SYNC_MSG:-sync: Truth Matrix from vrmod-x64 submodule}"; shift ;;
    --dry-run) DRY=1; shift ;;
    --no-commit) COMMIT=0; shift ;;
    --check) CHECK=1; COMMIT=0; shift ;;
    -h|--help)
      sed -n '2,20p' "$0"
      exit 0
      ;;
    *)
      echo "Unknown arg: $1" >&2
      exit 2
      ;;
  esac
done

if [[ ! -d "$SUB/.git" && ! -f "$SUB/.git" ]]; then
  echo "error: vrmod-x64 submodule missing. Run: git submodule update --init --recursive" >&2
  exit 1
fi
if [[ ! -d "$SRC_ADDON/lua" ]]; then
  echo "error: missing $SRC_ADDON/lua" >&2
  exit 1
fi

# Paths that must never be clobbered on either side (repo identity).
RSYNC_EXCLUDES=(
  --exclude '.git/'
  --exclude 'state/'
  --exclude 'docs/'
  --exclude '.github/'
  --exclude '.workshop_id'
  --exclude 'addon.json'
  --exclude 'README.md'
  --exclude 'LICENSE'
  --exclude '*.md'
)

sync_tree() {
  local from="$1" to="$2" label="$3"
  mkdir -p "$to"
  local flags=(-a --delete)
  # --delete only inside lua/ so OpenXR-only modules stay intentional on both sides.
  # materials/models: no --delete (keep extras).
  if [[ "$label" == "lua" ]]; then
    flags+=(--delete)
  fi
  if [[ $DRY -eq 1 || $CHECK -eq 1 ]]; then
    # Show whether anything would change
    local out
    out=$(rsync -ani "${flags[@]}" "${RSYNC_EXCLUDES[@]}" "$from/" "$to/" || true)
    if [[ -n "$out" ]]; then
      echo "=== $label would change ($from → $to) ==="
      echo "$out"
      return 1
    fi
    echo "=== $label OK (in sync) ==="
    return 0
  fi
  rsync "${flags[@]}" "${RSYNC_EXCLUDES[@]}" "$from/" "$to/"
  echo "[+] synced $label: $from → $to"
}

DIFF=0
if [[ $DIR_TO_SUB -eq 1 ]]; then
  sync_tree "$SRC_ADDON/lua"        "$SUB/lua"        lua        || DIFF=1
  sync_tree "$SRC_ADDON/materials"  "$SUB/materials"  materials  || DIFF=1
  sync_tree "$SRC_ADDON/models"     "$SUB/models"     models     || DIFF=1
else
  sync_tree "$SUB/lua"        "$SRC_ADDON/lua"        lua        || DIFF=1
  sync_tree "$SUB/materials"  "$SRC_ADDON/materials"  materials  || DIFF=1
  sync_tree "$SUB/models"     "$SRC_ADDON/models"     models     || DIFF=1
fi

if [[ $CHECK -eq 1 ]]; then
  if [[ $DIFF -ne 0 ]]; then
    echo "OUT OF SYNC — run: ./scripts/sync_vrmod_x64.sh" >&2
    exit 1
  fi
  echo "OK: gVRMod addon and vrmod-x64 submodule trees match (lua/materials/models)."
  exit 0
fi

if [[ $DRY -eq 1 ]]; then
  exit 0
fi

if [[ $COMMIT -eq 0 ]]; then
  echo "[i] --no-commit: filesystem only."
  exit 0
fi

# Commit inside submodule if dirty
if [[ $DIR_TO_SUB -eq 1 ]]; then
  pushd "$SUB" >/dev/null
  git add -A lua materials models 2>/dev/null || true
  if ! git diff --cached --quiet 2>/dev/null || ! git diff --quiet 2>/dev/null; then
    # Only stage shared trees
    git status -sb
    if git diff --cached --quiet; then
      git add lua materials models || true
    fi
    if ! git diff --cached --quiet; then
      git commit -m "$MSG"
      echo "[+] committed in submodule vrmod-x64"
      # Push submodule so parent pointer is meaningful for others
      if git rev-parse --abbrev-ref --symbolic-full-name @{u} >/dev/null 2>&1; then
        git push || echo "[!] submodule push failed — push vrmod-x64 manually" >&2
      else
        git push -u origin HEAD || echo "[!] submodule push failed — push vrmod-x64 manually" >&2
      fi
    fi
  else
    echo "[i] submodule already clean"
  fi
  popd >/dev/null

  # Bump parent submodule pointer
  cd "$ROOT"
  git add vrmod-x64 .gitmodules 2>/dev/null || git add vrmod-x64
  if ! git diff --cached --quiet; then
    git commit -m "submodule: bump vrmod-x64 ($MSG)"
    echo "[+] parent committed submodule pointer"
  else
    echo "[i] parent pointer already current"
  fi
else
  # from-submodule: only monorepo addon may need commit
  cd "$ROOT"
  git add addon/gvrmod/lua addon/gvrmod/materials addon/gvrmod/models
  if ! git diff --cached --quiet; then
    git commit -m "$MSG"
    echo "[+] committed monorepo addon from submodule"
  else
    echo "[i] monorepo addon already matched submodule"
  fi
fi

echo "Done."
