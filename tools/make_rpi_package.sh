#!/usr/bin/env bash
#
# Package a Linux appliance cross build - a Raspberry Pi, or the 32-bit PC USB stick - into the tarball
# payload_linux/install.sh expects.
#
#   ./make_rpi.sh                      # 32-bit cross-compile (toolchains/rpi/RPitoolchain.cmake)
#   ./tools/make_rpi_package.sh        # -> build_rpi/autobleem-rpi.tar.gz
#
#   ./make_rpi64.sh                    # 64-bit cross-compile (toolchains/rpi64/RPi64toolchain.cmake)
#   ./tools/make_rpi_package.sh --arch arm64
#                                      # -> build_rpi64/autobleem-rpi-arm64.tar.gz
#
#   docker/run.sh ci/build.sh pcusb    # the PC stick (toolchains/pcusb/PcUsbToolchain.cmake, server only)
#   ./tools/make_rpi_package.sh --arch i386
#                                      # -> build_pcusb/autobleem-pcusb-i386.tar.gz, top dir autobleem-pcusb
#
#   ./tools/make_rpi_package.sh --with-covers   # the cover databases inside (290 MB) instead of fetched
#   ./tools/make_rpi_package.sh --push [user@host]
#                                      # ...and scp it to the Pi's home (default: $AB_PI_HOST, else
#                                      # pi@raspberrypi.local), the way make_psc.sh talks to its build server
#
# The package is payload_linux/ as checked in (install.sh, README.md, system/ for the host-side files, and the
# data-partition tree: Autobleem/ - with pcsx-ab and its plugins already in bin/emu (armhf), bin/emu-arm64
# (arm64) or bin/emu-i386 (the PC stick), put there by pcsx-rearmed-develop's builds - Games/, Apps/) with
# the built parts filled in: the binary and its resources in Autobleem/bin/autobleem, the cover databases in
# Autobleem/bin/db, and payload/Themes as Themes/. install.sh then copies Autobleem/ Themes/ Games/ Apps/
# onto the exFAT partition as they are - the same install.sh serves every architecture, detecting which at
# runtime (dpkg --print-architecture), and the platform (a Pi or a PC) with it.
#
# Copy the tarball to the Pi, unpack it, and run install.sh from inside it. See payload_linux/README.md.
set -euo pipefail

ARCH=armhf
PUSH_TO=""
COVERS_IN_PACKAGE=0   # --with-covers: the 290 MB of cover databases inside (the installer fetches them otherwise)
while [ $# -gt 0 ]; do
    case "$1" in
        --arch)
            ARCH="${2:?--arch needs armhf, arm64 or i386}"; shift 2 ;;
        --with-covers)
            COVERS_IN_PACKAGE=1; shift ;;
        --push)
            PUSH_TO="${AB_PI_HOST:-pi@raspberrypi.local}"
            if [ $# -gt 1 ] && [ "${2#-}" = "$2" ]; then PUSH_TO="$2"; shift; fi
            shift ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

# per architecture: the build dir, the tarball's name and top directory (autobleem-<platform>, what the
# first-boot script and autobleem-update look for), which checked-in emu-* tree is this one's pcsx-ab
case "$ARCH" in
    armhf) BUILD_SUBDIR=build_rpi;   TOP=autobleem-rpi;   TARBALL_NAME=autobleem-rpi.tar.gz;        EMU_SRC_SUBDIR=emu;       MAKE_SCRIPT="./make_rpi.sh" ;;
    arm64) BUILD_SUBDIR=build_rpi64; TOP=autobleem-rpi;   TARBALL_NAME=autobleem-rpi-arm64.tar.gz;  EMU_SRC_SUBDIR=emu-arm64; MAKE_SCRIPT="./make_rpi64.sh" ;;
    i386)  BUILD_SUBDIR=build_pcusb; TOP=autobleem-pcusb; TARBALL_NAME=autobleem-pcusb-i386.tar.gz; EMU_SRC_SUBDIR=emu-i386;  MAKE_SCRIPT="docker/run.sh ci/build.sh pcusb" ;;
    *) echo "--arch takes armhf, arm64 or i386 (got '$ARCH')" >&2; exit 2 ;;
