#!/usr/bin/env bash
# Package a PlayStation Classic build into the release zip: the USB stick's root, ready to unzip onto an
# empty stick (or to hand to tools/install_autobleem.py). The layout is the one every AutoBleem release has
# shipped (the 2020 BUILD.sh on the build server did the same by hand):
#
#   <zip root>/                        payload/ as checked in: the exploit folder, Autobleem/{rc,lib,start.sh},
#                                      Apps/, Games/, themes/, the release notes
#   Autobleem/bin/autobleem/           the launcher + src/resources (config.ini, internal.db, lang/, ...)
#   Autobleem/bin/db/                  coversJ/P/U.db
#   Autobleem/lib/libs.tar.gz          the shared libraries rc/autobleem.sh unpacks to /tmp/lib at boot: the
#                                      checked-in archive with the SDL2 family replaced by the libraries the
#                                      binary was just built against (AB_PSC_TOOLCHAIN/sdl2/lib - the Docker
#                                      image's build; kept as is when there is no such directory)
#   Apps/pscbios/, Apps/abflashkit/    the console tools built alongside, over their resources
#
#   tools/make_psc_package.sh [--build-dir build_psc] [--out dist/psc] [--version v2.0.0]
#
# UPX packs the three binaries unless AB_NO_UPX=1. The cover databases come from AB_COVERS_DB_DIR (the
# image's /opt/autobleem/db), else db/ - which is git-ignored and may hold tools/make_usb.py's stubs; a stub
# is refused, a release must not ship without the real ones.
set -euo pipefail
cd "$(dirname "$0")/.."
REPO="$PWD"

BUILD_DIR=build_psc
OUT=dist/psc
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
[ -f "$BUILD_DIR/autobleem-gui" ] || { echo "no $BUILD_DIR/autobleem-gui - build the console target first" >&2; exit 1; }

COVERS="${AB_COVERS_DB_DIR:-$REPO/db}"
for f in coversJ.db coversP.db coversU.db; do
    if [ ! -f "$COVERS/$f" ] || [ "$(stat -c %s "$COVERS/$f")" -lt 1000000 ]; then
        echo "$COVERS/$f is missing or a stub - a release needs the real cover databases (AB_COVERS_DB_DIR)" >&2
        exit 1
    fi
done

STAGE="$BUILD_DIR/package/AutoBleem-$VERSION"
ZIP="$REPO/$OUT/autobleem-psc-$VERSION.zip"
echo "==> staging into $STAGE"
rm -rf "$BUILD_DIR/package"
mkdir -p "$STAGE" "$OUT"

# the checked-in USB tree
cp -a "$REPO/payload/." "$STAGE/"

# the launcher and its resources
APP="$STAGE/Autobleem/bin/autobleem"
mkdir -p "$APP" "$STAGE/Autobleem/bin/db"
cp -a "$REPO/src/resources/." "$APP/"
cp -a "$BUILD_DIR/autobleem-gui" "$APP/autobleem-gui"
# absplash: the full-screen picture the launch scripts show around RetroArch (src/tools/absplash.cpp;
# its pictures are src/resources/splash/, copied with the resources above)
cp -a "$BUILD_DIR/absplash" "$APP/absplash"

# the console tools, each over its resources (what make_psc.sh copies into payload/Apps by hand)
for tool in pscbios abflashkit; do
    mkdir -p "$STAGE/Apps/$tool"
    cp -a "$REPO/apps/$tool/resources/." "$STAGE/Apps/$tool/"
    cp -a "$BUILD_DIR/apps/$tool/$tool" "$STAGE/Apps/$tool/$tool"
done

if [ -z "${AB_NO_UPX:-}" ] && command -v upx >/dev/null 2>&1; then
    echo "==> packing with upx"
    for bin in "$APP/autobleem-gui" "$APP/absplash" "$STAGE/Apps/pscbios/pscbios" "$STAGE/Apps/abflashkit/abflashkit"; do
        upx -q --best --lzma "$bin" >/dev/null
    done
fi

# the cover databases
cp -a "$COVERS"/coversJ.db "$COVERS"/coversP.db "$COVERS"/coversU.db "$STAGE/Autobleem/bin/db/"

# libs.tar.gz: the SDL2 family the binary was built against replaces the archive's, everything else in the
# archive (libiconv, libogg, libvorbis*) stays; macOS resource forks (._*) and the duplicate "name 2"
# entries an old Finder left in it are dropped
SDL_LIB="${AB_PSC_TOOLCHAIN:-/opt/psc}/sdl2/lib"
if ls "$SDL_LIB"/libSDL2-2.0.so.0.* >/dev/null 2>&1; then
    echo "==> libs.tar.gz with the SDL2 family from $SDL_LIB"
    LIBS="$REPO/$BUILD_DIR/package/libs"
    ARCHIVE="$REPO/$STAGE/Autobleem/lib/libs.tar.gz"
    rm -rf "$LIBS"; mkdir -p "$LIBS"
    tar -xzf "$REPO/payload/Autobleem/lib/libs.tar.gz" -C "$LIBS" --exclude='._*' --exclude='* *' --exclude='libSDL2*'
    cp -P "$SDL_LIB"/libSDL2*.so* "$LIBS/"
    (cd "$LIBS" && tar -czf "$ARCHIVE" --owner=0 --group=0 .)
    echo "    $(tar -tzf "$ARCHIVE" | grep -c '\.so') libraries: $(tar -tzf "$ARCHIVE" | grep -E '\.so\.[0-9]+\.[0-9]+' | sed 's|^\./||' | tr '\n' ' ')"
else
    echo "==> libs.tar.gz kept as checked in (no $SDL_LIB)"
fi

# git's directory keepers have no business on a stick; the executable bit does not survive a zip made on
# Windows, which is why rc/autobleem.sh chmods what it runs, but from here it can be right
find "$STAGE" -type f -name placeholder -delete
chmod +x "$APP/autobleem-gui" "$APP/absplash" "$STAGE/Apps/pscbios/pscbios" "$STAGE/Apps/abflashkit/abflashkit" \
         "$STAGE"/Autobleem/*.sh "$STAGE"/Autobleem/rc/*.sh "$STAGE"/Apps/*/*.sh 2>/dev/null || true

echo "==> $ZIP"
rm -f "$ZIP"
(cd "$STAGE" && zip -r -9 -q "$ZIP" .)
echo "    $(du -h "$ZIP" | cut -f1), $(unzip -l "$ZIP" | tail -1 | awk '{print $2}') files"
