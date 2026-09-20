#!/usr/bin/env bash
# Pack the RetroArch cores and bundles for one appliance architecture into a single tarball for the download
# repository (CLAUDE.md, "The download repository") - nothing is compiled: this downloads exactly what
# payload_linux/install.sh's download_retroarch_content() fetches from buildbot.libretro.com, once, so an
# install gets one file from our server instead of ~130 requests to libretro's.
#
#   ci/build_cores.sh armhf            # buildbot's linux/armhf nightly
#   ci/build_cores.sh arm64            # linux/aarch64 (buildbot's name for it)
#   ci/build_cores.sh i386             # linux/x86 (the PC stick)
#   ci/build_cores.sh all
#
# Output: build_cores/dist/cores-<arch>-<YYYYMMDD>.tar.gz (+ .sha256), laid out as the RetroArch tree the
# installer unpacks it into (cores/, info/, assets/, autoconfig/, database/rdb, database/cursors, cheats/,
# overlays/, shaders/) plus cores.manifest (the date, then every core with its size and sha256). Cores a
# Pi cannot run are left out (SKIP_CORES - the PC stick takes every one). Runs anywhere with wget, unzip and sha256sum -
# docker/run.sh is not needed but works.
#
#   AB_CORES_DATE=YYYYMMDD   name the tarball for that date (default: today)
set -euo pipefail
cd "$(dirname "$0")/.."
WORK="$PWD/build_cores"
DIST="$WORK/dist"
BASE=https://buildbot.libretro.com
DATE="${AB_CORES_DATE:-$(date -u +%Y%m%d)}"

# cores no Pi build of RetroArch can use, or that are known to break there (picodrive's Cyclone core
# segfaulted on the Pi 400; resources/platform/rpi.cores.cfg prefers Genesis Plus GX anyway)
SKIP_CORES="picodrive"

usage() { sed -n '2,19p' "$0" | sed 's/^# \{0,1\}//'; exit 1; }
banner() { echo; echo "==> $*"; }

[ $# -eq 1 ] || usage
case "$1" in armhf|arm64|i386|all) ;; *) usage ;; esac

#*******************************
# build_one
#*******************************
build_one() {
    local arch="$1" ra_arch skip="$SKIP_CORES"
    case "$arch" in armhf) ra_arch=armhf ;; arm64) ra_arch=aarch64 ;; i386) ra_arch=x86; skip="" ;; esac
    local cores_url="$BASE/nightly/linux/$ra_arch/latest"
    local stage="$WORK/stage-$arch" tmp="$WORK/tmp-$arch"
    local out="$DIST/cores-$arch-$DATE.tar.gz"
    rm -rf "$stage" "$tmp"
    mkdir -p "$stage/cores" "$tmp" "$DIST"

    banner "$arch: the core list ($cores_url)"
    wget -q -O "$tmp/.index-extended" "$cores_url/.index-extended"
    local total count=0 skipped=0 zip name
    total="$(grep -c . "$tmp/.index-extended")"
    banner "$arch: $total cores"
    while read -r _date _crc zip; do
        [ -n "$zip" ] || continue
        name="${zip%_libretro.so.zip}"
        count=$((count + 1))
        if printf '%s\n' $skip | grep -qx "$name"; then
            echo "    [$count/$total] $zip - skipped"
            skipped=$((skipped + 1))
            continue
        fi
        printf '    [%3d/%3d] %s\n' "$count" "$total" "$zip"
        wget -q -O "$tmp/$zip" "$cores_url/$zip"
        unzip -oq "$tmp/$zip" -d "$stage/cores"
        rm -f "$tmp/$zip"
    done < "$tmp/.index-extended"

    # bundle -> where it unpacks, the same table as install.sh's download_retroarch_content()
    local bundle dest
    for bundle in info:info assets:assets autoconfig:autoconfig database-rdb:database/rdb \
                  database-cursors:database/cursors cheats:cheats overlays:overlays shaders_glsl:shaders; do
        dest="${bundle#*:}"; bundle="${bundle%%:*}"
        banner "$arch: $bundle -> $dest/"
        mkdir -p "$stage/$dest"
        wget -q -O "$tmp/$bundle.zip" "$BASE/assets/frontend/$bundle.zip"
        unzip -oq "$tmp/$bundle.zip" -d "$stage/$dest"
        rm -f "$tmp/$bundle.zip"
    done

    banner "$arch: manifest"
    # cores.manifest lands in RetroArch/ on the Pi: the date on the first line, then one core per line
    {
        echo "# AutoBleem cores tarball $arch $DATE - name size sha256"
        (cd "$stage/cores" && for f in *_libretro.so; do
            printf '%s %s %s\n' "$f" "$(stat -c %s "$f")" "$(sha256sum "$f" | cut -d' ' -f1)"
        done)
    } > "$stage/cores.manifest"
    echo "    $(grep -vc '^#' "$stage/cores.manifest") cores packed, $skipped skipped, $(du -sh "$stage" | cut -f1) unpacked"

    banner "$arch: $out"
    tar -C "$stage" --owner=0 --group=0 -czf "$out" .
    (cd "$DIST" && sha256sum "$(basename "$out")" > "$(basename "$out").sha256")
    ls -la "$out"
    rm -rf "$tmp"
}

if [ "$1" = all ]; then
    build_one armhf
    build_one arm64
    build_one i386
else
    build_one "$1"
fi
banner "done: $DIST"
