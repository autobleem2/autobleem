#!/usr/bin/env bash
#
# Package the Raspberry Pi cross build into the tarball payload_rpi/install.sh expects.
#
#   ./make_rpi.sh                      # cross-compile first (toolchains/rpi/RPitoolchain.cmake)
#   ./tools/make_rpi_package.sh        # -> build_rpi/autobleem-rpi.tar.gz
#
# The package is payload_rpi/ as checked in (install.sh, README.md, system/ for the host-side files, and the
# data-partition tree: Autobleem/ - with pcsx-ab and its plugins already in bin/emu, put there by
# pcsx-rearmed-develop's make_rpi.sh - Games/, Apps/) with the built parts filled in: the binary and its
# resources in Autobleem/bin/autobleem, the cover databases in Autobleem/bin/db, and payload/themes as themes/.
# install.sh then copies Autobleem/ themes/ Games/ Apps/ onto the exFAT partition as they are.
#
# Copy the tarball to the Pi, unpack it, and run install.sh from inside it. See payload_rpi/README.md.
set -euo pipefail

cd "$(dirname "$0")/.."
REPO="$PWD"
PAYLOAD="$REPO/payload_rpi"
BUILD_DIR="$REPO/build_rpi"
STAGE="$BUILD_DIR/package/autobleem-rpi"
TARBALL="$BUILD_DIR/autobleem-rpi.tar.gz"

[ -f "$BUILD_DIR/autobleem-gui" ] || {
    echo "no $BUILD_DIR/autobleem-gui - run ./make_rpi.sh first" >&2
    exit 1
}

echo "==> Staging into $STAGE"
rm -rf "$BUILD_DIR/package"
mkdir -p "$STAGE"

# the checked-in payload: installer, README, system/ and the data-partition tree
cp -a "$PAYLOAD/." "$STAGE/"

# the app: the freshly cross-compiled binary plus the resources tree it reads at runtime. The resources come
# from the repo rather than from build_rpi, which also holds the object files and CMake's own scratch.
APP="$STAGE/Autobleem/bin/autobleem"
mkdir -p "$APP" "$STAGE/Autobleem/bin/db" "$STAGE/themes"
cp -a "$BUILD_DIR/autobleem-gui" "$APP/"
cp -a "$REPO/src/resources/." "$APP/"

# internal.db is the PlayStation Classic's own game list. A Pi has no built-in games (AB_PLATFORM_RPI) and
# never reads it, so shipping it would only be confusing.
rm -f "$APP/internal.db"

# themes: the converted theme.json layout, the same ones the console's payload ships
cp -a "$REPO/payload/themes/." "$STAGE/themes/"

# cover art databases. db/ is git-ignored, so a clean checkout has only stubs (or nothing) - the installer
# says so on the Pi rather than failing.
if ls "$REPO/db"/covers*.db >/dev/null 2>&1; then
    cp -a "$REPO"/db/covers*.db "$STAGE/Autobleem/bin/db/"
else
    echo "    (no db/covers*.db to include - scanned games will have no titles or covers)"
fi

# the empty 'placeholder' files (payload_rpi/ and src/resources/music) only exist so git keeps the empty
# directories; they have no business on the Pi
find "$STAGE" -type f -name placeholder -delete

# Best effort: on a Windows build host the executable bit does not stick, which is why install.sh checks for
# the binary with -f rather than -x, chmods what it deploys itself, and is documented as "sudo bash install.sh".
chmod +x "$STAGE/install.sh" "$STAGE/system/"*.sh "$STAGE/Autobleem/rc/"*.sh "$APP/autobleem-gui"          "$STAGE/Autobleem/bin/emu/pcsx-ab" 2>/dev/null || true

echo "==> Building $TARBALL"
rm -f "$TARBALL"
tar -czf "$TARBALL" -C "$BUILD_DIR/package" autobleem-rpi

echo "==> Done: $TARBALL ($(du -h "$TARBALL" | cut -f1))"
cat <<USAGE

  On the Pi (32-bit Raspberry Pi OS Lite):

    tar xzf autobleem-rpi.tar.gz
    cd autobleem-rpi
    sudo bash install.sh --dry-run  # see what it would do
    sudo bash install.sh

USAGE
