#!/usr/bin/env bash
# The UpdateRoms folder for a stick: build_win/UpdateRoms/ with UpdateRoms.exe and README.txt, nothing else -
# the exe is a plain Win32 program linked statically (no SDL, no MSYS2 DLLs), built here as Release, stripped
# and UPX-packed. Copy the folder to the stick's root; the tool finds the stick from where it sits.
#
# Run from the MSYS2 UCRT64 shell. The Release build goes into build_updateroms/ (incremental; --clean
# wipes it), separate from build_win/'s Debug one. Set AB_NO_UPX=1 to skip packing.
set -e
cd "$(dirname "$0")/.."

BUILD=build_updateroms
if [ "${1:-}" = "--clean" ]; then
    rm -rf "$BUILD"
fi
mkdir -p "$BUILD"
cmake -G Ninja -S . -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DAB_BUILD_TESTS=OFF -DAB_ENABLE_CHD=ON >/dev/null
ninja -C "$BUILD" updateroms

OUT=build_win/UpdateRoms
rm -rf "$OUT"
mkdir -p "$OUT"
cp "$BUILD/apps/updateroms/UpdateRoms.exe" "$OUT/"
cp -r apps/updateroms/resources/. "$OUT/"
strip "$OUT/UpdateRoms.exe"
if [ -z "${AB_NO_UPX:-}" ] && command -v upx >/dev/null 2>&1; then
    upx -q --best --lzma "$OUT/UpdateRoms.exe" >/dev/null || true
fi
echo "UpdateRoms folder: $OUT ($(du -sh "$OUT" | cut -f1)) - copy it to the stick's root"
ls -la "$OUT"
