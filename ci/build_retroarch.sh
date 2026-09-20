#!/usr/bin/env bash
# Cross-build RetroArch for the Raspberry Pi - and the 32-bit PC stick - inside the build image, as a tarball
# the appliance installer unpacks over / instead of building from source (CLAUDE.md, "The download repository"):
#
#   docker/run.sh ci/build_retroarch.sh armhf            # the newest v* tag on github.com
#   docker/run.sh ci/build_retroarch.sh arm64 v1.22.2    # that tag
#   docker/run.sh ci/build_retroarch.sh i386             # the PC stick (i686, desktop OpenGL added to GLES)
#   docker/run.sh ci/build_retroarch.sh win64            # the Windows product: nothing compiled - libretro's own
#                                                        # x86_64 build (RetroArch.7z) repacked as a tarball
#   docker/run.sh ci/build_retroarch.sh all              # every architecture
#
# Output: build_retroarch/dist/retroarch-<tag>-<arch>.tar.gz (+ .sha256) - `make DESTDIR=... install` of
# the same ./configure as payload_linux/install.sh's source build (KMS/EGL/GLES, udev, ALSA, SDL2, networking;
# no X11/Wayland/Qt/ffmpeg), plus two files under usr/local/share/autobleem/: retroarch.version (the tag -
# install.sh's stamp) and retroarch.depends (the runtime packages, one per line, Bookworm names).
#
# Built against the image's Bookworm multiarch libraries, so it runs on Bookworm and Trixie Raspberry Pi OS:
# a binary linked on the older glibc loads on the newer, and every library it needs keeps its soname across
# the two releases (FLAC does not - libFLAC.so.12 vs .14 - so it is left out; RetroArch only used it for
# playing FLAC files in its audio mixer). armhf targets armv7-a + NEON (Pi 2 and up), like the launcher;
# i386 a plain i686 (no SSE2), Debian's own baseline, like the launcher's pcusb build - and the PC stick is
# Bookworm i386 for good (no Trixie i386 kernel), so the soname question does not arise there.
#
#   AB_JOBS=N   parallel jobs (default: nproc)
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT="$PWD"
JOBS="${AB_JOBS:-$(nproc)}"
WORK="$ROOT/build_retroarch"
DIST="$WORK/dist"

usage() { sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 1; }
banner() { echo; echo "==> $*"; }

[ $# -ge 1 ] || usage
ARCH="$1"
TAG="${2:-}"
case "$ARCH" in armhf|arm64|i386|win64|all) ;; *) usage ;; esac

# the newest release tag, as install.sh finds it
if [ -z "$TAG" ]; then
    TAG="$(git ls-remote --tags --refs https://github.com/libretro/RetroArch.git \
            | awk -F/ '{print $NF}' | grep -E '^v[0-9]+\.[0-9]+(\.[0-9]+)?$' | sort -V | tail -1)"
    [ -n "$TAG" ] || { echo "cannot find the latest RetroArch tag on github.com" >&2; exit 1; }
fi
banner "RetroArch $TAG"

#*******************************
# build_win64
#*******************************
# The Windows product runs libretro's own build: its RetroArch.7z (the portable archive - the setup exe asks
# for administrator rights, which a per-user install has not got) fetched from buildbot's stable folder for
# the tag, unpacked with 7z, its RetroArch-Win64/ top folder stripped, a VERSION file added, and tarred as
# what AutoBleemWinSetup unpacks into <data>/RetroArch/bin (win/retroarch on the site). The cores are a
# separate pack (ci/build_cores.sh win64).
build_win64() {
    local version="${TAG#v}" stage="$WORK/stage-win64" tmp="$WORK/tmp-win64"
    local out="$DIST/retroarch-win64-$version.tar.gz"
    local url="https://buildbot.libretro.com/stable/$version/windows/x86_64/RetroArch.7z"
    local sevenzip
    sevenzip="$(command -v 7z || command -v 7za || command -v 7zr || true)"
    [ -n "$sevenzip" ] || { echo "no 7z on this machine (p7zip-full) - cannot unpack RetroArch.7z" >&2; exit 1; }
    rm -rf "$stage" "$tmp"
    mkdir -p "$stage" "$tmp" "$DIST"
    banner "win64: $url"
    wget -q -O "$tmp/RetroArch.7z" "$url"
    "$sevenzip" x -y -o"$tmp" "$tmp/RetroArch.7z" >/dev/null
    [ -f "$tmp/RetroArch-Win64/retroarch.exe" ] || { echo "no RetroArch-Win64/retroarch.exe in the archive" >&2; exit 1; }
    mv "$tmp/RetroArch-Win64/"* "$stage/"
    echo "$version" > "$stage/VERSION"
    banner "win64: $out"
    tar -C "$stage" --owner=0 --group=0 -czf "$out" .
    (cd "$DIST" && sha256sum "$(basename "$out")" > "$(basename "$out").sha256")
    ls -la "$out"
    rm -rf "$tmp"
}

