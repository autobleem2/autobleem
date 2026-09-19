#!/usr/bin/env bash
# Package a Windows cross build (toolchains/mingw, build_mingw/) into the two zips a release carries:
#
#   autobleem-win-<v>.zip    AutoBleem/bin/autobleem/: autobleem-gui.exe + src/resources + the SDL2 DLLs and
#                            libwinpthread-1.dll - the launcher for a PC, run as
#                            "autobleem-gui.exe <usb root>" (see CLAUDE.md, "Running on PC")
#   UpdateRoms-<v>.zip       UpdateRoms/: UpdateRoms.exe (static, stripped, UPX) + README.txt - the folder for
#                            a stick's root (tools/make_updateroms_bundle.sh makes the same from MSYS2)
#
#   tools/make_win_package.sh [--build-dir build_mingw] [--out dist/win] [--version v2.0.0]
#
# The DLLs come from the SDL2 mingw development packages at AB_MINGW_SDL2 (/opt/mingw-sdl2 in the image)
# and Debian's mingw-w64 runtime. AB_NO_UPX=1 leaves the exes unpacked.
set -euo pipefail
cd "$(dirname "$0")/.."
REPO="$PWD"

BUILD_DIR=build_mingw
OUT=dist/win
VERSION=""
while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir) BUILD_DIR="$2"; shift 2 ;;
        --out) OUT="$2"; shift 2 ;;
        --version) VERSION="$2"; shift 2 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done
[ -n "$VERSION" ] || VERSION="$(git describe --tags --always --dirty 2>/dev/null || echo dev)"
[ -f "$BUILD_DIR/autobleem-gui.exe" ] || { echo "no $BUILD_DIR/autobleem-gui.exe - build the win target first" >&2; exit 1; }
SDL="${AB_MINGW_SDL2:-/opt/mingw-sdl2}"
STRIP="$(command -v x86_64-w64-mingw32-strip || command -v strip)"
pack() { if [ -z "${AB_NO_UPX:-}" ] && command -v upx >/dev/null 2>&1; then upx -q --best --lzma "$1" >/dev/null; fi; }

PKG="$BUILD_DIR/package"
rm -rf "$PKG"
mkdir -p "$OUT"

# --- the launcher ------------------------------------------------------------------------------------------
APP="$PKG/launcher/AutoBleem/bin/autobleem"
mkdir -p "$APP"
cp -a "$REPO/src/resources/." "$APP/"
cp "$BUILD_DIR/autobleem-gui.exe" "$APP/"
"$STRIP" "$APP/autobleem-gui.exe"
pack "$APP/autobleem-gui.exe"
for dll in SDL2.dll SDL2_image.dll SDL2_mixer.dll SDL2_ttf.dll; do
    cp "$SDL/bin/$dll" "$APP/"
done
# winpthread: the one GCC runtime DLL the exe still needs (libgcc/libstdc++ are linked in)
pthread="$(ls /usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll /usr/lib/gcc/x86_64-w64-mingw32/*-posix/libwinpthread-1.dll 2>/dev/null | head -1 || true)"
[ -n "$pthread" ] || { echo "libwinpthread-1.dll not found" >&2; exit 1; }
cp "$pthread" "$APP/"
find "$APP" -type f -name placeholder -delete
ZIP="$REPO/$OUT/autobleem-win-$VERSION.zip"
rm -f "$ZIP"
(cd "$PKG/launcher" && zip -r -9 -q "$ZIP" .)
echo "==> $ZIP ($(du -h "$ZIP" | cut -f1))"

# --- UpdateRoms ----------------------------------------------------------------------------------------------
UR="$PKG/updateroms/UpdateRoms"
mkdir -p "$UR"
cp "$BUILD_DIR/apps/updateroms/UpdateRoms.exe" "$UR/"
cp -a "$REPO/apps/updateroms/resources/." "$UR/"
"$STRIP" "$UR/UpdateRoms.exe"
pack "$UR/UpdateRoms.exe"
ZIP="$REPO/$OUT/UpdateRoms-$VERSION.zip"
rm -f "$ZIP"
(cd "$PKG/updateroms" && zip -r -9 -q "$ZIP" .)
echo "==> $ZIP ($(du -h "$ZIP" | cut -f1))"
