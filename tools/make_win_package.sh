#!/usr/bin/env bash
# Package a Windows cross build (toolchains/mingw, build_mingw/) into the zips a release carries:
#
#   autobleem-win-<v>.zip    AutoBleem/bin/autobleem/: autobleem-gui.exe + src/resources + the SDL2 DLLs and
#                            libwinpthread-1.dll - the launcher for a PC, run as
#                            "autobleem-gui.exe <usb root>" (see CLAUDE.md, "Running on PC")
#   UpdateRoms-<v>.zip       UpdateRoms/: UpdateRoms.exe (static, stripped, UPX) + README.txt - the folder for
#                            a stick's root (tools/make_updateroms_bundle.sh makes the same from MSYS2)
#
# and, with --product DIR (a build configured with -DAB_TARGET=win - make_win.sh --product, ci/build.sh win),
# the Windows product's program folder, what the NSIS installer packs and what a portable copy is:
#
#   autobleem-win-product-<v>.zip   AutoBleem/: autobleem-gui.exe (GUI subsystem, full screen, the data tree
#                            found by itself - see EnvironmentSetup::fromWindowsInstall) + resources + DLLs,
#                            Themes/ (payload/Themes, copied into the data tree on the first start), emu/
#                            (pcsx-ab's Windows build when AB_PCSX_WIN_DIST names one - none yet: PS1 games
#                            go through RetroArch's pcsx_rearmed until then), dataroot.txt.example
#
#   tools/make_win_package.sh [--build-dir build_mingw] [--product build_win_product] [--out dist/win]
#                             [--version v2.0.0]
#
# The DLLs come from the SDL2 mingw development packages at AB_MINGW_SDL2 (/opt/mingw-sdl2 in the image)
# and Debian's mingw-w64 runtime - or, on MSYS2 (--product from make_win.sh), from the UCRT64 bin dir next
# to the compiler. AB_NO_UPX=1 leaves the exes unpacked.
set -euo pipefail
cd "$(dirname "$0")/.."
REPO="$PWD"

BUILD_DIR=build_mingw
PRODUCT_DIR=""
OUT=dist/win
VERSION=""
while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir) BUILD_DIR="$2"; shift 2 ;;
        --product) PRODUCT_DIR="$2"; shift 2 ;;
        --out) OUT="$2"; shift 2 ;;
        --version) VERSION="$2"; shift 2 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done
[ -n "$VERSION" ] || VERSION="$(git describe --tags --always --dirty 2>/dev/null || echo dev)"
SDL="${AB_MINGW_SDL2:-/opt/mingw-sdl2}"
STRIP="$(command -v x86_64-w64-mingw32-strip || command -v strip)"
pack() { if [ -z "${AB_NO_UPX:-}" ] && command -v upx >/dev/null 2>&1; then upx -q --best --lzma "$1" >/dev/null; fi; }

