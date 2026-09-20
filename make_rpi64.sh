#!/usr/bin/env bash
# Cross-compile for a 64-bit Raspberry Pi OS Lite (Trixie) userland, using the Windows-hosted "SysGCC for
# Raspberry Pi (64-bit)" toolchain at C:\sysGCC\raspberry64 (not the 32-bit toolchain - see make_rpi.sh for
# that, and CLAUDE.md's "Raspberry Pi port" section for both). The result goes into build_rpi64/;
# tools/make_rpi_package.sh --arch arm64 turns it into the installable tarball (see payload_linux/README.md).
#
# Run it the way make_rpi.sh is run, from the MSYS2 UCRT64 shell. The toolchain file names the compilers by
# absolute path, so C:\sysGCC\raspberry64\bin deliberately does NOT go on PATH: its rm.exe/mkdir.exe/make.exe
# would shadow the MSYS2 ones and break this script.
#
#   ./make_rpi64.sh            -> build_rpi64/autobleem-gui, the shipped binary (-Os, stripped)
#   ./make_rpi64.sh --debug    -> build_rpi64_dbg/autobleem-gui, with symbols (-O1 -g) for gdb on the Pi:
#                                  copy it over the installed one, get a core dump, "gdb autobleem-gui core"
set -e
cd "$(dirname "$0")"
BUILD_DIR=build_rpi64
DEBUG=OFF
CLEAN=no
for arg in "$@"; do
    case "$arg" in
        --debug) BUILD_DIR=build_rpi64_dbg; DEBUG=ON ;;
        --clean) CLEAN=yes ;;
        *) echo "usage: $0 [--debug] [--clean]" >&2; exit 2 ;;
    esac
done
if [ "$CLEAN" = yes ]; then
    rm -rf "./$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake -G Ninja -DCMAKE_SYSTEM_PROCESSOR="aarch64" -DCMAKE_BUILD_TYPE=Release -DAB_RPI_DEBUG=$DEBUG \
  -DCMAKE_TOOLCHAIN_FILE=../toolchains/rpi64/RPi64toolchain.cmake ../
ninja
cd ..
