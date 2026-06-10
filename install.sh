#!/bin/bash
#
# gVRMod Linux installer / uninstaller
#
# - Builds the module by calling build.sh
# - Locates your Garry's Mod installation (Steam Linux)
# - Collects libopenxr_loader.so + its runtime dependencies (using ldd)
# - Copies:
#     * the module (gmcl_vrmod_linux64.dll) → garrysmod/lua/bin/
#     * OpenXR loader + all its dependencies → bin/linux64/
#   (overwrites existing files)
# - Supports --uninstall to cleanly remove what was installed
#
# Usage:
#   ./install.sh                 # build + install (or update)
#   ./install.sh --gmod-dir /path/to/GarrysMod
#   ./install.sh --uninstall
#   ./install.sh --help
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_SCRIPT="$SCRIPT_DIR/build.sh"
MODULE_SRC="$SCRIPT_DIR/install/GarrysMod/garrysmod/lua/bin/gmcl_vrmod_linux64.dll"

# Files we manage (will be written to a manifest at install time)
MANIFEST_NAME=".vrmod_bundle_manifest.txt"

# Client-side addon (Lua, models, materials, etc.)
ADDON_SRC="$SCRIPT_DIR/addon/gvrmod"

# Default / common Steam locations (in order of preference)
GMOD_CANDIDATES=(
    "$HOME/.local/share/Steam/steamapps/common/GarrysMod"
    "$HOME/.steam/steam/steamapps/common/GarrysMod"
    "$HOME/.steam/root/steamapps/common/GarrysMod"
    "$HOME/Steam/steamapps/common/GarrysMod"
)

GMOD_DIR=""
UNINSTALL=false
SKIP_BUILD=false
YES=false

# Will be set after we find GMOD_DIR
ADDONS_DIR=""

print_usage() {
    cat <<EOF
gVRMod installer for Linux (Steam)

Usage:
  $0 [options]

Options:
  --gmod-dir PATH     Explicit path to the Garry's Mod folder
                      (the one that contains "garrysmod/" and "bin/")
  --uninstall         Remove the module (from lua/bin), the OpenXR
                      libraries (from bin/linux64), and the client
                      addon (garrysmod/addons/gvrmod) that were
                      installed by this script.
  --skip-build        Do not run build.sh (assume the module is already built).
  -y, --yes           Assume "yes" to all prompts (non-interactive).
  -h, --help          Show this help.

Examples:
  $0
  $0 --gmod-dir ~/.local/share/Steam/steamapps/common/GarrysMod
  $0 --uninstall
  $0 --uninstall -y

Note:
- OpenXR runtime libraries → bin/linux64/
- Lua module                  → garrysmod/lua/bin/
- Client addon (gvrmod/)      → garrysmod/addons/gvrmod/   (existing folder is deleted first)
EOF
}

# Simple arg parser
while [[ $# -gt 0 ]]; do
    case "$1" in
        --gmod-dir)
            GMOD_DIR="${2:-}"
            shift 2
            ;;
        --gmod-dir=*)
            GMOD_DIR="${1#*=}"
            shift
            ;;
        --uninstall|uninstall)
            UNINSTALL=true
            shift
            ;;
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        -y|--yes)
            YES=true
            shift
            ;;
        -h|--help)
            print_usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1"
            print_usage
            exit 1
            ;;
    esac
done

# -----------------------------------------------------------------------------
# Locate Garry's Mod
# -----------------------------------------------------------------------------
find_gmod_dir() {
    local dir

    # If user gave an explicit path, validate it
    if [[ -n "$GMOD_DIR" ]]; then
        if [[ -d "$GMOD_DIR/garrysmod" ]]; then
            echo "$GMOD_DIR"
            return 0
        else
            echo "ERROR: --gmod-dir was given but does not look like a Garry's Mod install:"
            echo "       $GMOD_DIR"
            echo "       (expected to contain a 'garrysmod/' subdirectory)"
            return 1
        fi
    fi

    # Try well-known locations
    for dir in "${GMOD_CANDIDATES[@]}"; do
        if [[ -d "$dir/garrysmod" ]]; then
            echo "$dir"
            return 0
        fi
    done

    # Last attempt: look for any "GarrysMod/garrysmod" under common Steam roots
    local steam_roots=(
        "$HOME/.local/share/Steam"
        "$HOME/.steam/steam"
        "$HOME/.steam/root"
        "$HOME/Steam"
    )

    for root in "${steam_roots[@]}"; do
        if [[ -d "$root" ]]; then
            # Search a couple of levels deep for other library folders
            while IFS= read -r -d '' candidate; do
                if [[ -d "$candidate/garrysmod" ]]; then
                    echo "$candidate"
                    return 0
                fi
            done < <(find "$root" -maxdepth 4 -type d -name "GarrysMod" -print0 2>/dev/null || true)
        fi
    done

    return 1
}

