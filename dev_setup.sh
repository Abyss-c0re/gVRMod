#!/bin/bash
set -e

echo "=== gVRMod Development Environment Setup ==="
echo "This script will detect your package manager (apt or pacman)"
echo "and install all required system dependencies to build the project."
echo
echo "Options:"
echo "  --vision   Also install dependencies for advanced headless image-level"
echo "             stereo/distortion validation tests (OpenCV + EGL + xvfb)."
echo "             Useful in containers/CI for the vision test suite."
echo

# Helper to run package manager commands.
# Tries without sudo first (useful in containers, root shells, or passwordless setups).
# Falls back to sudo only if the direct attempt fails.
run_privileged() {
    if [ "$(id -u)" -eq 0 ]; then
        # Already running as root
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        # Try without sudo first
        if "$@"; then
            return 0
        fi
        echo "Direct execution failed, retrying with sudo..."
        sudo "$@"
    else
        # No sudo available, just run it (will likely fail with permission error)
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
            echo "  --vision   Install extra packages for advanced vision/stereo tests"
            echo "             (OpenCV, headless EGL/OpenGL, xvfb) for container/CI use."
            exit 0
            ;;
        *)
            echo "[!] Unknown argument: $arg"
            echo "    Use --help for usage information."
            exit 1
            ;;
    esac
done

if [ "$VISION" = true ]; then
    echo "[*] --vision mode enabled: will install OpenCV + headless GL/EGL + xvfb packages."
    echo
fi

if command -v apt-get >/dev/null 2>&1; then
    echo "[+] Detected Debian/Ubuntu (apt)"
    echo "Updating package lists..."
    run_privileged apt-get update -qq

    echo "Installing build dependencies..."
    if ! run_privileged apt-get install -y -qq \
        build-essential \
        cmake \
        pkg-config \
        wget \
        unzip \
        ca-certificates \
        libgl1-mesa-dev \
        libx11-dev \
        libxrandr-dev \
        libxinerama-dev \
        libxcursor-dev \
        libxi-dev \
        libopenxr-dev; then
            echo "[!] libopenxr-dev not available on this distro (common on older releases)."
            echo "    OpenXR headers will be automatically downloaded by build.sh instead."
    fi

    if [ "$VISION" = true ]; then
        echo "Installing additional vision test dependencies (headless EGL + xvfb + OpenCV)..."
        run_privileged apt-get install -y -qq \
            libegl1-mesa-dev \
            xvfb \
            libopencv-dev
    fi

    echo "[+] apt dependencies installed."

elif command -v pacman >/dev/null 2>&1; then
    echo "[+] Detected Arch Linux (pacman)"
    echo "Updating system and installing dependencies..."
    run_privileged pacman -Syu --needed --noconfirm \
        base-devel \
        cmake \
        pkg-config \
        wget \
        unzip \
        mesa \
        libx11 \
        libxrandr \
        libxinerama \
        libxcursor \
        libxi \
        openxr

    if [ "$VISION" = true ]; then
        echo "Installing additional vision test dependencies (headless GL + xvfb + OpenCV)..."
        run_privileged pacman -S --needed --noconfirm \
            mesa \
            libglvnd \
            xorg-server-xvfb \
            opencv
    fi

    echo "[+] pacman dependencies installed."

else
    echo "[!] Unsupported package manager."
    echo "Typical packages needed:"
    echo "  - build tools: cmake, pkg-config, wget, unzip"
    echo "  - OpenGL/X11: libgl, libx11, libxrandr, libxinerama, libxcursor, libxi (dev versions)"
    echo "  - OpenXR: openxr / libopenxr-dev (headers + loader)"
    if [ "$VISION" = true ]; then
        echo "  - Vision tests (--vision): OpenCV, EGL/GL dev packages, xvfb / virtual framebuffer"
    fi
    exit 1
fi

echo
echo "=== Setup complete! ==="
echo "You can now run:"
echo "  ./build.sh"
echo
echo "This will download any remaining vendored headers (GMod module base, OpenXR)"
echo "and build the release module into install/GarrysMod/garrysmod/lua/bin/"

if [ "$VISION" = true ]; then
    echo
    echo "[*] Vision dependencies installed."
    echo "    To build the advanced distortion/stereo validation tests:"
    echo "      cmake -B build_tests -DVRMOD_BUILD_TESTS=ON \\"
    echo "            -DVRMOD_BUILD_ADVANCED_IMAGE_TESTS=ON \\"
    echo "            -DVRMOD_ENABLE_OPENCV_ANALYSIS=ON"
    echo "      cmake --build build_tests -j\$(nproc)"
    echo
    echo "    For headless execution in containers/CI (recommended):"
    echo "      xvfb-run -s \"-screen 0 1280x1024x24\" ./build_tests/vrmod_tests"
    echo "    (or use EGL surfaceless context if your image synthesis supports it)"
fi

