#!/usr/bin/env bash
# Cross-compile for a 32-bit Raspberry Pi OS userland, using the Windows-hosted "SysGCC for Raspberry Pi"
# toolchain at C:\sysGCC\raspberry (not the PSC toolchain - see make_psc.sh for that). The result goes into
# build_rpi/; tools/make_rpi_package.sh turns it into the installable tarball (see payload_linux/README.md).
#
# Run it the way make_win.sh is run, from the MSYS2 UCRT64 shell. The toolchain file names the compilers by
# absolute path, so C:\sysGCC\raspberry\bin deliberately does NOT go on PATH: its rm.exe/mkdir.exe/make.exe
# would shadow the MSYS2 ones and break this script.
#
#   ./make_rpi.sh            -> build_rpi/autobleem-gui, the shipped binary (-Os, stripped)
#   ./make_rpi.sh --debug    -> build_rpi_dbg/autobleem-gui, with symbols (-O1 -g) for gdb on the Pi:
#                               copy it over the installed one, get a core dump, "gdb autobleem-gui core"
#   ./make_rpi.sh --clean    -> wipe the build dir first (either flag order). Otherwise the build is
#                               incremental: ninja rebuilds what changed, like make_win.sh - a full Pi
#                               build is ~180 targets with sqlite, libchdr and zstd among them.
set -e
cd "$(dirname "$0")"
BUILD_DIR=build_rpi
DEBUG=OFF
CLEAN=no
for arg in "$@"; do
    case "$arg" in
        --debug) BUILD_DIR=build_rpi_dbg; DEBUG=ON ;;
        --clean) CLEAN=yes ;;
        *) echo "usage: $0 [--debug] [--clean]" >&2; exit 2 ;;
    esac
done
if [ "$CLEAN" = yes ]; then
    rm -rf "./$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake -G Ninja -DCMAKE_SYSTEM_PROCESSOR="arm" -DCMAKE_BUILD_TYPE=Release -DAB_RPI_DEBUG=$DEBUG \
  -DCMAKE_TOOLCHAIN_FILE=../toolchains/rpi/RPitoolchain.cmake ../
ninja
cd ..
