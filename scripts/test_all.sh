#!/usr/bin/env bash
# Full offline test gate: contracts + Lua + C++ module + launcher
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

FAST=0
NO_CLEAN=0
for a in "$@"; do
  case "$a" in
    --fast) FAST=1 ;;
    --no-clean|-n) NO_CLEAN=1 ;;
  esac
done

pass=0
fail=0
run() {
  local name="$1"; shift
  echo ""
  echo "======== $name ========"
  if "$@"; then
    echo "[PASS] $name"
    pass=$((pass + 1))
  else
    echo "[FAIL] $name"
    fail=$((fail + 1))
  fi
}

echo "=== gVRMod test_all (offline) ==="

run "contracts.generate" python3 scripts/gen_contracts.py
run "contracts.check" python3 scripts/check_test_contracts.py

if command -v luajit >/dev/null 2>&1; then
  run "lua.unit" luajit tests/lua/run.lua
  run "lua.scenarios" luajit tests/scenarios/run.lua
elif command -v lua5.1 >/dev/null 2>&1; then
  run "lua.unit" lua5.1 tests/lua/run.lua
  run "lua.scenarios" lua5.1 tests/scenarios/run.lua
else
  echo "[FAIL] no luajit/lua5.1"
  fail=$((fail + 1))
fi

if [[ $FAST -eq 0 ]]; then
  # C++ module tests
  mkdir -p build_tests
  pushd build_tests >/dev/null
  if [[ $NO_CLEAN -eq 0 ]] || [[ ! -f CMakeCache.txt ]]; then
    cmake .. -DCMAKE_BUILD_TYPE=Debug -DVRMOD_BUILD_TESTS=ON >/dev/null
  fi
  make -j"$(nproc)" vrmod_tests
  popd >/dev/null
  run "cpp.module" ./build_tests/vrmod_tests

  # Launcher math/desktop tests
  mkdir -p native_launcher/build_tests
  pushd native_launcher/build_tests >/dev/null
  if [[ $NO_CLEAN -eq 0 ]] || [[ ! -f CMakeCache.txt ]]; then
    cmake .. -DCUBE_BUILD_TESTS=ON >/dev/null
  fi
  make -j"$(nproc)" cube_launcher_tests
  popd >/dev/null
  run "cpp.launcher" ./native_launcher/build_tests/cube_launcher_tests
else
  echo "[i] --fast: skipped C++ rebuilds"
fi

echo ""
echo "========================================"
echo "test_all: $pass suites passed, $fail failed"
echo "========================================"
[[ $fail -eq 0 ]]
