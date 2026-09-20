#!/usr/bin/env bash
# Windows development build using MSYS2 UCRT64 (gcc, cmake, ninja, SDL2 packages).
# Run from an MSYS2 UCRT64 shell, or:  C:\msys64\usr\bin\bash.exe -lc "cd /e/Programming/autobleem-develop && ./make_win.sh"
#
#   ./make_win.sh              the dev build (AB_TARGET=dev) into build_win/: a window, keyboard-as-pad, the
#                              splash instead of an emulator - what tools/win_drive.ps1 drives; then the
#                              tests, the language check and the format check
#   ./make_win.sh --product    the Windows product (AB_TARGET=win) into build_win_product/, Release: what
#                              the installer ships. The tests run there too; the checks are the dev build's.
#
# CHD support builds the vendored libchdr (lib_ableem/third_party/libchdr) on every host. It is passed
# explicitly because a build_win/ configured before it was vendored had AB_ENABLE_CHD=OFF cached, and a
# cached OFF silently outlived the default becoming ON - test_cd_image is what finally noticed.
set -e
cd "$(dirname "$0")"

if [ "$1" = "--product" ]; then
    mkdir -p build_win_product
    cd build_win_product
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DAB_TARGET=win -DAB_ENABLE_CHD=ON ../
    ninja
    ctest --output-on-failure
    exit 0
fi

mkdir -p build_win
cd build_win
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DAB_ENABLE_CHD=ON -DAB_TARGET=dev ../
ninja

# the suite is fast and catches a broken ab_core before the app is ever started
ctest --output-on-failure

# every language file has every _("...") key and nothing malformed (tools/lang_tools.py update fixes the former);
# the console tools under apps/ keep their own lang/ folders, validated against their own sources
python ../tools/lang_tools.py validate
python ../tools/lang_tools.py --src-dir ../apps/pscbios/src --lang-dir ../apps/pscbios/resources/lang validate
python ../tools/lang_tools.py --src-dir ../apps/abflashkit/src --lang-dir ../apps/abflashkit/resources/lang validate

# every source is clang-formatted (tools/format.sh rewrites the ones that are not)
bash ../tools/format.sh --check