if [ "$ARCH" = win64 ]; then
    mkdir -p "$DIST"
    build_win64
    banner "done: $DIST"
    exit 0
fi

SRC="$WORK/RetroArch-$TAG"
if [ ! -d "$SRC/.git" ]; then
    rm -rf "$SRC"
    git clone -q --depth 1 --branch "$TAG" https://github.com/libretro/RetroArch.git "$SRC"
fi

#*******************************
# build_one
#*******************************
build_one() {
    # triplet names the compiler, multiarch Debian's library directory - the same for ARM, not for i386
    # (i686-linux-gnu-gcc, /usr/lib/i386-linux-gnu)
    local arch="$1" triplet multiarch cflags
    case "$arch" in
        armhf) triplet=arm-linux-gnueabihf; multiarch=$triplet;       cflags="-O2 -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard" ;;
        arm64) triplet=aarch64-linux-gnu;   multiarch=$triplet;       cflags="-O2" ;;
        i386)  triplet=i686-linux-gnu;      multiarch=i386-linux-gnu; cflags="-O2 -march=i686 -mtune=generic" ;;
    esac
    # a PC's Mesa drivers speak desktop OpenGL, which more cores and shaders expect than GLES - and the two
    # cannot be mixed: with both on, RetroArch compiles its legacy gl1 driver but links GLESv2, which has no
    # glMatrixMode (the first i386 build died there). So: desktop GL on x86, GLES on the Pis.
    local gl_flags="--enable-opengles --enable-opengles3"
    [ "$arch" = i386 ] && gl_flags="--enable-opengl --disable-opengles --disable-opengles3"
    local stage="$WORK/stage-$arch"
    local out="$DIST/retroarch-$TAG-$arch.tar.gz"
    banner "$arch: configure ($triplet)"
    rm -rf "$stage"
    mkdir -p "$stage" "$DIST"
    (
        cd "$SRC"
        # each architecture starts from a clean tree: the Makefile builds in place
        make -s clean >/dev/null 2>&1 || true
        export CROSS_COMPILE="$triplet-"
        export PKG_CONFIG_LIBDIR="/usr/lib/$multiarch/pkgconfig:/usr/share/pkgconfig"
        export CFLAGS="$cflags" CXXFLAGS="$cflags"
        ./configure --prefix=/usr/local \
            --disable-x11 --disable-wayland --disable-videocore --disable-vulkan --disable-qt \
            --disable-ffmpeg --disable-jack --disable-oss --disable-pulse --disable-sdl --disable-flac \
            --enable-sdl2 --enable-kms --enable-egl $gl_flags \
            --enable-udev --enable-alsa --enable-networking \
            $([ "$arch" = armhf ] && echo --enable-neon)
        banner "$arch: make -j$JOBS"
        make -j"$JOBS"
        make DESTDIR="$stage" install
    )
    "$triplet-strip" "$stage/usr/local/bin/retroarch"

    # the runtime packages: the sonames the binary needs, mapped to the packages that own them here
    banner "$arch: dependencies"
    local meta="$stage/usr/local/share/autobleem"
    mkdir -p "$meta"
    echo "$TAG" > "$meta/retroarch.version"
    # (dpkg knows a library by the path its package shipped - /lib/... for glibc and liblzma on a merged-usr
    # system, /usr/lib/... for the rest - so both are asked; libc6/libgcc/libstdc++ are always there)
    "$triplet-objdump" -p "$stage/usr/local/bin/retroarch" | awk '/NEEDED/ {print $2}' | while read -r so; do
        { dpkg -S "/usr/lib/$multiarch/$so" 2>/dev/null || dpkg -S "/lib/$multiarch/$so" 2>/dev/null || true; } \
            | head -1 | sed 's/:.*//'
    done | grep -vE '^(libc6|libgcc-s1|libstdc\+\+6|)$' | sort -u > "$meta/retroarch.depends"
    [ -s "$meta/retroarch.depends" ] || { echo "no dependencies found for $arch - something is off" >&2; exit 1; }
    cat "$meta/retroarch.depends"

    banner "$arch: $out"
    tar -C "$stage" --owner=0 --group=0 -czf "$out" .
    (cd "$DIST" && sha256sum "$(basename "$out")" > "$(basename "$out").sha256")
    ls -la "$out"
    file "$stage/usr/local/bin/retroarch" || true
}

if [ "$ARCH" = all ]; then
    build_one armhf
    build_one arm64
    build_one i386
    build_win64
else
    build_one "$ARCH"
fi
banner "done: $DIST"
