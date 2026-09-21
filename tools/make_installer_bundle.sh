#!/usr/bin/env bash
# The installer for a release: AutoBleemInstaller-<version>.zip with AutoBleemInstaller.exe (a plain Win32
# program linked statically - no SDL, no MSYS2 DLLs - built as Release, stripped, UPX-packed), README.txt
# and the console package it installs from, autobleem-psc-<version>.tar.gz. The exe looks for the package
# next to itself, so the two stay together.
#
#   tools/make_installer_bundle.sh <autobleem-psc-<version>.tar.gz> [--out dist/installer] [--clean]
#   tools/make_installer_bundle.sh <autobleem-psc-<version>.tar.gz> --exe dist/win/AutoBleemInstaller.exe
#
# Run from the MSYS2 UCRT64 shell. The Release build goes into build_installer/ (incremental; --clean
# wipes it), separate from build_win/'s Debug one. Set AB_NO_UPX=1 to skip packing. The package comes
# from the build server's ci/build.sh psc (dist/psc/), or the download repository's pre-release. --exe
# takes an installer already built (ci/build.sh win leaves one in dist/win/, stripped and packed) and
# builds nothing - the workflow's site job, any Linux host with zip or python3.
set -e
cd "$(dirname "$0")/.."

PACKAGE=""
OUT=dist/installer
CLEAN=0
EXE=""
while [ $# -gt 0 ]; do
    case "$1" in
        --out) OUT="$2"; shift 2 ;;
        --clean) CLEAN=1; shift ;;
        --exe) EXE="$2"; shift 2 ;;
        *) PACKAGE="$1"; shift ;;
    esac
done
[ -n "$PACKAGE" ] && [ -f "$PACKAGE" ] || { echo "usage: $0 <autobleem-psc-<version>.tar.gz> [--out DIR] [--clean]" >&2; exit 2; }
NAME=$(basename "$PACKAGE")
VERSION=${NAME#autobleem-psc-}
VERSION=${VERSION%.tar.gz}

BUILD=build_installer
if [ -n "$EXE" ]; then
    [ -f "$EXE" ] || { echo "no $EXE" >&2; exit 2; }
else
    [ "$CLEAN" = 1 ] && rm -rf "$BUILD"
    mkdir -p "$BUILD"
    cmake -G Ninja -S . -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DAB_BUILD_TESTS=OFF -DAB_ENABLE_CHD=ON >/dev/null
    ninja -C "$BUILD" installer
    EXE="$BUILD/apps/installer/AutoBleemInstaller.exe"
fi

STAGE="$BUILD/bundle/AutoBleemInstaller"
rm -rf "$BUILD/bundle"
mkdir -p "$STAGE" "$OUT"
cp "$EXE" "$STAGE/AutoBleemInstaller.exe"
cp -r apps/installer/resources/. "$STAGE/"
cp "$PACKAGE" "$STAGE/$NAME"
# a fresh build is stripped and packed here; one from --exe already is (strip and upx refuse a packed one)
if [ "$EXE" = "$BUILD/apps/installer/AutoBleemInstaller.exe" ]; then
    strip "$STAGE/AutoBleemInstaller.exe"
    if [ -z "${AB_NO_UPX:-}" ] && command -v upx >/dev/null 2>&1; then
        upx -q --best --lzma "$STAGE/AutoBleemInstaller.exe" >/dev/null || true
    fi
fi
ZIP="$PWD/$OUT/AutoBleemInstaller-$VERSION.zip"
rm -f "$ZIP"
# zip when there is one (the image), python's zipfile otherwise (MSYS2 has python, not always zip)
if command -v zip >/dev/null 2>&1; then
    (cd "$BUILD/bundle" && zip -r -9 -q "$ZIP" AutoBleemInstaller)
else
    python3 - "$BUILD/bundle" "$ZIP" <<'PY'
import os, sys, zipfile
root, out = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for base, _dirs, files in os.walk(root):
        for f in sorted(files):
            p = os.path.join(base, f)
            z.write(p, os.path.relpath(p, root).replace(os.sep, "/"))
PY
fi
echo "==> $ZIP ($(du -h "$ZIP" | cut -f1)): AutoBleemInstaller.exe ($(du -h "$STAGE/AutoBleemInstaller.exe" | cut -f1)), README.txt, $NAME"