# -----------------------------------------------------------------------------
# Collect OpenXR loader + dependencies into a temporary directory
# -----------------------------------------------------------------------------
collect_openxr_bundle() {
    local out_dir="$1"
    mkdir -p "$out_dir"

    echo "Locating system OpenXR loader..."

    local loader=""
    # Try ldconfig first (fast and reliable when the package is installed)
    if command -v ldconfig >/dev/null 2>&1; then
        loader=$(ldconfig -p 2>/dev/null | grep -o '/[^ ]*libopenxr_loader\.so[^ ]*' | head -1 || true)
    fi

    # Fallback to common paths (Debian/Ubuntu multiarch, Fedora, Arch, etc.)
    if [[ -z "$loader" || ! -f "$loader" ]]; then
        local search_paths=(
            /usr/lib/x86_64-linux-gnu
            /usr/lib64
            /usr/lib
            /usr/local/lib
            /usr/local/lib64
        )
        for p in "${search_paths[@]}"; do
            if [[ -f "$p/libopenxr_loader.so.1" ]]; then
                loader="$p/libopenxr_loader.so.1"
                break
            elif [[ -f "$p/libopenxr_loader.so" ]]; then
                loader="$p/libopenxr_loader.so"
                break
            fi
        done
    fi

    if [[ -z "$loader" || ! -f "$loader" ]]; then
        echo "ERROR: Could not find libopenxr_loader.so on this system."
        echo "       Please install it first:"
        echo "         Debian/Ubuntu:  sudo apt install libopenxr-dev"
        echo "         Arch:           sudo pacman -S openxr"
        echo "         Fedora:         sudo dnf install openxr-devel"
        return 1
    fi

    echo "  Found loader: $loader"

    # Copy the loader itself (preserve .so or .so.1 name)
    cp -f "$loader" "$out_dir/"

    # Copy every shared object that ldd reports as a dependency.
    # This is what makes the "copy everything into GMod" approach work inside
    # the Steam Linux Runtime container.
    echo "  Collecting dependencies via ldd..."
    local dep
    while IFS= read -r dep; do
        [[ -z "$dep" || ! -f "$dep" ]] && continue
        # Only copy real files (skip linux-vdso.so.1 etc.)
        cp -f "$dep" "$out_dir/" 2>/dev/null || true
    done < <(ldd "$loader" 2>/dev/null | awk '/=>/ {print $3}' | sort -u || true)

    local count
    count=$(find "$out_dir" -maxdepth 1 -type f | wc -l)
    echo "  Bundle contains $count file(s) in $out_dir"
}

# -----------------------------------------------------------------------------
# Main
# -----------------------------------------------------------------------------
GMOD_DIR="$(find_gmod_dir || true)"

if [[ -z "$GMOD_DIR" ]]; then
    echo "ERROR: Could not automatically locate your Garry's Mod installation."
    echo ""
    echo "Common locations checked:"
    printf '  %s\n' "${GMOD_CANDIDATES[@]}"
    echo ""
    echo "Please run the script with an explicit path:"
    echo "  $0 --gmod-dir /path/to/GarrysMod"
    echo ""
    echo "Typical path on modern Steam Linux:"
    echo "  $HOME/.local/share/Steam/steamapps/common/GarrysMod"
    exit 1
fi

LUA_BIN_DIR="$GMOD_DIR/garrysmod/lua/bin"
ENGINE_BIN_DIR="$GMOD_DIR/bin/linux64"
MANIFEST="$LUA_BIN_DIR/$MANIFEST_NAME"
ADDONS_DIR="$GMOD_DIR/garrysmod/addons"

echo "Garry's Mod found at: $GMOD_DIR"
echo "  Lua modules:        $LUA_BIN_DIR"
echo "  Engine libs (XR):   $ENGINE_BIN_DIR"
echo "  Addons:             $ADDONS_DIR"
echo

