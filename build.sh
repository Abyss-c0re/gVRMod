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

# ── Clean previous *module* build artifacts ──
# Do NOT wipe install/ entirely — install/native/CubeUI is the product launcher
# (built by native_launcher). Wiping it makes cube-host fail with "missing binary".
echo "[+] Cleaning module build directories (preserving install/native CubeUI)..."
rm -rf build_release build_test
# Only clear the OpenXR module staging path under install/GarrysMod
rm -rf install/GarrysMod
mkdir -p install/GarrysMod

# ── Build release module ──
echo "[+] Building release module (OpenXR backend)..."
mkdir -p build_release
cd build_release
cmake .. -DCMAKE_BUILD_TYPE=Release -DVRMOD_BUILD_TESTS=OFF
make -j$(nproc) vrmod_release

# Ensure CubeUI launcher still exists after module build (rebuild if wiped/missing)
ROOT="$(cd .. && pwd)"
CUBE_BIN="$ROOT/install/native/CubeUI"
if [[ ! -x "$CUBE_BIN" ]]; then
    echo "[+] CubeUI missing after module build — rebuilding native launcher..."
    mkdir -p "$ROOT/native_launcher/build"
    (cd "$ROOT/native_launcher/build" && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -j"$(nproc)" --target CubeUI)
fi

echo ""
echo "[+] Build complete!"
echo "    Module: install/GarrysMod/garrysmod/lua/bin/gmcl_vrmod_xr_linux64.dll"
echo "    (OpenXR name — OpenVR keeps gmcl_vrmod_linux64.dll; both may coexist)"
if [[ -x "$CUBE_BIN" ]]; then
    echo "    CubeUI:  $CUBE_BIN"
else
    echo "    WARNING: CubeUI still missing at $CUBE_BIN — run: cmake --build native_launcher/build --target CubeUI"
fi
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
