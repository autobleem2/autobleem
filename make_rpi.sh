#!/usr/bin/env bash
# Cross-compile for a 32-bit Raspberry Pi OS userland, using the Windows-hosted "SysGCC for Raspberry Pi"
# toolchain at C:\sysGCC\raspberry (not the PSC toolchain - see make_psc.sh for that). The result goes into
# build_rpi/; tools/make_rpi_package.sh turns it into the installable tarball (see payload_rpi/README.md).
#
# Run it the way make_win.sh is run, from the MSYS2 UCRT64 shell. The toolchain file names the compilers by
# absolute path, so C:\sysGCC\raspberry\bin deliberately does NOT go on PATH: its rm.exe/mkdir.exe/make.exe
# would shadow the MSYS2 ones and break this script.
set -e
cd "$(dirname "$0")"
rm -rf ./build_rpi
mkdir -p build_rpi
cd build_rpi
cmake -G Ninja -DCMAKE_SYSTEM_PROCESSOR="arm" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=../toolchains/rpi/RPitoolchain.cmake ../
ninja
cd ..
