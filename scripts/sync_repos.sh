#!/usr/bin/env bash
# =============================================================================
# Keep gVRMod + vrmod-x64 repos up to date (fetch/ff-pull + push local)
#
# Safe defaults:
#   - Never hard-reset local work
#   - Only fast-forward pull when clean (or with --ff-only attempt)
#   - Push current branch if ahead of origin
#
# Usage:
#   ./scripts/sync_repos.sh              # fetch + ff-pull both; push if ahead
#   ./scripts/sync_repos.sh --pull-only  # no push
#   ./scripts/sync_repos.sh --push-only  # push only
#   ./scripts/sync_repos.sh --status     # show status only
# =============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ADDON="$ROOT/addon/vrmod-x64"
PULL=1
PUSH=1
STATUS_ONLY=0

for a in "$@"; do
  case "$a" in
    --pull-only) PUSH=0 ;;
    --push-only) PULL=0 ;;
    --status) STATUS_ONLY=1 ;;
    -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
  esac
done

sync_one() {
  local dir="$1" name="$2"
  if [[ ! -d "$dir/.git" ]] && [[ ! -f "$dir/.git" ]]; then
    echo "[sync] skip $name (not a git repo)"
    return 0
  fi
  echo ""
  echo "══ $name ($dir) ══"
  (
    cd "$dir"
    git rev-parse --is-inside-work-tree >/dev/null
    local branch
    branch="$(git rev-parse --abbrev-ref HEAD)"
    echo "  branch=$branch"
    git status -sb

    if [[ "$STATUS_ONLY" == "1" ]]; then
      return 0
    fi

    if [[ "$PULL" == "1" ]]; then
      echo "  fetch origin…"
      git fetch origin 2>&1 | sed 's/^/  /' || true
      if git rev-parse --verify "origin/$branch" >/dev/null 2>&1; then
        if git diff --quiet && git diff --cached --quiet; then
          echo "  ff-pull origin/$branch…"
          if git merge --ff-only "origin/$branch" 2>&1 | sed 's/^/  /'; then
            :
          else
            echo "  (ff-only blocked — resolve manually; no hard reset)"
          fi
        else
          echo "  local changes present — skip pull (no hard reset)"
        fi
      fi
    fi

    if [[ "$PUSH" == "1" ]]; then
      local ahead
      ahead="$(git rev-list --count "origin/$branch..HEAD" 2>/dev/null || echo 0)"
      if [[ "${ahead:-0}" -gt 0 ]]; then
        echo "  push origin $branch (ahead $ahead)…"
        git push -u origin "$branch" 2>&1 | sed 's/^/  /'
      else
        echo "  nothing to push"
      fi
    fi

    echo "  HEAD=$(git rev-parse --short HEAD) $(git log -1 --oneline)"
  )
}

# Never leave steam-runtime on LD path for git/curl
env -u LD_LIBRARY_PATH bash -c '
  source /dev/null
'

sync_one "$ADDON" "vrmod-x64 (addon)"
sync_one "$ROOT" "gVRMod (module + native launcher)"

# After addon move, refresh submodule pointer if parent is a superproject
if [[ -f "$ROOT/.gitmodules" ]] || git -C "$ROOT" submodule status >/dev/null 2>&1; then
  echo ""
  echo "══ submodule note ══"
  git -C "$ROOT" submodule status 2>/dev/null | sed 's/^/  /' || true
fi

echo ""
echo "[sync] done"
