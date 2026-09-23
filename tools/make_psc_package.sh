#!/usr/bin/env bash
# Package a PlayStation Classic build, twice over: the release zip - the USB stick's root, ready to unzip
# onto an empty stick - and autobleem-psc-<version>.tar.gz, the stick's initial file system as the PC
# installer lays it down: the same tree without RetroArch/, which the installer adds on request from the
# download repository's psc/ packs (RetroArch, cores, libs, apps, the BIOS list), and without the cover
# databases, which the installer fetches from the repository's db/ (the zip carries them - the console
# itself has no network). The layout is the one every AutoBleem release has shipped (the 2020 BUILD.sh
# on the build server did the same by hand):
#
#   <zip root>/                        payload/ as checked in: the exploit folder, Autobleem/{rc,lib,start.sh},
#                                      Apps/, Games/, Themes/, RetroArch/{bin,bios,roms}, Docs/ (the manuals)
#   Autobleem/bin/autobleem/           the launcher + src/resources (config.ini, internal.db, lang/, ...)
#   Autobleem/bin/db/                  coversJ/P/U.db
#   Autobleem/lib/libs.tar.gz          the shared libraries rc/autobleem.sh unpacks to /tmp/lib at boot: the
#                                      checked-in archive with the SDL2 family replaced by the libraries the
#                                      binary was just built against (AB_PSC_TOOLCHAIN/sdl2/lib - the Docker
#                                      image's build; kept as is when there is no such directory)
#   Apps/pscbios/, Apps/abflashkit/    the console tools built alongside, over their resources
#   VERSION                            what this package is (the tag, plus hash and -dirty unless clean at it),
#                                      read from the build's generated version.h as the Pi package does
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
[ -n "$VERSION" ] || VERSION="$(git describe --tags --exclude nightly --always --dirty 2>/dev/null || echo dev)"
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
TARBALL="$REPO/$OUT/autobleem-psc-$VERSION.tar.gz"
echo "==> staging into $STAGE"
rm -rf "$BUILD_DIR/package"
mkdir -p "$STAGE" "$OUT"

# the checked-in USB tree
cp -a "$REPO/payload/." "$STAGE/"

# the launcher and its resources
APP="$STAGE/Autobleem/bin/autobleem"
mkdir -p "$APP" "$STAGE/Autobleem/bin/db"
cp -a "$REPO/src/resources/." "$APP/"
cp "$REPO/LICENSE" "$REPO/THIRD_PARTY_NOTICES.md" "$APP/"  # the GPL and the notices travel with the binary
cp -a "$BUILD_DIR/autobleem-gui" "$APP/autobleem-gui"
# absplash: the full-screen picture the launch scripts show around RetroArch (src/tools/absplash.cpp;
# its pictures are src/resources/splash/, copied with the resources above)
cp -a "$BUILD_DIR/absplash" "$APP/absplash"
# abfatflag: the stick's dirty flag, for rc/checkstick.sh and the standby in rc/selection.sh
cp -a "$BUILD_DIR/abfatflag" "$APP/abfatflag"

# the virtual gamepad (docs/virtual-gamepad-plan.md): the daemon that reads the pads and the shim an
# App is preloaded with. Both are optional at run time - rc/app_env.sh checks for them and an App runs
# without them as it always did - so a build that somehow lacks them is a warning, not a failure.
ABPAD="$STAGE/Autobleem/bin/abpad"
mkdir -p "$ABPAD"
if [ -f "$BUILD_DIR/apps/abpad/abpadd" ] && [ -f "$BUILD_DIR/apps/abpad/libabpad.so" ]; then
    cp -a "$BUILD_DIR/apps/abpad/abpadd" "$BUILD_DIR/apps/abpad/libabpad.so" "$ABPAD/"
else
    echo "WARNING: no abpad in $BUILD_DIR/apps/abpad - Apps will run without the virtual gamepad" >&2
