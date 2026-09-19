#!/usr/bin/env bash
# Windows development build using MSYS2 UCRT64 (gcc, cmake, ninja, SDL2 packages).
# Run from an MSYS2 UCRT64 shell, or:  C:\msys64\usr\bin\bash.exe -lc "cd /e/Programming/autobleem-develop && ./make_win.sh"
# CHD support builds the vendored libchdr (lib_ableem/third_party/libchdr) on every host. It is passed
# explicitly because a build_win/ configured before it was vendored had AB_ENABLE_CHD=OFF cached, and a
# cached OFF silently outlived the default becoming ON - test_cd_image is what finally noticed.
set -e
cd "$(dirname "$0")"
mkdir -p build_win
cd build_win
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DAB_ENABLE_CHD=ON ../
ninja

# the suite is fast and catches a broken ab_core before the app is ever started
ctest --output-on-failure

# every language file has every _("...") key and nothing malformed (tools/lang_tools.py update fixes the former);
# the console tools under apps/ keep their own lang/ folders, validated against their own sources
python ../tools/lang_tools.py validate
python ../tools/lang_tools.py --src-dir ../apps/pscbios/src --lang-dir ../apps/pscbios/resources/lang validate
python ../tools/lang_tools.py --src-dir ../apps/abflashkit/src --lang-dir ../apps/abflashkit/resources/lang validate
python ../tools/lang_tools.py --src-dir ../apps/updateroms/src --lang-dir ../apps/updateroms/resources/lang validate

# every source is clang-formatted (tools/format.sh rewrites the ones that are not)
bash ../tools/format.sh --check