esac

cd "$(dirname "$0")/.."
REPO="$PWD"
PAYLOAD="$REPO/payload_linux"
BUILD_DIR="$REPO/$BUILD_SUBDIR"
STAGE="$BUILD_DIR/package/$TOP"
TARBALL="$BUILD_DIR/$TARBALL_NAME"

[ -f "$BUILD_DIR/autobleem-gui" ] || {
    echo "no $BUILD_DIR/autobleem-gui - run $MAKE_SCRIPT first" >&2
    exit 1
}

echo "==> Staging into $STAGE ($ARCH)"
rm -rf "$BUILD_DIR/package"
mkdir -p "$STAGE"

# the checked-in payload: installer, README, system/ and the data-partition tree
cp -a "$PAYLOAD/." "$STAGE/"

# the arch-specific pcsx-ab tree lives at Autobleem/bin/emu (armhf), emu-arm64 or emu-i386 in the checked-in
# payload - and the same for emunxt (pcsx-abnxt, the next emulator): emunxt, emunxt-arm64, emunxt-i386. The
# staged tree always uses emu/ and emunxt/ on the device, so another architecture's package replaces each
# with its own tree's contents (an emu-* that is not checked in yet - the PC stick until pcsx-ab has been
# built for it - leaves no emu/ at all: install.sh's RetroArch core fallback plays PS1 then).
EMU_ARCH_SUFFIX="${EMU_SRC_SUBDIR#emu}"   # "" for armhf, "-arm64", "-i386"
for emu in emu emunxt; do
    if [ -n "$EMU_ARCH_SUFFIX" ]; then
        rm -rf "$STAGE/Autobleem/bin/$emu"
        if [ -d "$STAGE/Autobleem/bin/$emu$EMU_ARCH_SUFFIX" ]; then
            mv "$STAGE/Autobleem/bin/$emu$EMU_ARCH_SUFFIX" "$STAGE/Autobleem/bin/$emu"
        elif [ "$emu" = emu ]; then
            echo "    (no $PAYLOAD/Autobleem/bin/$emu$EMU_ARCH_SUFFIX - the package ships no pcsx-ab)"
        fi
    fi
    rm -rf "$STAGE/Autobleem/bin/$emu-arm64" "$STAGE/Autobleem/bin/$emu-i386"
done

# the app: the freshly cross-compiled binary plus the resources tree it reads at runtime. The resources come
# from the repo rather than from $BUILD_DIR, which also holds the object files and CMake's own scratch.
APP="$STAGE/Autobleem/bin/autobleem"
mkdir -p "$APP" "$STAGE/Autobleem/bin/db" "$STAGE/Themes"
cp -a "$BUILD_DIR/autobleem-gui" "$APP/"
# packed with UPX (3.1 MB -> 1 MB; unpacked in memory at start, verified on the Pi 400) unless AB_NO_UPX=1 -
# a packed binary is no use to gdb, and make_rpi.sh/make_rpi64.sh --debug's build is never packaged anyway
if [ -z "${AB_NO_UPX:-}" ] && command -v upx >/dev/null 2>&1; then
    echo "==> packing autobleem-gui with upx"
    upx -q --best --lzma "$APP/autobleem-gui"
fi
cp -a "$REPO/src/resources/." "$APP/"

# internal.db is the PlayStation Classic's own game list. An appliance has no built-in games (AB_APPLIANCE) and
# never reads it, so shipping it would only be confusing.
rm -f "$APP/internal.db"

