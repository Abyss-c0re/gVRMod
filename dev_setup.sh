#!/bin/bash
set -e

echo "=== gVRMod Development Environment Setup ==="
echo "This script will detect your package manager (apt or pacman)"
echo "and install all required system dependencies to build the project."
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

    echo "[+] pacman dependencies installed."

else
    echo "[!] Unsupported package manager."
    echo "Typical packages needed:"
    echo "  - build tools: cmake, pkg-config, wget, unzip"
    echo "  - OpenGL/X11: libgl, libx11, libxrandr, libxinerama, libxcursor, libxi (dev versions)"
    echo "  - OpenXR: openxr / libopenxr-dev (headers + loader)"
    exit 1
fi

echo
echo "=== Setup complete! ==="
echo "You can now run:"
echo "  ./build.sh"
echo
echo "This will download any remaining vendored headers (GMod module base, OpenXR)"
echo "and build the release module into install/GarrysMod/garrysmod/lua/bin/"