# the runtime DLLs next to an exe: SDL2's four and winpthread, from the mingw devkit (the image) or from
# the MSYS2 environment the compiler came from
runtime_dlls() {
    local dest="$1" dll src
    for dll in SDL2.dll SDL2_image.dll SDL2_mixer.dll SDL2_ttf.dll libwinpthread-1.dll; do
        src=""
        for candidate in "$SDL/bin/$dll" "$(dirname "$(command -v gcc)")/$dll" \
            /usr/x86_64-w64-mingw32/lib/$dll /usr/lib/gcc/x86_64-w64-mingw32/*-posix/$dll; do
            [ -f "$candidate" ] && { src="$candidate"; break; }
        done
        [ -n "$src" ] || { echo "$dll not found" >&2; exit 1; }
        cp "$src" "$dest/"
    done
}

# a zip of a staged folder's contents: zip where there is one, python's zipfile otherwise (MSYS2 has no zip)
zipdir() {
    local dir="$1" out="$2"
    rm -f "$out"
    if command -v zip >/dev/null 2>&1; then
        (cd "$dir" && zip -r -9 -q "$out" .)
    else
        python3 -c 'import shutil,sys; shutil.make_archive(sys.argv[1][:-4], "zip", sys.argv[2])' "$out" "$dir"
    fi
}

mkdir -p "$OUT"

# --- the product ---------------------------------------------------------------------------------------------
if [ -n "$PRODUCT_DIR" ]; then
    [ -f "$PRODUCT_DIR/autobleem-gui.exe" ] || { echo "no $PRODUCT_DIR/autobleem-gui.exe - make_win.sh --product first" >&2; exit 1; }
    PKG="$PRODUCT_DIR/package"
    rm -rf "$PKG"
    APP="$PKG/AutoBleem"
    mkdir -p "$APP"
    cp -a "$REPO/src/resources/." "$APP/"
    rm -f "$APP/internal.db" "$APP/run.sh"   # the console's own
    cp "$PRODUCT_DIR/autobleem-gui.exe" "$APP/"
    "$STRIP" "$APP/autobleem-gui.exe"
    pack "$APP/autobleem-gui.exe"
    runtime_dlls "$APP"
    mkdir -p "$APP/Themes"
    cp -a "$REPO/payload/Themes/." "$APP/Themes/"
    # pcsx-ab's Windows build, when there is one (AB_PCSX_WIN_DIST: a folder with pcsx-ab.exe and its DLLs)
    if [ -n "${AB_PCSX_WIN_DIST:-}" ] && [ -f "$AB_PCSX_WIN_DIST/pcsx-ab.exe" ]; then
        mkdir -p "$APP/emu"
        cp -a "$AB_PCSX_WIN_DIST/." "$APP/emu/"
    else
        echo "    (no pcsx-ab Windows build - PS1 games run through RetroArch's pcsx_rearmed core)"
    fi
    cat > "$APP/dataroot.txt.example" <<'EOF'
# A portable copy of AutoBleem: rename this file to dataroot.txt and put on its first line the folder that
# holds the games (Games\), the settings (System\) and the themes - a folder on this stick, say. Without
# it, and without an install (the registry), the launcher uses Documents\AutoBleem.
D:\AutoBleem
EOF
    find "$APP" -type f -name placeholder -delete
    ZIP="$REPO/$OUT/autobleem-win-product-$VERSION.zip"
    zipdir "$PKG" "$ZIP"
    echo "==> $ZIP ($(du -h "$ZIP" | cut -f1))"
    [ -f "$BUILD_DIR/autobleem-gui.exe" ] || exit 0
fi

[ -f "$BUILD_DIR/autobleem-gui.exe" ] || { echo "no $BUILD_DIR/autobleem-gui.exe - build the win target first" >&2; exit 1; }
PKG="$BUILD_DIR/package"
rm -rf "$PKG"

# --- the launcher ------------------------------------------------------------------------------------------
APP="$PKG/launcher/AutoBleem/bin/autobleem"
mkdir -p "$APP"
cp -a "$REPO/src/resources/." "$APP/"
cp "$BUILD_DIR/autobleem-gui.exe" "$APP/"
"$STRIP" "$APP/autobleem-gui.exe"
pack "$APP/autobleem-gui.exe"
# winpthread: the one GCC runtime DLL the exe still needs (libgcc/libstdc++ are linked in)
runtime_dlls "$APP"
find "$APP" -type f -name placeholder -delete
ZIP="$REPO/$OUT/autobleem-win-$VERSION.zip"
zipdir "$PKG/launcher" "$ZIP"
echo "==> $ZIP ($(du -h "$ZIP" | cut -f1))"

# --- UpdateRoms ----------------------------------------------------------------------------------------------
UR="$PKG/updateroms/UpdateRoms"
mkdir -p "$UR"
cp "$BUILD_DIR/apps/updateroms/UpdateRoms.exe" "$UR/"
cp -a "$REPO/apps/updateroms/resources/." "$UR/"
"$STRIP" "$UR/UpdateRoms.exe"
pack "$UR/UpdateRoms.exe"
ZIP="$REPO/$OUT/UpdateRoms-$VERSION.zip"
zipdir "$PKG/updateroms" "$ZIP"
echo "==> $ZIP ($(du -h "$ZIP" | cut -f1))"
