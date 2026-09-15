#!/usr/bin/env bash
# Windows development build using MSYS2 UCRT64 (gcc, cmake, ninja, SDL2 packages).
# Run from an MSYS2 UCRT64 shell, or:  C:\msys64\usr\bin\bash.exe -lc "cd /e/Programming/autobleem-develop && ./make_win.sh"
# CHD support is off because libmamecd is only available in the PSC toolchain.
set -e
cd "$(dirname "$0")"
mkdir -p build_win
cd build_win
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DAB_ENABLE_CHD=OFF ../
ninja
