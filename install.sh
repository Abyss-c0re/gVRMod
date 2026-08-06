#!/bin/bash
#
# gVRMod Linux installer / uninstaller
#
# - Builds the module by calling build.sh
# - Locates your Garry's Mod installation (Steam Linux)
# - Collects libopenxr_loader.so + its runtime dependencies (using ldd)
# - Copies:
#     * the OpenXR module (gmcl_vrmod_xr_linux64.dll) → garrysmod/lua/bin/
#       (does not overwrite OpenVR gmcl_vrmod_linux64.dll — both may coexist)
#     * OpenXR loader + all its dependencies → bin/linux64/
#   (overwrites existing files)
# - Supports --uninstall to cleanly remove what was installed
# - Installs a desktop launcher (gVRMod.desktop) for the OpenXR launcher script
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
MODULE_SRC="$SCRIPT_DIR/install/GarrysMod/garrysmod/lua/bin/gmcl_vrmod_xr_linux64.dll"
# Legacy OpenXR name (pre-rename) — removed on install/uninstall so it does not shadow OpenVR.
LEGACY_XR_MODULE_NAME="gmcl_vrmod_linux64.dll"

# Files we manage (will be written to a manifest at install time)
MANIFEST_NAME=".vrmod_bundle_manifest.txt"

# Client-side addon — single tree (git submodule: Workshop package layout)
ADDON_SRC="$SCRIPT_DIR/addon/vrmod-x64"
ADDON_INSTALL_NAME="vrmod-x64"
# Home map (InfMap hub) — lives in monorepo, not a submodule
HOME_ADDON_SRC="$SCRIPT_DIR/addon/cube_home"
HOME_ADDON_INSTALL_NAME="cube_home"

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
  --uninstall         Remove the OpenXR module (gmcl_vrmod_xr_*), OpenXR
                      libraries (bin/linux64), and the client addon
                      (garrysmod/addons/vrmod-x64, legacy gvrmod/).
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
- OpenXR Lua module         → garrysmod/lua/bin/gmcl_vrmod_xr_linux64.dll
  (OpenVR gmcl_vrmod_linux64.dll is never overwritten — dual install OK)
- Client addon              → garrysmod/addons/vrmod-x64 → symlink to addon/vrmod-x64
- Desktop launcher          → ~/.local/share/applications/gvrmod.desktop
EOF
}