fi


# the console tools (pscbios, abflashkit) are autobleem2/autobleem-console-tools' own release since the
# launcher took core as a submodule (2026-09-23): autobleem-appliance's assemble-psc.sh puts them on the
# stick; this zip carries them only when a build tree still has them
PACK=("$APP/autobleem-gui" "$APP/absplash") # not abfatflag: 10 KB, which upx refuses (NotCompressibleException)
for tool in pscbios abflashkit; do
    [ -f "$BUILD_DIR/apps/$tool/$tool" ] && [ -d "$REPO/apps/$tool/resources" ] || continue
    mkdir -p "$STAGE/Apps/$tool"
    cp -a "$REPO/apps/$tool/resources/." "$STAGE/Apps/$tool/"
    cp -a "$BUILD_DIR/apps/$tool/$tool" "$STAGE/Apps/$tool/$tool"
    PACK+=("$STAGE/Apps/$tool/$tool") # a fresh build; payload/Apps' own copies come packed already
done

if [ -z "${AB_NO_UPX:-}" ] && command -v upx >/dev/null 2>&1; then
    echo "==> packing with upx"
    for bin in "${PACK[@]}"; do
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

# VERSION, from the build's own version.h (see tools/make_rpi_package.sh for the rule)
VERSION_H="$BUILD_DIR/generated/core/version.h"
if [ -f "$VERSION_H" ]; then
    ab_version="$(sed -n 's/^constexpr const char \*VERSION = "\([^"]*\)".*/\1/p' "$VERSION_H")"
    ab_hash="$(sed -n 's/^constexpr const char \*GIT_HASH = "\([^"]*\)".*/\1/p' "$VERSION_H")"
    ab_dirty="$(sed -n 's/^constexpr bool GIT_DIRTY = \([a-z]*\);.*/\1/p' "$VERSION_H")"
    if [ "$ab_dirty" = false ] && git -C "$REPO" describe --tags --exclude nightly --exact-match HEAD >/dev/null 2>&1; then
        ab_full="$ab_version"
    else
        ab_full="$ab_version${ab_hash:+-$ab_hash}"
        [ "$ab_dirty" = true ] && ab_full="$ab_full-dirty"
    fi
    printf '%s\n' "$ab_full" > "$STAGE/VERSION"
    echo "==> version $ab_full"
else
    echo "    (no $VERSION_H - the package carries no VERSION file)"
fi

# git's directory keepers have no business on a stick; the executable bit does not survive a zip made on
# Windows, which is why rc/autobleem.sh chmods what it runs, but from here it can be right
find "$STAGE" -type f -name placeholder -delete
chmod +x "$APP/autobleem-gui" "$APP/absplash" "$APP/abfatflag" "$STAGE/Apps/pscbios/pscbios" "$STAGE/Apps/abflashkit/abflashkit" \
         "$STAGE"/Autobleem/*.sh "$STAGE"/Autobleem/rc/*.sh "$STAGE"/Apps/*/*.sh 2>/dev/null || true

echo "==> $ZIP"
rm -f "$ZIP"
(cd "$STAGE" && zip -r -9 -q "$ZIP" .)
echo "    $(du -h "$ZIP" | cut -f1), $(unzip -l "$ZIP" | tail -1 | awk '{print $2}') files"

# the installer's tarball: the same stick, RetroArch left to the installer's optional step and the cover
# databases to its download. Modes and ownership are what the console wants (a tar keeps the executable
# bit a zip from Windows loses).
echo "==> $TARBALL"
rm -f "$TARBALL"
(cd "$STAGE" && tar -czf "$TARBALL" --owner=0 --group=0 --exclude=./RetroArch --exclude=./Autobleem/bin/db .)
echo "    $(du -h "$TARBALL" | cut -f1), $(tar -tzf "$TARBALL" | grep -vc '/$') files, no RetroArch/, no cover databases"
