#!/bin/bash
set -e

echo "=== gVRMod Build Script (OpenXR) ==="

mkdir -p "deps/gmod"
mkdir -p "deps/openxr"

# ── GMod module base headers ──
if [ ! -f "deps/gmod/Interface.h" ]; then
    echo "[+] Downloading GMod module base headers..."
    wget -q -O deps/gmod/tmp.zip https://github.com/Facepunch/gmod-module-base/archive/15bf18f369a41ac3d4eba29ee0679f386ec628b7.zip
    unzip -j deps/gmod/tmp.zip gmod-module-base-15bf18f369a41ac3d4eba29ee0679f386ec628b7/include/GarrysMod/Lua/* -d deps/gmod/
    rm deps/gmod/tmp.zip
fi

# ── OpenXR headers ──
# We only need the headers; the loader (libopenxr_loader.so) is provided by the
# system OpenXR runtime (Monado, SteamVR OpenXR layer, Meta Quest Link, etc.).
if [ ! -f "deps/openxr/openxr/openxr.h" ]; then
    XR_TAG="release-1.1.60"
    echo "[+] Downloading OpenXR-SDK ${XR_TAG} headers..."
    TMP_TAR="/tmp/openxr-sdk-${XR_TAG}.tar.gz"
    wget -q -O "$TMP_TAR" "https://github.com/KhronosGroup/OpenXR-SDK/archive/refs/tags/${XR_TAG}.tar.gz"
    tar --wildcards -xzf "$TMP_TAR" --strip-components=2 -C deps/openxr "OpenXR-SDK-${XR_TAG}/include/openxr"
    rm -f "$TMP_TAR"
    echo "    OpenXR headers installed to deps/openxr/openxr/"
fi

# ── Note on OpenXR loader ──
# We no longer link against libopenxr_loader at build time (pure dlopen at runtime).
# The loader (libopenxr_loader.so / .so.1) is still required on the *target* machine
# at runtime so the module can discover the active OpenXR runtime.
# It is normally provided by libopenxr-dev / openxr packages or your runtime (Monado/ALVR etc.).
# On the build machine you only need the headers (which this script downloads if missing).
if ! ldconfig -p 2>/dev/null | grep -q "libopenxr_loader.so"; then
    echo "[i] Note: libopenxr_loader.so not found on build machine (this is usually fine now)."
    echo "    Players will need a working OpenXR runtime + loader on their system."
fi

# ── Clean previous build and install artifacts ──
# Ensures we always start from a clean state (no stale CMake caches, object files,
# or old modules left behind in the install staging area).
echo "[+] Cleaning build and install directories..."
rm -rf build_release build_test install

# ── Build release module ──
echo "[+] Building release module (OpenXR backend)..."
mkdir -p build_release
cd build_release
cmake .. -DCMAKE_BUILD_TYPE=Release -DVRMOD_BUILD_TESTS=OFF
make -j$(nproc) vrmod_release

echo ""
echo "[+] Build complete!"
echo "    Module: install/GarrysMod/garrysmod/lua/bin/gmcl_vrmod_linux64.dll"
echo ""

# ── Build and run tests ──
if [ "${1}" = "--test" ] || [ "${1}" = "-t" ]; then
    echo "[+] Building and running tests..."
    cd "${OLDPWD}"
    mkdir -p build_test
    cd build_test
    cmake .. -DCMAKE_BUILD_TYPE=Debug -DVRMOD_BUILD_TESTS=ON
    make -j$(nproc) vrmod_tests
    echo ""
    echo "=== Running tests ==="
    ./vrmod_tests
fi