# Install application menu entry — Cube native launcher (default product entry)
install_desktop_entry() {
    # Product law: desktop opens Cube OpenXR menu first; GMod only after START GAME.
    local launcher="$SCRIPT_DIR/scripts/CubeUI.sh"
    local desktop_src="$SCRIPT_DIR/scripts/gvrmod.desktop"
    local app_dir="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
    local icon_dir="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor/256x256/apps"
    local desktop_dst="$app_dir/gvrmod.desktop"
    local banner="$SCRIPT_DIR/docs/assets/banner.png"
    local icon_name="gvrmod"

    echo
    echo "=== Installing desktop launcher (Cube UI) ==="

    if [[ ! -x "$launcher" ]]; then
        if [[ -f "$launcher" ]]; then
            chmod +x "$launcher"
        else
            echo "  WARNING: Cube launcher missing at $launcher — skip desktop entry"
            return 0
        fi
    fi

    mkdir -p "$app_dir" "$icon_dir"

    if [[ -f "$banner" ]]; then
        cp -f "$banner" "$icon_dir/gvrmod.png"
        icon_name="gvrmod"
        echo "  icon: $icon_dir/gvrmod.png"
    else
        icon_name="steam_icon_4000"
        echo "  icon: steam_icon_4000 (banner.png not found)"
    fi

    # Prefer host launcher (clean env, no rebuild). Falls back to CubeUI.sh.
    local host_launcher="$SCRIPT_DIR/scripts/CubeUI_host.sh"
    local exec_line="$launcher"
    if [[ -x "$host_launcher" ]]; then
        exec_line="$host_launcher"
    fi

    # Always write a fresh .desktop with absolute paths for this install tree
    cat > "$desktop_dst" <<EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=gVRMod
GenericName=VR Launcher for Garry's Mod
Comment=OpenXR Cube menu — maps, addons, settings, bindings; GMod starts on START GAME
TryExec=$SCRIPT_DIR/install/native/CubeUI
Exec=$exec_line
Path=$SCRIPT_DIR
Icon=$icon_name
Terminal=false
StartupNotify=true
Categories=Game;ActionGame;Simulation;
Keywords=GMod;VR;OpenXR;WiVRn;Quest;gVRMod;Cube;Launcher;Garry;
StartupWMClass=CubeUI_glx
EOF
    chmod +x "$desktop_dst" 2>/dev/null || true

    # Repo template (install rewrites absolute Exec)
    if [[ -d "$SCRIPT_DIR/scripts" ]]; then
        cat > "$desktop_src" <<EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=gVRMod
GenericName=VR Launcher for Garry's Mod
Comment=OpenXR Cube menu — maps, addons, settings, bindings; GMod starts on START GAME
TryExec=$SCRIPT_DIR/install/native/CubeUI
Exec=$exec_line
Path=$SCRIPT_DIR
Icon=$icon_name
Terminal=false
StartupNotify=true
Categories=Game;ActionGame;Simulation;
Keywords=GMod;VR;OpenXR;WiVRn;Quest;gVRMod;Cube;Launcher;Garry;
StartupWMClass=CubeUI_glx
EOF
    fi

    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "$app_dir" 2>/dev/null || true
    fi
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -f -t "${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor" 2>/dev/null || true
    fi

    # KDE Menu Editor can pin apps into .hidden or Exclude them from Games.
    # Unhide gvrmod and pin it next to Garry's Mod when that custom menu exists.
    local kmenu="${XDG_CONFIG_HOME:-$HOME/.config}/menus/applications-kmenuedit.menu"
    if [[ -f "$kmenu" ]]; then
        python3 - "$kmenu" <<'PY' 2>/dev/null || true
import re, sys
from pathlib import Path
p = Path(sys.argv[1])
t = p.read_text(encoding="utf-8", errors="replace")
orig = t
# drop from .hidden / Exclude blocks
t = re.sub(r"\s*<Filename>gvrmod\.desktop</Filename>\s*", "\n", t)
t = re.sub(r"\s*<Filename>CubeUI\.desktop</Filename>\s*", "\n", t)
# ensure Games layout lists gvrmod after Garry's Mod (or at end of Games Layout)
g = t.find("<Name>Games</Name>")
if g >= 0:
    g_end = t.find("</Menu>", g)
    chunk = t[g:g_end]
    if "gvrmod.desktop" not in chunk:
        needle = "<Filename>Garry's Mod.desktop</Filename>"
        line = "   <Filename>gvrmod.desktop</Filename>"
        if needle in chunk:
            chunk = chunk.replace(needle, needle + "\n" + line, 1)
        else:
            chunk = chunk.replace("</Layout>", line + "\n  </Layout>", 1)
        t = t[:g] + chunk + t[g_end:]
if t != orig:
    p.write_text(t, encoding="utf-8")
    print("  kmenuedit: unhid gvrmod.desktop → Games")
PY
    fi
    if command -v kbuildsycoca6 >/dev/null 2>&1; then
        kbuildsycoca6 --noincremental >/dev/null 2>&1 || true
    elif command -v kbuildsycoca5 >/dev/null 2>&1; then
        kbuildsycoca5 --noincremental >/dev/null 2>&1 || true
    fi

    echo "  desktop: $desktop_dst"
    echo "  name:    gVRMod  (Application Launcher → Games)"
    echo "  launch:  $launcher  (Cube UI — not GMod-first)"
}

