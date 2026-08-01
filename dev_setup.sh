#!/bin/bash
set -e

echo "=== gVRMod Development Environment Setup ==="
echo "This script installs dependencies for building gVRMod + Monado mock headset simulation"
echo "(headless OpenGL + OpenCV texture analysis in containers)."
echo
echo "Options:"
echo "  --vision   Install extra packages for headless image-level stereo/distortion"
echo "             validation tests (OpenCV + EGL + xvfb + Vulkan)."
echo

run_privileged() {
    if [ "$(id -u)" -eq 0 ]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        if "$@"; then return 0; fi
        echo "Direct execution failed, retrying with sudo..."
        sudo "$@"
    else
        "$@"
    fi
}

VISION=false
for arg in "$@"; do
    case $arg in
        --vision) VISION=true ;;
        -h|--help)
            echo "Usage: $0 [--vision]"
            exit 0
            ;;
        *) echo "[!] Unknown argument: $arg"; exit 1 ;;
    esac
done

if [ "$VISION" = true ]; then
    echo "[*] --vision mode enabled (OpenCV + headless OpenGL/EGL + xvfb)"
fi

install_monado() {
    echo "Installing Monado (OpenXR runtime with simulated driver)..."

    if command -v apt-get >/dev/null 2>&1; then
        # Try official Monado PPA first (best option on Ubuntu)
        if ! apt-cache policy | grep -q monado; then
            echo "Adding Monado PPA..."
            run_privileged add-apt-repository -y ppa:monado-xr/monado || true
            run_privileged apt-get update -qq
        fi

        if run_privileged apt-get install -y -qq monado libopenxr1-monado; then
            echo "[+] Monado installed from PPA"
            return 0
        fi

        echo "[!] Monado package not available. Building from source..."
    fi

    # Fallback: Build Monado from source (most reliable for simulated driver)
    echo "Building Monado from source (this may take a few minutes)..."

    MONADO_DIR="/opt/monado"
    if [ ! -d "$MONADO_DIR" ]; then
        run_privileged git clone --depth 1 https://gitlab.freedesktop.org/monado/monado.git "$MONADO_DIR"
    fi

    cd "$MONADO_DIR"
    run_privileged mkdir -p build && cd build

    run_privileged cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DXRT_BUILD_DRIVER_SIMULATED=ON \
        -DXRT_BUILD_DRIVER_REMOTE=ON \
        -DXRT_HAVE_SYSTEMD=OFF

    run_privileged cmake --build . -j$(nproc)
    run_privileged cmake --install .

    echo "[+] Monado built and installed from source"
}

if command -v apt-get >/dev/null 2>&1; then
    echo "[+] Detected Debian/Ubuntu (apt)"
    run_privileged apt-get update -qq

    echo "Installing core build dependencies..."
    run_privileged apt-get install -y -qq \
        build-essential cmake ninja-build meson pkg-config \
        wget unzip ca-certificates git

    echo "Installing OpenGL + EGL + X11 dependencies..."
    run_privileged apt-get install -y -qq \
        libgl1-mesa-dev libglvnd-dev libegl1-mesa-dev \
        libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev

    echo "Installing Vulkan support (recommended for Monado)..."
    run_privileged apt-get install -y -qq \
        libvulkan-dev mesa-vulkan-drivers vulkan-tools

    echo "Installing Monado build dependencies..."
    run_privileged apt-get install -y -qq \
        libeigen3-dev glslang-tools

    # OpenXR
    if ! run_privileged apt-get install -y -qq libopenxr-dev; then
        echo "[!] libopenxr-dev not in repos. Will rely on Monado providing headers."
    fi

    install_monado

    if [ "$VISION" = true ]; then
        echo "Installing vision test dependencies..."
        run_privileged apt-get install -y -qq libopencv-dev xvfb
    fi

elif command -v pacman >/dev/null 2>&1; then
    echo "[+] Detected Arch Linux (pacman)"
    run_privileged pacman -Syu --needed --noconfirm \
        base-devel cmake ninja meson pkg-config wget unzip git

    run_privileged pacman -S --needed --noconfirm \
        mesa libglvnd libgl vulkan-icd-loader vulkan-headers vulkan-tools \
        eigen glslang

    if ! pacman -Q openxr >/dev/null 2>&1; then
        run_privileged pacman -S --needed --noconfirm openxr
    fi

    # Monado on Arch
    if ! pacman -Q monado >/dev/null 2>&1; then
        run_privileged pacman -S --needed --noconfirm monado
    fi

    if [ "$VISION" = true ]; then
        run_privileged pacman -S --needed --noconfirm opencv xorg-server-xvfb
    fi
else
    echo "[!] Unsupported package manager."
    exit 1
fi

echo
echo "=== Setup complete! ==="
echo
echo "Monado is now installed with the simulated driver enabled."
echo
echo "To use the mock headset:"
echo "  export XR_RUNTIME_JSON=/usr/share/openxr/1/openxr_monado.json"
echo "  # or let the system pick it up automatically"
echo
echo "You can now run your OpenXR application. It should fall back to"
echo "the simulated driver when no real headset is connected."
echo
echo "For advanced programmatic control (recommended for distortion tests),"
echo "consider building a custom Monado driver based on the 'sample_hmd' example."
echo

if [ "$VISION" = true ]; then
    echo "=== Vision / OpenCV Test Mode ==="
    echo
    echo "Build the advanced distortion tests with:"
    echo "  cmake -B build_tests -DVRMOD_BUILD_TESTS=ON \\"
    echo "        -DVRMOD_BUILD_ADVANCED_IMAGE_TESTS=ON \\"
    echo "        -DVRMOD_ENABLE_OPENCV_ANALYSIS=ON"
    echo "  cmake --build build_tests -j\$(nproc)"
    echo
    echo "Run headlessly:"
    echo "  xvfb-run -s \"-screen 0 1280x1024x24\" ./build_tests/vrmod_tests"
    echo
    echo "Note: Your app uses OpenGL. Monado supports OpenGL contexts."
fi