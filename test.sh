#!/bin/bash
set -euo pipefail

# test.sh
# Rebuilds the project (release module + test binary) from a clean state and runs
# the offline C++ unit test suite.
#
# This is the recommended command for developers when working on math, rendering
# logic, input, or distortion-related changes (projection matrices, UV bounds,
# eye separation, head rotation/tilt cases, etc.).
#
# The unit tests require no Garry's Mod and no VR runtime. They are especially
# valuable for validating the "distortion" behavior (the legacy Linux + OpenGL +
# "auto offset" / renderOffset path is treated as the golden reference).
#
# Usage:
#   ./test.sh            # clean rebuild + run tests
#   ./test.sh --no-clean # reuse existing build dirs (faster incremental)
#
# After this you can still do full in-game cycles with:
#   ./quick_test.sh
#
# See also:
#   ./build.sh --test
#   ./dev_setup.sh   (installs system build deps)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

DO_CLEAN=1
if [[ "${1:-}" == "--no-clean" || "${1:-}" == "-n" ]]; then
    DO_CLEAN=0
fi

echo "=== gVRMod Rebuild + Test ==="

# Ensure header deps are present (build.sh logic is authoritative).
if [[ ! -f "deps/gmod/Interface.h" || ! -f "deps/openxr/openxr/openxr.h" ]]; then
    echo "[i] Vendored headers missing or incomplete — running build.sh header fetch (no full build)."
    # We only want the header download parts; build.sh will handle mkdirs etc.
    mkdir -p "deps/gmod" "deps/openxr"
    if [ ! -f "deps/gmod/Interface.h" ]; then
        echo "[+] Downloading GMod module base headers..."
        wget -q -O deps/gmod/tmp.zip https://github.com/Facepunch/gmod-module-base/archive/15bf18f369a41ac3d4eba29ee0679f386ec628b7.zip
        unzip -j deps/gmod/tmp.zip gmod-module-base-15bf18f369a41ac3d4eba29ee0679f386ec628b7/include/GarrysMod/Lua/* -d deps/gmod/
        rm -f deps/gmod/tmp.zip
    fi
    if [ ! -f "deps/openxr/openxr/openxr.h" ]; then
        XR_TAG="release-1.1.60"
        echo "[+] Downloading OpenXR-SDK ${XR_TAG} headers..."
        TMP_TAR="/tmp/openxr-sdk-${XR_TAG}.tar.gz"
        wget -q -O "$TMP_TAR" "https://github.com/KhronosGroup/OpenXR-SDK/archive/refs/tags/${XR_TAG}.tar.gz"
        tar --wildcards -xzf "$TMP_TAR" --strip-components=2 -C deps/openxr "OpenXR-SDK-${XR_TAG}/include/openxr"
        rm -f "$TMP_TAR"
    fi
fi

if [[ $DO_CLEAN -eq 1 ]]; then
    echo "[+] Cleaning previous build artifacts (build_release, build_test, build_tests, install)..."
    rm -rf build_release build_test build_tests install
else
    echo "[i] Skipping clean (--no-clean)."
fi

# ── Build release module (so install/ is fresh if someone wants to deploy after) ──
echo "[+] Building release module (OpenXR)..."
mkdir -p build_release
pushd build_release >/dev/null
cmake .. -DCMAKE_BUILD_TYPE=Release -DVRMOD_BUILD_TESTS=OFF >/dev/null
make -j"$(nproc)" vrmod_release
popd >/dev/null
echo "    Release module: install/GarrysMod/garrysmod/lua/bin/gmcl_vrmod_linux64.dll"

# ── Build + run tests ──
echo "[+] Building test runner (VRMOD_BUILD_TESTS=ON)..."
mkdir -p build_tests
pushd build_tests >/dev/null
cmake .. -DCMAKE_BUILD_TYPE=Debug -DVRMOD_BUILD_TESTS=ON >/dev/null
make -j"$(nproc)" vrmod_tests
popd >/dev/null

echo ""
echo "=== Running C++ unit tests ==="
echo ""

./build_tests/vrmod_tests

echo ""
echo "=== All tests completed ==="
echo ""
echo "Release artifact is ready in install/ if you want to deploy with ./install.sh --skip-build"
echo "For a full in-game quick test cycle (build + auto-launch GMod + vrmod_start): ./quick_test.sh"
echo ""
echo "Tip: run with --no-clean for faster incremental rebuilds during active development."