uninstall_desktop_entry() {
    local app_dir="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
    local icon="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor/256x256/apps/gvrmod.png"
    local desktop_dst="$app_dir/gvrmod.desktop"
    if [[ -f "$desktop_dst" ]]; then
        rm -f "$desktop_dst"
        echo "  removed desktop: $desktop_dst"
    fi
    if [[ -f "$icon" ]]; then
        rm -f "$icon"
        echo "  removed icon: $icon"
    fi
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "$app_dir" 2>/dev/null || true
    fi
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
    if [[ ! -d "$LUA_BIN_DIR" && ! -d "$ENGINE_BIN_DIR" \
        && ! -d "$ADDONS_DIR/vrmod-x64" && ! -d "$ADDONS_DIR/gvrmod" \
        && ! -e "$ADDONS_DIR/cube_home" ]]; then
        echo "Nothing to uninstall (no module, no OpenXR libs, and no addon found)."
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
        # Fallback for old/manual installs — only remove OpenXR-named binaries.
        # Never delete OpenVR gmcl_vrmod_linux64.dll (Workshop / module-master).
        rm -f "$LUA_BIN_DIR/gmcl_vrmod_xr_linux64.dll"
        rm -f "$ENGINE_BIN_DIR"/libopenxr_loader*
        rm -rf "$ADDONS_DIR/vrmod-x64" "$ADDONS_DIR/gvrmod"
        rm -f "$ADDONS_DIR/cube_home" 2>/dev/null || rm -rf "$ADDONS_DIR/cube_home"
        echo "  removed gmcl_vrmod_xr_linux64.dll (OpenXR) + libopenxr_loader* (bin/linux64)"
        echo "  removed addons/vrmod-x64, cube_home, and legacy addons/gvrmod (if present)"
        echo "  left gmcl_vrmod_linux64.dll alone (OpenVR slot)"
        echo "  (no manifest was present)"
    fi

    uninstall_desktop_entry

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

# 1. OpenXR GMod module — distinct name so OpenVR gmcl_vrmod_linux64.dll can coexist.
MODULE_REL="garrysmod/lua/bin/gmcl_vrmod_xr_linux64.dll"
cp -f "$MODULE_SRC" "$GMOD_DIR/$MODULE_REL"
echo "$MODULE_REL" >> "$MANIFEST"
echo "  copied: gmcl_vrmod_xr_linux64.dll   →  garrysmod/lua/bin/  (require \"vrmod_xr\")"
if [[ -f "$LUA_BIN_DIR/gmcl_vrmod_linux64.dll" ]]; then
    echo "  note: gmcl_vrmod_linux64.dll also present — OpenVR + OpenXR dual install OK"
    echo "        (if that file is an *old* gVRMod OpenXR build, remove/rename it so OpenVR can use the slot)"
fi

# 2. OpenXR loader + deps → engine bin/linux64 (Steam/engine path) AND next to
#    the XR module in garrysmod/lua/bin (module dlopen searches moduleDir first —
#    "not detecting headset" is often a failed loader load under Steam Runtime).
shopt -s nullglob
for f in "$BUNDLE_DIR"/*; do
    bn="$(basename "$f")"
    REL="bin/linux64/$bn"
    cp -f "$f" "$GMOD_DIR/$REL"
    echo "$REL" >> "$MANIFEST"
    echo "  copied: $bn   →  bin/linux64/"
    # Mirror next to gmcl_vrmod_xr_*.dll for XR_LoadLoader candidates
    REL2="garrysmod/lua/bin/$bn"
    cp -f "$f" "$GMOD_DIR/$REL2"
    echo "$REL2" >> "$MANIFEST"
    echo "  copied: $bn   →  garrysmod/lua/bin/"
done
shopt -u nullglob
# Convenience soname next to module
if [[ -f "$LUA_BIN_DIR/libopenxr_loader.so.1" ]]; then
    ln -sfn libopenxr_loader.so.1 "$LUA_BIN_DIR/libopenxr_loader.so"
fi

rm -rf "$BUNDLE_DIR"

echo
echo "=== Installing client addon (addon/vrmod-x64 submodule) ==="
ADDONS_DIR="$GMOD_DIR/garrysmod/addons"
mkdir -p "$ADDONS_DIR"

link_addon() {
    local src="$1"
    local name="$2"
    local dest="$ADDONS_DIR/$name"
    if [[ ! -d "$src" ]]; then
        echo "  WARNING: No addon at $src"
        return 1
    fi
    if [[ -L "$dest" ]]; then
        local cur want
        cur="$(readlink -f "$dest" 2>/dev/null || true)"
        want="$(readlink -f "$src")"
        if [[ "$cur" == "$want" ]]; then
            echo "  already linked: $dest → $want"
        else
            rm -f "$dest"
            ln -sfn "$want" "$dest"
            echo "  relinked: $dest → $want"
        fi
    else
        if [[ -e "$dest" ]]; then
            echo "  Removing existing addon tree: $dest"
            rm -rf "$dest"
        fi
        local want
        want="$(readlink -f "$src")"
        ln -sfn "$want" "$dest"
        echo "  linked: $dest → $want"
    fi
    echo "garrysmod/addons/$name" >> "$MANIFEST"
    return 0
}

if [[ -d "$ADDON_SRC/lua" ]]; then
    # Drop legacy folder name from older gVRMod installs
    if [[ -d "$ADDONS_DIR/gvrmod" ]] || [[ -L "$ADDONS_DIR/gvrmod" ]]; then
        echo "  Removing legacy addon: $ADDONS_DIR/gvrmod"
        rm -rf "$ADDONS_DIR/gvrmod"
    fi
    # Prefer symlink to gVRMod submodule so GMod always loads live Dev tree
    # (no stale rsync copies). Replace real dirs / wrong links.
    link_addon "$ADDON_SRC" "$ADDON_INSTALL_NAME" || \
        echo "  WARNING: No addon at $ADDON_SRC — run: git submodule update --init --recursive"
else
    echo "  WARNING: No addon at $ADDON_SRC — run: git submodule update --init --recursive"
fi

echo
echo "=== Installing home map addon (addon/cube_home) ==="
if [[ -f "$HOME_ADDON_SRC/maps/xr_infmap_passthrough.bsp" ]] || [[ -d "$HOME_ADDON_SRC/lua" ]]; then
    link_addon "$HOME_ADDON_SRC" "$HOME_ADDON_INSTALL_NAME" || true
    # Also land BSP under garrysmod/maps so CubeUI scan + cold +map always see it
    # (category was Sandbox-only for gm_*; xr_* lived in Other before the scan fix).
    if [[ -f "$HOME_ADDON_SRC/maps/xr_infmap_passthrough.bsp" ]]; then
        mkdir -p "$GMOD_DIR/garrysmod/maps"
        cp -f "$HOME_ADDON_SRC/maps/xr_infmap_passthrough.bsp" \
            "$GMOD_DIR/garrysmod/maps/xr_infmap_passthrough.bsp"
        echo "  maps/xr_infmap_passthrough.bsp installed for launcher scan"
    fi
    echo "  Home map: xr_infmap_passthrough (XR Home Passthrough; needs InfMap WS 2905327911)"
else
    echo "  WARNING: cube_home missing maps/xr_infmap_passthrough.bsp — home map unavailable"
fi

# Best-effort: extract InfMap base from Steam workshop if subscribed
WS_INFMAP=""
for root in \
    "$GMOD_DIR/../../workshop/content/4000/2905327911" \
    "$HOME/.steam/steam/steamapps/workshop/content/4000/2905327911" \
    "$HOME/.local/share/Steam/steamapps/workshop/content/4000/2905327911"; do
    if [[ -d "$root" ]]; then
        WS_INFMAP="$root"
        break
    fi
done
if [[ -n "$WS_INFMAP" ]] && [[ ! -d "$ADDONS_DIR/cube_ws_2905327911" ]]; then
    GMA=$(find "$WS_INFMAP" -maxdepth 1 -name '*.gma' 2>/dev/null | head -1 || true)
    if [[ -n "$GMA" ]] && command -v "$SCRIPT_DIR/native_launcher/build/CubeUI" >/dev/null 2>&1; then
        : # extraction happens on Cube Start via EnsureMapAvailable
    fi
    if [[ -n "$GMA" ]] && [[ ! -d "$ADDONS_DIR/cube_ws_2905327911" ]]; then
        echo "  InfMap workshop GMA present; CubeUI will extract on first Start of an InfMap map"
    fi
elif [[ -d "$ADDONS_DIR/cube_ws_2905327911" ]]; then
    echo "  InfMap base extract present: addons/cube_ws_2905327911"
fi

install_desktop_entry

echo
echo "Installation successful!"
echo "  Module installed to:      $LUA_BIN_DIR"
echo "  OpenXR libs installed to: $ENGINE_BIN_DIR"
echo "  Client addon installed to: $ADDONS_DIR/$ADDON_INSTALL_NAME"
echo "  Home map addon:           $ADDONS_DIR/$HOME_ADDON_INSTALL_NAME"
echo "  Desktop launcher:         ~/.local/share/applications/gvrmod.desktop"
echo
echo "A manifest was written to:"
echo "    $MANIFEST"
echo
echo "App menu: search for \"gVRMod\" → scripts/CubeUI.sh"
echo "(GMod is started only after START GAME in the Cube headset menu.)"
echo "Legacy GMod-only helper: scripts/gvrmod_launcher.sh"
echo "If you ever want to remove everything this script installed, run:"
echo "    $0 --uninstall"
echo
echo "=== Runtime ==="
echo "  Binaries: OpenXR = gmcl_vrmod_xr_* ; OpenVR = gmcl_vrmod_* (both may coexist)."
echo "  Prefer:   vrmod_prefer_backend auto|openxr|openvr"
echo "  Status:   vrmod_backend"
echo "  OpenXR:   any active runtime (SteamVR OpenXR, Monado, ALVR, WiVRn, …)"
echo "  Start:    desktop \"gVRMod\" (Cube UI) → START GAME → GMod"
echo
echo "Tip: re-run this installer after updating packages or the submodule."
