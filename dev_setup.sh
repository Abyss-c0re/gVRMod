#!/bin/bash
set -e

echo "=== gVRMod Development Environment Setup ==="
echo "This script installs dependencies for building gVRMod + Monado simulation"
echo "(including headless OpenGL + OpenCV texture analysis in containers)."
echo
echo "Options:"
echo "  --vision   Install extra packages for headless image-level stereo/distortion"
echo "             validation tests (OpenCV + EGL + xvfb + Vulkan)."
echo

# Helper to run package manager commands (works in containers/root/passwordless sudo)
run_privileged() {
    if [ "$(id -u)" -eq 0 ]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        if "$@"; then
            return 0
        fi
        echo "Direct execution failed, retrying with sudo..."
        sudo "$@"
    else
        "$@"
    fi
}

# Parse arguments
VISION=false
for arg in "$@"; do
    case $arg in
        --vision)
            VISION=true
            ;;
        -h|--help)
            echo "Usage: $0 [--vision]"
            echo "  --vision   Install OpenCV + headless EGL/OpenGL + xvfb for container/CI vision tests"
            exit 0
            ;;
        *)
            echo "[!] Unknown argument: $arg"
            exit 1
            ;;
    esac
done

if [ "$VISION" = true ]; then
    echo "[*] --vision mode enabled (OpenCV + headless OpenGL/EGL + xvfb)"
    echo
fi

if command -v apt-get >/dev/null 2>&1; then
    echo "[+] Detected Debian/Ubuntu (apt)"
    echo "Updating package lists..."
    run_privileged apt-get update -qq

    echo "Installing core build dependencies..."
    run_privileged apt-get install -y -qq \
        build-essential \
        cmake \
        ninja-build \
        meson \
        pkg-config \
        wget \
        unzip \
        ca-certificates \
        git

    echo "Installing OpenGL + X11 dependencies..."
    run_privileged apt-get install -y -qq \
        libgl1-mesa-dev \
        libglvnd-dev \
        libegl1-mesa-dev \
        libx11-dev \
        libxrandr-dev \
        libxinerama-dev \
        libxcursor-dev \
        libxi-dev

    echo "Installing Vulkan (recommended for Monado compositor)..."
    run_privileged apt-get install -y -qq \
        libvulkan-dev \
        mesa-vulkan-drivers \
        vulkan-tools

    # OpenXR headers/loader (fallback if not available)
    if ! run_privileged apt-get install -y -qq libopenxr-dev; then
        echo "[!] libopenxr-dev not available on this distro. Will be downloaded during build."
    fi

    if [ "$VISION" = true ]; then
        echo "Installing vision test dependencies (OpenCV + headless support)..."
        run_privileged apt-get install -y -qq \
            libopencv-dev \
            xvfb
    fi

    echo "[+] apt dependencies installed."

elif command -v pacman >/dev/null 2>&1; then
    echo "[+] Detected Arch Linux (pacman)"
    echo "Updating system..."
    run_privileged pacman -Syu --needed --noconfirm \
        base-devel \
        cmake \
        ninja \
        meson \
        pkg-config \
        wget \
        unzip \
        git

    echo "Installing OpenGL + Vulkan dependencies..."
    run_privileged pacman -S --needed --noconfirm \
        mesa \
        libglvnd \
        libgl \
        vulkan-icd-loader \
        vulkan-headers \
        vulkan-tools

    echo "Installing X11 dependencies..."
    run_privileged pacman -S --needed --noconfirm \
        libx11 \
        libxrandr \
        libxinerama \
        libxcursor \
        libxi

    if ! pacman -Q openxr >/dev/null 2>&1; then
        run_privileged pacman -S --needed --noconfirm openxr
    fi

    if [ "$VISION" = true ]; then
        echo "Installing vision test dependencies..."
        run_privileged pacman -S --needed --noconfirm \
            opencv \
            xorg-server-xvfb
    fi

    echo "[+] pacman dependencies installed."

else
    echo "[!] Unsupported package manager."
    echo "Typical packages needed:"
    echo "  - Build: cmake, ninja-build, meson, pkg-config, git"
    echo "  - OpenGL: libgl1-mesa-dev, libglvnd-dev, libegl1-mesa-dev"
    echo "  - X11: libx11-dev, libxrandr-dev, etc."
    echo "  - Vulkan: libvulkan-dev, mesa-vulkan-drivers"
    echo "  - OpenXR: libopenxr-dev"
    if [ "$VISION" = true ]; then
        echo "  - Vision: libopencv-dev, xvfb"
    fi
    exit 1
fi

echo
echo "=== Setup complete! ==="
echo
echo "You can now build the project with:"
echo "  ./build.sh"
echo
echo "For Monado simulated driver testing (headless):"
echo "  - Use Monado's built-in simulated driver (no extra setup needed)"
echo "  - Or build a custom driver for full programmatic head pose control"
echo

if [ "$VISION" = true ]; then
    echo "=== Vision / OpenCV Test Mode ==="
    echo
    echo "To build the advanced distortion/stereo validation tests:"
    echo "  cmake -B build_tests -DVRMOD_BUILD_TESTS=ON \\"
    echo "        -DVRMOD_BUILD_ADVANCED_IMAGE_TESTS=ON \\"
    echo "        -DVRMOD_ENABLE_OPENCV_ANALYSIS=ON"
    echo "  cmake --build build_tests -j\$(nproc)"
    echo
    echo "Run tests headlessly in container/CI:"
    echo "  xvfb-run -s \"-screen 0 1280x1024x24\" ./build_tests/vrmod_tests"
    echo
    echo "Alternative (pure EGL surfaceless, if supported):"
    echo "  EGL_PLATFORM=surfaceless ./build_tests/vrmod_tests"
    echo
    echo "Note: You are using OpenGL — make sure your app creates an OpenGL context"
    echo "      compatible with the Monado runtime (Monado supports OpenGL)."
fi