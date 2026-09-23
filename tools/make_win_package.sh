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
#                            AutoBleemWinSetup.exe, Themes/ (payload/Themes, copied into the data tree on the
#                            first start), the two PS1 emulators as emu/ (pcsx-ab) and emunxt/ (pcsx-abnxt) -
#                            Options -> "PS1 Emulator" picks, as on the console and the Pi - from a local
#                            build (AB_PCSX_WIN_DIST / AB_PCSXNXT_WIN_DIST: a folder with pcsx-ab.exe and its
#                            DLLs) or else the site's win64 package (emu/pcsx-ab, emu/pcsx-abnxt latest.json;
#                            AB_NO_PCSX=1 ships none: PS1 games then go through RetroArch's pcsx_rearmed),
#                            dataroot.txt.example
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
[ -n "$VERSION" ] || VERSION="$(git describe --tags --exclude nightly --always --dirty 2>/dev/null || echo dev)"
SDL="${AB_MINGW_SDL2:-/opt/mingw-sdl2}"
STRIP="$(command -v x86_64-w64-mingw32-strip || command -v strip)"
pack() { if [ -z "${AB_NO_UPX:-}" ] && command -v upx >/dev/null 2>&1; then upx -q --best --lzma "$1" >/dev/null; fi; }

# the runtime DLLs next to an exe: SDL2's four and winpthread, from the mingw devkit (the image) or from
# the MSYS2 environment the compiler came from
runtime_dlls() {
    local dest="$1" dll src bindir
    bindir="$(dirname "$(command -v gcc)")"
    for dll in SDL2.dll SDL2_image.dll SDL2_mixer.dll SDL2_ttf.dll libwinpthread-1.dll; do
        src=""
        for candidate in "$SDL/bin/$dll" "$bindir/$dll" \
            /usr/x86_64-w64-mingw32/lib/$dll /usr/lib/gcc/x86_64-w64-mingw32/*-posix/$dll; do
            [ -f "$candidate" ] && { src="$candidate"; break; }
        done
        [ -n "$src" ] || { echo "$dll not found" >&2; exit 1; }
        cp "$src" "$dest/"
    done
    # the image's SDL DLLs are self-contained; MSYS2's pull in forty more (libpng, freetype, the codecs,
    # libstdc++ ...) - there, every DLL ldd finds under the environment's bin dir comes along, so the
    # folder runs on a PC with no MSYS2
    [ -d "$SDL/bin" ] || msys_dll_closure "$dest"
}

# every DLL of the MSYS2 environment the exes and DLLs in a folder need, copied next to them
msys_dll_closure() {
    local dest="$1" bindir exe found
    [ -n "${MSYSTEM:-}" ] && command -v ldd >/dev/null 2>&1 || return 0
    bindir="$(dirname "$(command -v gcc)")"
    for exe in "$dest"/*.exe "$dest"/*.dll; do
        [ -f "$exe" ] || continue
        for found in $(ldd "$exe" 2>/dev/null | grep -io "$bindir/[^ ]*[.]dll" | sort -u); do
            [ -f "$dest/$(basename "$found")" ] || cp "$found" "$dest/"
        done
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

# emulator NAME FOLDER [LOCAL_DIST]: pcsx-ab.exe with its DLLs, plugins and skin into $APP/FOLDER - from
# LOCAL_DIST when it holds one, else the site's win64 zip (emu/<name>/latest.json; its one top folder is
# stripped); nothing, with a note, when neither is there or AB_NO_PCSX is set
emulator() {
    local name="$1" folder="$2" local_dist="$3" url tmp
    if [ -n "${AB_NO_PCSX:-}" ]; then
        echo "    AB_NO_PCSX: no $name"
        return
    fi
    if [ -n "$local_dist" ] && [ -f "$local_dist/pcsx-ab.exe" ]; then
        # a build dir as well as a packaged folder: the exe, its DLLs, the plugins and the menu's skin
        mkdir -p "$APP/$folder"
        cp "$local_dist/pcsx-ab.exe" "$APP/$folder/"
        cp "$local_dist"/*.dll "$APP/$folder/" 2>/dev/null || true
        [ -d "$local_dist/plugins" ] && cp -a "$local_dist/plugins" "$APP/$folder/"
        if [ -d "$local_dist/skin" ]; then
            cp -a "$local_dist/skin" "$APP/$folder/"
        elif [ -d "$local_dist/../frontend/pandora/skin" ]; then
            cp -a "$local_dist/../frontend/pandora/skin" "$APP/$folder/" # a build dir: the menu's skin from the tree
        fi
        msys_dll_closure "$APP/$folder"
        echo "    $name from $local_dist -> $folder/"
        return
    fi
    url="$(curl -sfL "${AB_REPO_URL:-https://autobleem.retromenele.pl}/emu/$name/latest.json" \
        | python3 -c 'import json,sys; print(json.load(sys.stdin)["files"]["win64"]["url"])' 2>/dev/null || true)"
    if [ -z "$url" ]; then
        echo "    (no Windows build of $name - locally or on the site)"
        return
    fi
    tmp="$(mktemp -d)"
    if curl -sfL -o "$tmp/emu.zip" "$url" && python3 -c '
import sys, zipfile
z = zipfile.ZipFile(sys.argv[1])
names = [n for n in z.namelist() if not n.endswith("/")]
top = names[0].split("/")[0] + "/" if all("/" in n and n.split("/")[0] == names[0].split("/")[0] for n in names) else ""
for n in names:
    dest = sys.argv[2] + "/" + n[len(top):]
    import os; os.makedirs(os.path.dirname(dest), exist_ok=True)
    open(dest, "wb").write(z.read(n))
' "$tmp/emu.zip" "$APP/$folder" && [ -f "$APP/$folder/pcsx-ab.exe" ]; then
        echo "    $name from $url -> $folder/"
    else
        echo "    (the site's $name package could not be fetched - none shipped)"
        rm -rf "$APP/$folder"
    fi
    rm -rf "$tmp"
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
    cp "$REPO/LICENSE" "$REPO/THIRD_PARTY_NOTICES.md" "$APP/"  # the GPL and the notices travel with the binary
    rm -f "$APP/internal.db" "$APP/run.sh"   # the console's own
    cp "$PRODUCT_DIR/autobleem-gui.exe" "$APP/"
    "$STRIP" "$APP/autobleem-gui.exe"
    pack "$APP/autobleem-gui.exe"
    runtime_dlls "$APP"
    # the setup helper: the data tree from the download repository (the NSIS installer runs it, and the
    # Start Menu's "AutoBleem Setup"); static, so it needs no DLL
    if [ -f "$PRODUCT_DIR/apps/installer/AutoBleemWinSetup.exe" ]; then
        cp "$PRODUCT_DIR/apps/installer/AutoBleemWinSetup.exe" "$APP/"
        "$STRIP" "$APP/AutoBleemWinSetup.exe"
        pack "$APP/AutoBleemWinSetup.exe"
    else
        echo "    (no apps/installer/AutoBleemWinSetup.exe in $PRODUCT_DIR - the package has no setup helper)"
    fi
    mkdir -p "$APP/Themes"
    cp -a "$REPO/payload/Themes/." "$APP/Themes/"
    # the two PS1 emulators: a local Windows build, else the site's package of each
    emulator "pcsx-ab" "emu" "${AB_PCSX_WIN_DIST:-}"
    emulator "pcsx-abnxt" "emunxt" "${AB_PCSXNXT_WIN_DIST:-}"
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
cp "$REPO/LICENSE" "$REPO/THIRD_PARTY_NOTICES.md" "$APP/"
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