if [[ "$UNINSTALL" == true ]]; then
    if [[ ! -d "$LUA_BIN_DIR" && ! -d "$ENGINE_BIN_DIR" && ! -d "$ADDONS_DIR/gvrmod" ]]; then
        echo "Nothing to uninstall (no module, no OpenXR libs, and no gvrmod addon found)."
        exit 0
    fi

    if [[ "$YES" != true ]]; then
        read -r -p "Remove gVRMod module and OpenXR libraries from this Garry's Mod install? [y/N] " reply
        if [[ ! "$reply" =~ ^[Yy]$ ]]; then
            echo "Aborted."
            exit 0
        fi
    fi

    echo "Uninstalling..."

    if [[ -f "$MANIFEST" ]]; then
        # Best case: we know exactly what we installed (paths are relative to GMOD_DIR)
        while IFS= read -r rel || [[ -n "$rel" ]]; do
            [[ -z "$rel" ]] && continue
            target="$GMOD_DIR/$rel"
            if [[ -d "$target" ]]; then
                rm -rf "$target"
                echo "  removed dir: $rel"
            elif [[ -f "$target" ]]; then
                rm -f "$target"
                echo "  removed: $rel"
            fi
        done < "$MANIFEST"
        rm -f "$MANIFEST"
    else
        # Fallback for old/manual installs
        rm -f "$LUA_BIN_DIR/gmcl_vrmod_linux64.dll"
        rm -f "$ENGINE_BIN_DIR"/libopenxr_loader*
        rm -rf "$ADDONS_DIR/gvrmod"
        echo "  removed module (lua/bin) + any libopenxr_loader* files (bin/linux64)"
        echo "  removed addon (if present)"
        echo "  (no manifest was present)"
    fi

    echo "Uninstall complete."
    exit 0
fi

# --------------------------- INSTALL / UPDATE ---------------------------

if [[ "$SKIP_BUILD" != true ]]; then
    if [[ ! -x "$BUILD_SCRIPT" ]]; then
        echo "ERROR: $BUILD_SCRIPT not found or not executable."
        exit 1
    fi

    echo "=== Building module (./build.sh) ==="
    (cd "$SCRIPT_DIR" && "$BUILD_SCRIPT")
    echo
fi

if [[ ! -f "$MODULE_SRC" ]]; then
    echo "ERROR: Built module not found at:"
    echo "       $MODULE_SRC"
    echo "       Did the build succeed?"
    exit 1
fi

echo "=== Collecting OpenXR loader and dependencies ==="
BUNDLE_DIR="$(mktemp -d -t vrmod_openxr_bundle.XXXXXX)"
if ! collect_openxr_bundle "$BUNDLE_DIR"; then
    rm -rf "$BUNDLE_DIR"
    exit 1
fi
echo

echo "=== Installing to Garry's Mod ==="
mkdir -p "$LUA_BIN_DIR"
mkdir -p "$ENGINE_BIN_DIR"

# Start fresh manifest for this install.
# We store paths relative to the GMod root so uninstall works regardless of where
# the manifest itself lives.
: > "$MANIFEST"

# 1. The actual GMod Lua module (must stay in garrysmod/lua/bin)
MODULE_REL="garrysmod/lua/bin/gmcl_vrmod_linux64.dll"
cp -f "$MODULE_SRC" "$GMOD_DIR/$MODULE_REL"
echo "$MODULE_REL" >> "$MANIFEST"
echo "  copied: gmcl_vrmod_linux64.dll   →  garrysmod/lua/bin/"

# 2. OpenXR loader + all its dependencies (must go to the engine's bin/linux64
#    so the Steam Linux Runtime / engine can resolve them when the module dlopen's).
shopt -s nullglob
for f in "$BUNDLE_DIR"/*; do
    bn="$(basename "$f")"
    REL="bin/linux64/$bn"
    cp -f "$f" "$GMOD_DIR/$REL"
    echo "$REL" >> "$MANIFEST"
    echo "  copied: $bn   →  bin/linux64/"
done
shopt -u nullglob

rm -rf "$BUNDLE_DIR"

echo
echo "=== Installing client addon (gvrmod) ==="
ADDONS_DIR="$GMOD_DIR/garrysmod/addons"

if [[ -d "$ADDON_SRC" ]]; then
    if [[ -d "$ADDONS_DIR/gvrmod" ]]; then
        echo "  Removing existing addon: $ADDONS_DIR/gvrmod"
        rm -rf "$ADDONS_DIR/gvrmod"
    fi

    cp -r "$ADDON_SRC" "$ADDONS_DIR/"
    echo "garrysmod/addons/gvrmod" >> "$MANIFEST"
    echo "  copied: gvrmod/ → garrysmod/addons/gvrmod/"
else
    echo "  WARNING: No addon/ directory found at $ADDON_SRC — skipping client addon install."
fi

echo
echo "Installation successful!"
echo "  Module installed to:      $LUA_BIN_DIR"
echo "  OpenXR libs installed to: $ENGINE_BIN_DIR"
echo "  Client addon installed to: $ADDONS_DIR/gvrmod"
echo
echo "A manifest was written to:"
echo "    $MANIFEST"
echo
echo "You can now start Garry's Mod and use vrmod."
echo "If you ever want to remove everything this script installed, run:"
echo "    $0 --uninstall"
echo
echo "Tip: If you update your system OpenXR packages later and want the"
echo "     latest loader + deps, just run this installer again."