# VERSION: what this package is, for tools/make_rpi_image.sh to name the image after (the build host has no
# git tree, and the binary is for another architecture). Read from the build's own generated version.h so
# it says exactly what the binary's About screen says: the tag, plus the short hash and "-dirty" unless the
# tree is clean and exactly at that tag - so a release is "v2.0.0", a development build "v2.0.0-pre0-a65250f-dirty".
VERSION_H="$BUILD_DIR/generated/core/version.h"
if [ -f "$VERSION_H" ]; then
    ab_version="$(sed -n 's/^constexpr const char \*VERSION = "\([^"]*\)".*/\1/p' "$VERSION_H")"
    ab_hash="$(sed -n 's/^constexpr const char \*GIT_HASH = "\([^"]*\)".*/\1/p' "$VERSION_H")"
    ab_dirty="$(sed -n 's/^constexpr bool GIT_DIRTY = \([a-z]*\);.*/\1/p' "$VERSION_H")"
    if [ "$ab_dirty" = false ] && git -C "$REPO" describe --tags --exact-match HEAD >/dev/null 2>&1; then
        ab_full="$ab_version"
    else
        ab_full="$ab_version${ab_hash:+-$ab_hash}"
        [ "$ab_dirty" = true ] && ab_full="$ab_full-dirty"
    fi
    printf '%s\n' "$ab_full" > "$STAGE/VERSION"
    echo "==> version $ab_full"
else
    echo "    (no $VERSION_H - the package carries no VERSION file; make_rpi_image.sh will name the image without one)"
fi

# themes: the converted theme.json layout, the same ones the console's payload ships
cp -a "$REPO/payload/Themes/." "$STAGE/Themes/"

# cover art databases: not in the package by default since 2026-09-19 - they are 290 MB of the 306, and
# install.sh downloads them from the download repository (CLAUDE.md, "The download repository"; a Pi needs
# the network for its install anyway). --with-covers puts them in: the Docker image's copy
# (AB_COVERS_DB_DIR, see docker/), else the checkout's db/ - git-ignored, so a clean checkout has only
# stubs (or nothing); the installer says so on the Pi rather than failing.
COVERS="${AB_COVERS_DB_DIR:-$REPO/db}"
if [ "$COVERS_IN_PACKAGE" -eq 0 ]; then
    echo "    (cover databases left out - install.sh fetches them; --with-covers includes them)"
elif ls "$COVERS"/covers*.db >/dev/null 2>&1; then
    cp -a "$COVERS"/covers*.db "$STAGE/Autobleem/bin/db/"
else
    echo "    (no db/covers*.db to include - scanned games will have no titles or covers)"
fi

# the empty 'placeholder' files (payload_linux/ and src/resources/music) only exist so git keeps the empty
# directories; they have no business on the Pi
find "$STAGE" -type f -name placeholder -delete

# Best effort: on a Windows build host the executable bit does not stick, which is why install.sh checks for
# the binary with -f rather than -x, chmods what it deploys itself, and is documented as "sudo bash install.sh".
chmod +x "$STAGE/install.sh" "$STAGE/system/"*.sh "$STAGE/Autobleem/rc/"*.sh "$APP/autobleem-gui"          "$STAGE/Autobleem/bin/emu/pcsx-ab" "$STAGE/Autobleem/bin/emunxt/pcsx-ab" 2>/dev/null || true

echo "==> Building $TARBALL"
rm -f "$TARBALL"
tar -czf "$TARBALL" -C "$BUILD_DIR/package" "$TOP"

echo "==> Done: $TARBALL ($(du -h "$TARBALL" | cut -f1))"
if [ -n "$PUSH_TO" ]; then
    echo "==> pushing to $PUSH_TO:~/"
    scp "$TARBALL" "$PUSH_TO:~/"
    cat <<USAGE

  On the Pi ($PUSH_TO), stop the launcher first if it is installed already:

    sudo systemctl stop autobleem
    rm -rf $TOP && tar xzf $(basename "$TARBALL") && cd $TOP
    sudo bash install.sh --retroarch none --no-downloads --yes   # a re-install over a working Pi
    sudo reboot

USAGE
    exit 0
fi
cat <<USAGE

  On the machine ($ARCH - Raspberry Pi OS Lite, or Debian 12 i386 for the PC stick):

    tar xzf $(basename "$TARBALL")
    cd $TOP
    sudo bash install.sh --dry-run  # see what it would do
    sudo bash install.sh

USAGE
