#!/usr/bin/env bash
# The UpdateRoms folder for a stick: build_win/UpdateRoms/ with UpdateRoms.exe, the MSYS2 DLLs it needs
# (ldd says which - SDL2 and its image/mixer/ttf libraries with every codec they were built with, ~50 MB;
# a static build that leaves the codecs out is the plan's follow-up), its lang/ and README.txt. Copy the
# folder to the stick's root; the tool finds the stick from where it sits.
#
# Run from the MSYS2 UCRT64 shell after make_win.sh. Set AB_NO_UPX=1 to skip packing.
set -e
cd "$(dirname "$0")/.."
EXE=build_win/apps/updateroms/UpdateRoms.exe
[ -f "$EXE" ] || { echo "build first: ./make_win.sh" >&2; exit 1; }

OUT=build_win/UpdateRoms
rm -rf "$OUT"
mkdir -p "$OUT"
cp "$EXE" "$OUT/"
cp -r apps/updateroms/resources/. "$OUT/"
# every DLL the exe pulls from the MSYS2 tree (the system's own, in C:/Windows, are not ours to ship)
ldd "$EXE" | awk '/ucrt64/ {print $3}' | while read -r dll; do cp "$dll" "$OUT/"; done

if [ -z "${AB_NO_UPX:-}" ] && command -v upx >/dev/null 2>&1; then
    upx -q --best --lzma "$OUT/UpdateRoms.exe" >/dev/null || true
fi
echo "UpdateRoms folder: $OUT ($(du -sh "$OUT" | cut -f1)) - copy it to the stick's root"
