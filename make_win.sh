#!/usr/bin/env bash
# Windows development build using MSYS2 UCRT64 (gcc, cmake, ninja, SDL2 packages).
# Run from an MSYS2 UCRT64 shell, or:  C:\msys64\usr\bin\bash.exe -lc "cd /e/Programming/autobleem-develop && ./make_win.sh"
# CHD support builds the vendored libmamecd (lib_ableem/third_party/libmamecd) on every host now.
set -e
cd "$(dirname "$0")"
mkdir -p build_win
cd build_win
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../
ninja

# the suite is fast and catches a broken ab_core before the app is ever started
ctest --output-on-failure
