# syntax=docker/dockerfile:1
#
# autobleem-build: one image that cross-compiles AutoBleem (and pcsx-ab) for every target, plus the native
# Linux build the unit tests and the lint/format checks run in. See docs/ci-plan.md for the reasoning and
# docker/README.md for how to use it. Build it with docker/build-image.sh (it stages the cover databases into
# the build context first), run a build in it with docker/run.sh.
#
# Stages, each FROM the previous so a change to one leaves the earlier ones cached:
#   base    - Debian Bookworm + cmake/ninja/git/python/upx
#   native  - host gcc + SDL2 dev packages, clang-format/clang-tidy pinned to the major MSYS2 has
#   pi      - Debian's own armhf and arm64 cross compilers + multiarch SDL2 dev packages (Raspberry Pi OS is
#             Debian; Bookworm's glibc 2.36 keeps the binaries loadable on Bookworm and Trixie alike)
#   mingw   - mingw-w64 (posix threads) + the official SDL2 mingw development packages (Windows)
#   db      - the three cover databases, baked in so a console package is complete wherever it is built
#   psc     - the PlayStation Classic toolchain, AutoBleem-NG's recipe: a Debian Stretch armhf sysroot
#             (glibc 2.24 / libstdc++ 6.0.22 - the console's own), Stretch's gcc-6 cross compiler, and the
#             SDL2 family built from source against it, wrapped as armv8-sony-linux-gnueabihf-* so
#             toolchains/psc/PSCtoolchainV8.cmake works with -DAB_PSC_TOOLCHAIN=/opt/psc
#   all     - the default target: everything above, with the environment the ci/ scripts expect
#
# Every version is a build-arg; the defaults are what has been verified.

ARG DEBIAN_RELEASE=bookworm

# ---------------------------------------------------------------------------------------------------------
FROM debian:${DEBIAN_RELEASE}-slim AS base
ARG DEBIAN_RELEASE
ENV DEBIAN_FRONTEND=noninteractive
# every RUN below is bash (the SDL build steps use bash functions and "${@:4}"), and a failing pipe fails
SHELL ["/bin/bash", "-o", "pipefail", "-c"]
RUN apt-get update && apt-get install -y --no-install-recommends \
        bash ca-certificates curl wget gnupg \
        git rsync xz-utils bzip2 zip unzip file patchelf \
        cmake ninja-build make pkg-config python3 \
    && rm -rf /var/lib/apt/lists/*
# UPX packs the shipped binaries to a third of their size (Bookworm has no upx package)
ARG UPX_VERSION=5.0.2
RUN cd /tmp && wget -q https://github.com/upx/upx/releases/download/v${UPX_VERSION}/upx-${UPX_VERSION}-amd64_linux.tar.xz \
    && tar -xf upx-${UPX_VERSION}-amd64_linux.tar.xz \
    && install -m755 upx-${UPX_VERSION}-amd64_linux/upx /usr/local/bin/upx \
    && rm -rf upx-${UPX_VERSION}-amd64_linux* && upx --version | head -1
# cmake/generate_version.cmake reads the bind-mounted checkout's .git, which belongs to the calling uid
RUN git config --system --add safe.directory '*'

# ---------------------------------------------------------------------------------------------------------
# native: the build the tests run in, and the format/lint tools
FROM base AS native
ARG DEBIAN_RELEASE
# the same major as MSYS2's clang-format - a different one formats some constructs differently and would
# fail tools/format.sh --check on a tree the other one formatted
ARG LLVM_VERSION=22
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential g++ gdb \
        libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-ttf-dev libpng-dev zlib1g-dev \
        libjack-jackd2-dev \
    && rm -rf /var/lib/apt/lists/*
RUN wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor -o /usr/share/keyrings/llvm.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/llvm.gpg] http://apt.llvm.org/${DEBIAN_RELEASE}/ llvm-toolchain-${DEBIAN_RELEASE}-${LLVM_VERSION} main" \
        > /etc/apt/sources.list.d/llvm.list \
    && apt-get update && apt-get install -y --no-install-recommends \
        clang-format-${LLVM_VERSION} clang-tidy-${LLVM_VERSION} \
    && ln -s /usr/bin/clang-format-${LLVM_VERSION} /usr/local/bin/clang-format \
    && ln -s /usr/bin/clang-tidy-${LLVM_VERSION} /usr/local/bin/clang-tidy \
    && ln -s /usr/bin/run-clang-tidy-${LLVM_VERSION} /usr/local/bin/run-clang-tidy \
    && rm -rf /var/lib/apt/lists/*
COPY ab-validate.sh /usr/local/bin/ab-validate
RUN ab-validate native

# ---------------------------------------------------------------------------------------------------------
# pi: Debian's cross compilers and the foreign-architecture SDL2 dev packages (real headers, unversioned
# .so links and sdl2-config.cmake - no borrowed headers needed, unlike the SysGCC toolchains on Windows).
# libsdl2-mixer-dev pulls a JACK dev package through fluidsynth; jack1 and jackd2 conflict, so every
# architecture (the host's above included) is told to take jackd2, which co-installs across them.
FROM native AS pi
RUN dpkg --add-architecture armhf && dpkg --add-architecture arm64 \
    && apt-get update && apt-get install -y --no-install-recommends \
        crossbuild-essential-armhf crossbuild-essential-arm64 \
        libsdl2-dev:armhf libsdl2-image-dev:armhf libsdl2-mixer-dev:armhf libsdl2-ttf-dev:armhf \
        libpng-dev:armhf zlib1g-dev:armhf libjack-jackd2-dev:armhf \
        libsdl2-dev:arm64 libsdl2-image-dev:arm64 libsdl2-mixer-dev:arm64 libsdl2-ttf-dev:arm64 \
        libpng-dev:arm64 zlib1g-dev:arm64 libjack-jackd2-dev:arm64 \
    && rm -rf /var/lib/apt/lists/*
COPY ab-validate.sh /usr/local/bin/ab-validate
RUN ab-validate pi

# ---------------------------------------------------------------------------------------------------------
# mingw: the Windows cross build. The official SDL2 "devel-mingw" packages carry x86_64-w64-mingw32
# headers, import libraries, pkg-config files and self-contained DLLs (SDL2_image/ttf with their png/
# freetype inside), which is what makes the Windows zip four DLLs instead of MSYS2's dependency closure.
FROM pi AS mingw
ARG SDL2_MINGW_VERSION=2.32.10
ARG SDL2_IMAGE_MINGW_VERSION=2.8.8
ARG SDL2_MIXER_MINGW_VERSION=2.8.1
ARG SDL2_TTF_MINGW_VERSION=2.24.0
RUN apt-get update && apt-get install -y --no-install-recommends \
        g++-mingw-w64-x86-64-posix binutils-mingw-w64-x86-64 mingw-w64-tools \
    && rm -rf /var/lib/apt/lists/*
# each devel package's Makefile has an install-package target that copies bin/include/lib and rewrites the
# prefix in sdl2-config, the .la and the .pc files
RUN set -e; cd /tmp; \
    fetch() { wget -q "https://github.com/libsdl-org/$1/releases/download/release-$3/$2-devel-$3-mingw.tar.gz" \
              && tar -xzf "$2-devel-$3-mingw.tar.gz" \
              && make -C "$2-$3" install-package arch=x86_64-w64-mingw32 prefix=/opt/mingw-sdl2 \
              && rm -rf "$2-devel-$3-mingw.tar.gz" "$2-$3"; }; \
    mkdir -p /opt/mingw-sdl2; \
    fetch SDL       SDL2       ${SDL2_MINGW_VERSION}; \
    fetch SDL_image SDL2_image ${SDL2_IMAGE_MINGW_VERSION}; \
    fetch SDL_mixer SDL2_mixer ${SDL2_MIXER_MINGW_VERSION}; \
    fetch SDL_ttf   SDL2_ttf   ${SDL2_TTF_MINGW_VERSION}
COPY ab-validate.sh /usr/local/bin/ab-validate
RUN ab-validate mingw

# ---------------------------------------------------------------------------------------------------------
# db: the cover databases (docker/build-image.sh stages them into docker/db/ from wherever they are)
FROM mingw AS db
COPY db/coversJ.db db/coversP.db db/coversU.db /opt/autobleem/db/

# ---------------------------------------------------------------------------------------------------------
# psc: the PlayStation Classic. Everything under /opt/psc:
#   sysroot/   Debian Stretch armhf, extracted (no maintainer scripts) - the console's glibc 2.24 /
#              libstdc++ 6.0.22, plus the -dev packages SDL2 and pcsx-ab need
#   gcc-6/     Stretch's amd64 gcc-6/g++-6 cross packages, extracted; the newest GCC whose libstdc++ ABI
#              matches that glibc (Bookworm's arm-linux-gnueabihf gcc would need glibc 2.34+ on the console)
#   sdl2/      SDL2 2.0.12 + image 2.6.3 + mixer 2.6.3 + ttf 2.20.2 built here against the sysroot - the
#              versions and backends of the console's libs.tar.gz; what a release ships in it
#   bin/       armv8-sony-linux-gnueabihf-{gcc,g++,cpp} wrappers adding --sysroot, the binutils as links
FROM db AS psc
ARG SDL2_VERSION=2.0.12
ARG SDL2_IMAGE_VERSION=2.6.3
ARG SDL2_MIXER_VERSION=2.6.3
ARG SDL2_TTF_VERSION=2.20.2
ARG WAYLAND_VERSION=1.12.0
ARG PSC_CFLAGS="-march=armv8-a -mfpu=neon-vfpv4 -mfloat-abi=hard -O2"

RUN apt-get update && apt-get install -y --no-install-recommends \
        mmdebstrap debian-archive-keyring binutils-arm-linux-gnueabihf libexpat1-dev \
    && rm -rf /var/lib/apt/lists/*

# The sysroot. --variant=extract unpacks the .debs without running maintainer scripts (the target's
# parent has to exist - mmdebstrap resolves the path before creating it). The package set is what SDL2's
# backends link against, plus libpng and GLESv1 for pcsx-ab (its gpu_gles plugin is GLES 1 on EGL). Stretch's Release
# file has expired and its key was retired, so validity checks are off and the retired key comes from
# debian-archive-removed-keys.gpg.
RUN mkdir -p /opt/psc && mmdebstrap \
        --architectures=armhf \
        --variant=extract \
        --aptopt='Acquire::Check-Valid-Until "false"' \
        --aptopt='Acquire::AllowInsecureRepositories "true"' \
        --aptopt='Acquire::AllowDowngradeToInsecureRepositories "true"' \
        --aptopt='APT::Get::AllowUnauthenticated "true"' \
        --keyring=/usr/share/keyrings/debian-archive-removed-keys.gpg \
        --include="libasound2-dev,libc6-dev,libgcc-6-dev,libstdc++-6-dev,libgles2-mesa-dev,libgles1-mesa-dev,libogg-dev,libvorbis-dev,libudev-dev,libwayland-dev,libxkbcommon-dev,linux-libc-dev,wayland-protocols,zlib1g-dev,libpng-dev" \
        stretch /opt/psc/sysroot \
        "deb http://archive.debian.org/debian stretch main"

# Absolute symlinks (libc.so -> /lib/arm-linux-gnueabihf/...) would resolve on the host, where nothing is;
# the linker would then silently fall back to the static .a. Rewritten relative by hand - the `symlinks`
# tool refuses links whose targets do not resolve.
RUN cd /opt/psc/sysroot && \
    find . -type l | while read -r link; do \
        target=$(readlink "$link"); \
        case "$target" in \
            /*) dir=$(dirname "$link"); \
                ln -sfn "$(realpath -m --relative-to="$dir" ".$target")" "$link" ;; \
        esac; \
    done

# The cross compiler: Stretch's amd64 packages, extracted the same way. Their dependencies come along -
# Stretch's binutils-arm-linux-gnueabihf (2.28), the -cross libc/libgcc/libstdc++ trees, and the host
# libraries the compiler proper needs (libisl15, libmpc3, libmpfr4, libgmp10, libbfd-2.28).
RUN mmdebstrap \
        --architectures=amd64 \
        --variant=extract \
        --aptopt='Acquire::Check-Valid-Until "false"' \
        --aptopt='Acquire::AllowInsecureRepositories "true"' \
        --aptopt='Acquire::AllowDowngradeToInsecureRepositories "true"' \
        --aptopt='APT::Get::AllowUnauthenticated "true"' \
        --keyring=/usr/share/keyrings/debian-archive-removed-keys.gpg \
        --include="g++-6-arm-linux-gnueabihf,gcc-6-arm-linux-gnueabihf,libgcc-6-dev-armhf-cross,libstdc++-6-dev-armhf-cross" \
        stretch /opt/psc/gcc-6 \
        "deb http://archive.debian.org/debian stretch main"

# Stretch's own libstdc++/libgcc_s for the host must not be picked up by anything (the host's are newer
# and backward compatible); the rest of that directory - isl/mpc/mpfr/gmp/bfd - is what gcc-6's cc1,
# cc1plus, lto1 and ld need, and a RUNPATH on each host executable points them there. No LD_LIBRARY_PATH
# anywhere, so nothing else in the image is affected.
RUN cd /opt/psc/gcc-6 && \
    rm -f usr/lib/x86_64-linux-gnu/libstdc++.so* usr/lib/x86_64-linux-gnu/libgcc_s.so* \
          lib/x86_64-linux-gnu/libgcc_s.so* && \
    find . -type f -perm -u+x | while read -r f; do \
        if file -b "$f" | grep -q '^ELF 64-bit LSB .*x86-64.*dynamically'; then \
            patchelf --set-rpath /opt/psc/gcc-6/usr/lib/x86_64-linux-gnu "$f" || exit 1; \
        fi; \
    done

# The relocated g++ looks for its C++ headers and the -cross libstdc++/libgcc under
# <sysroot>/usr/arm-linux-gnueabihf/ (its configured include dir is sysroot-relative), so the cross tree
# is symlink-farmed there, and the sysroot's own multiarch libraries are mirrored into that tree's lib/
# so the tooldir search finds libwayland, libudev, libGLESv2... next to libc.
RUN mkdir -p /opt/psc/sysroot/usr/arm-linux-gnueabihf && \
    cp -rs /opt/psc/gcc-6/usr/arm-linux-gnueabihf/. /opt/psc/sysroot/usr/arm-linux-gnueabihf/ && \
    cd /opt/psc/sysroot/lib/arm-linux-gnueabihf && \
    for f in *; do \
        ln -sfn "../../../lib/arm-linux-gnueabihf/$f" "/opt/psc/sysroot/usr/arm-linux-gnueabihf/lib/$f"; \
    done && \
    cd /opt/psc/sysroot/usr/lib/arm-linux-gnueabihf && \
    for f in *; do \
        if [ -L "/opt/psc/sysroot/usr/arm-linux-gnueabihf/lib/$f" ] && \
           [ -e "/opt/psc/sysroot/usr/arm-linux-gnueabihf/lib/$f" ]; then continue; fi; \
        ln -sfn "../../lib/arm-linux-gnueabihf/$f" "/opt/psc/sysroot/usr/arm-linux-gnueabihf/lib/$f"; \
    done

# glibc's libc.so / libpthread.so / libm.so are linker scripts naming their pieces by absolute path
# (GROUP ( /usr/arm-linux-gnueabihf/lib/libc.so.6 ... ) in the -cross packages, /lib/arm-linux-gnueabihf/
# in the sysroot's), which ld only prefixes with the sysroot when it thinks the script lives inside it.
# AutoBleem-NG solved that by pointing the *host's* /usr/arm-linux-gnueabihf at the sysroot - which is
# where Bookworm's cross libc for the Pi build lives in this image. Rewritten to bare names instead (the
# crosstool-NG form): ld then searches its -L paths, all of which are ours.
RUN for dir in /opt/psc/gcc-6/usr/arm-linux-gnueabihf/lib /opt/psc/sysroot/usr/lib/arm-linux-gnueabihf; do \
        for f in "$dir"/*.so; do \
            [ -f "$f" ] && ! file -b "$f" | grep -q ELF && grep -qa 'GROUP\|INPUT' "$f" || continue; \
            sed -i -e 's|/usr/arm-linux-gnueabihf/lib/||g' -e 's|/usr/lib/arm-linux-gnueabihf/||g' \
                   -e 's|/lib/arm-linux-gnueabihf/||g' "$f"; \
        done; \
    done

# The wrappers PSCtoolchainV8.cmake and the SDL builds compile with, and the binutils under the same
# prefix (Bookworm's arm-linux-gnueabihf-* - the binutils version is not an ABI matter; tools/
# check_psc_binary.sh finds the readelf here).
RUN mkdir -p /opt/psc/bin && \
    for t in gcc g++ cpp; do \
        printf '#!/bin/sh\nexec /opt/psc/gcc-6/usr/bin/arm-linux-gnueabihf-%s-6 --sysroot=/opt/psc/sysroot "$@"\n' "$t" \
            > /opt/psc/bin/armv8-sony-linux-gnueabihf-$t && chmod 755 /opt/psc/bin/armv8-sony-linux-gnueabihf-$t; \
    done && \
    for t in ar as ld nm objcopy objdump ranlib readelf strip; do \
        ln -s /usr/bin/arm-linux-gnueabihf-$t /opt/psc/bin/armv8-sony-linux-gnueabihf-$t; \
    done
COPY ab-validate.sh /usr/local/bin/ab-validate
RUN ab-validate psc-compiler

# wayland-scanner for SDL2's Wayland backend, built from the sysroot's own wayland version: the sysroot's
# scanner is an ARM binary and the host's (1.22) generates calls Stretch's 1.12 headers do not have. Only
# the scanner (an xml -> C generator) is needed; SDL finds it through the sysroot's wayland-scanner.pc.
RUN cd /tmp && \
    wget -q https://wayland.freedesktop.org/releases/wayland-${WAYLAND_VERSION}.tar.xz && \
    tar -xf wayland-${WAYLAND_VERSION}.tar.xz && \
    cd wayland-${WAYLAND_VERSION}/src && \
    printf '#define PACKAGE_VERSION "%s"\n#define HAVE_STRNDUP 1\n' "${WAYLAND_VERSION}" > config.h && \
    gcc -O2 -I. -o wayland-scanner scanner.c wayland-util.c $(pkg-config --cflags --libs expat) && \
    install -D -m755 wayland-scanner /opt/psc/host/bin/wayland-scanner && \
    ln -sf /opt/psc/host/bin/wayland-scanner /opt/psc/sysroot/usr/bin/wayland-scanner && \
    /opt/psc/host/bin/wayland-scanner --version && \
    cd /tmp && rm -rf wayland-${WAYLAND_VERSION}*

# A toolchain file for the SDL builds below (AutoBleem itself uses the repo's toolchains/psc/ files).
# Libraries and headers are looked up only in the sysroot and the SDL prefix; pkg-config is pointed at the
# sysroot's .pc files with the sysroot prepended to their paths.
RUN printf '%s\n' \
    'set(CMAKE_SYSTEM_NAME Linux)' \
    'set(CMAKE_SYSTEM_PROCESSOR arm)' \
    'set(CMAKE_SYSROOT /opt/psc/sysroot)' \
    'set(CMAKE_C_COMPILER /opt/psc/bin/armv8-sony-linux-gnueabihf-gcc)' \
    'set(CMAKE_CXX_COMPILER /opt/psc/bin/armv8-sony-linux-gnueabihf-g++)' \
    'set(CMAKE_FIND_ROOT_PATH /opt/psc/sysroot /opt/psc/sdl2)' \
    'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' \
    'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' \
    'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' \
    'set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)' \
    'set(ENV{PKG_CONFIG_LIBDIR} "/opt/psc/sdl2/lib/pkgconfig:/opt/psc/sysroot/usr/lib/arm-linux-gnueabihf/pkgconfig:/opt/psc/sysroot/usr/share/pkgconfig")' \
    'set(ENV{PKG_CONFIG_SYSROOT_DIR} "/opt/psc/sysroot")' \
    > /opt/psc/toolchain.cmake

# SDL2 - the console's libSDL2-2.0.so.0.12.0 rebuilt: autotools (a CMake-built 2.0.12 left the console at a
# black screen in NG's testing), Wayland (shared) + dummy video, GLES 1/2 through EGL, ALSA (shared) +
# disk + dummy audio, udev hot-plug; no X11, KMSDRM, RPi, Vulkan, PulseAudio, JACK, sndio, DBus, IBus.
# The SDL family installs to its own prefix so its .pc/.cmake files hold real paths; the sysroot gets
# links to it so a build that only knows the sysroot (toolchains/psc/cmake/FindSDL2.cmake) sees
# usr/lib/libSDL2.so and usr/include/SDL2 there.
RUN cd /tmp && \
    wget -q https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-${SDL2_VERSION}.tar.gz && \
    tar -xzf SDL2-${SDL2_VERSION}.tar.gz && cd SDL2-${SDL2_VERSION} && \
    PKG_CONFIG_LIBDIR=/opt/psc/sysroot/usr/lib/arm-linux-gnueabihf/pkgconfig:/opt/psc/sysroot/usr/share/pkgconfig \
    PKG_CONFIG_SYSROOT_DIR=/opt/psc/sysroot \
    CC=/opt/psc/bin/armv8-sony-linux-gnueabihf-gcc CXX=/opt/psc/bin/armv8-sony-linux-gnueabihf-g++ \
    CFLAGS="${PSC_CFLAGS}" \
    ./configure --host=arm-linux-gnueabihf --prefix=/opt/psc/sdl2 \
        --enable-shared --enable-static \
        --disable-video-x11 --enable-video-wayland --enable-wayland-shared --disable-video-wayland-qt-touch \
        --disable-video-kmsdrm --disable-video-rpi --disable-video-vivante --disable-video-vulkan \
        --enable-video-opengl --enable-video-opengles --enable-video-opengles1 --enable-video-opengles2 \
        --enable-alsa --enable-alsa-shared --disable-pulseaudio --disable-jack --disable-sndio \
        --enable-libudev --enable-diskaudio --enable-dummyaudio --enable-video-dummy \
        --disable-dbus --disable-ibus --disable-rpath && \
    make -j"$(nproc)" && make install && \
    test -f /opt/psc/sdl2/lib/libSDL2.so && \
    cd /tmp && rm -rf SDL2-${SDL2_VERSION}*

# SDL2_image - vendored PNG + JPG only. SDL2_mixer - the backends of the console's libSDL2_mixer-2.0.so.0:
# WAV, OGG through the sysroot's libvorbisfile (a NEEDED library, not dlopen'ed), the native MIDI parser;
# no MOD/FLAC/Opus/MP3/WavPack/FluidSynth/Timidity. SDL2_ttf - vendored FreeType, no HarfBuzz.
RUN cd /tmp && \
    build() { wget -q "https://github.com/libsdl-org/$1/releases/download/release-$3/$2-$3.tar.gz" && \
              tar -xzf "$2-$3.tar.gz" && \
              cmake -S "$2-$3" -B "$2-$3/build" -G Ninja \
                  -DCMAKE_TOOLCHAIN_FILE=/opt/psc/toolchain.cmake \
                  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="${PSC_CFLAGS}" \
                  -DCMAKE_INSTALL_PREFIX=/opt/psc/sdl2 -DCMAKE_INSTALL_LIBDIR=lib \
                  -DSDL2_DIR=/opt/psc/sdl2/lib/cmake/SDL2 "${@:4}" && \
              cmake --build "$2-$3/build" -j"$(nproc)" && cmake --install "$2-$3/build" && \
              rm -rf "$2-$3.tar.gz" "$2-$3"; }; \
    build SDL_image SDL2_image ${SDL2_IMAGE_VERSION} \
        -DSDL2IMAGE_SHARED=ON -DSDL2IMAGE_STATIC=OFF -DSDL2IMAGE_TESTS=OFF -DSDL2IMAGE_SAMPLES=OFF \
        -DSDL2IMAGE_VENDORED=ON -DSDL2IMAGE_PNG=ON -DSDL2IMAGE_JPG=ON \
        -DSDL2IMAGE_AVIF=OFF -DSDL2IMAGE_JXL=OFF -DSDL2IMAGE_TIFF=OFF -DSDL2IMAGE_WEBP=OFF && \
    build SDL_mixer SDL2_mixer ${SDL2_MIXER_VERSION} \
        -DSDL2MIXER_SHARED=ON -DSDL2MIXER_STATIC=OFF -DSDL2MIXER_SAMPLES=OFF \
        -DSDL2MIXER_MOD=OFF -DSDL2MIXER_MIDI=ON -DSDL2MIXER_MIDI_NATIVE=ON -DSDL2MIXER_MIDI_FLUIDSYNTH=OFF \
        -DSDL2MIXER_OPUS=OFF -DSDL2MIXER_FLAC=OFF -DSDL2MIXER_WAVPACK=OFF -DSDL2MIXER_MP3=OFF \
        -DSDL2MIXER_OGG=ON -DSDL2MIXER_VORBIS=VORBISFILE -DSDL2MIXER_VORBIS_VORBISFILE_SHARED=OFF && \
    build SDL_ttf SDL2_ttf ${SDL2_TTF_VERSION} \
        -DSDL2TTF_SHARED=ON -DSDL2TTF_STATIC=OFF -DSDL2TTF_SAMPLES=OFF \
        -DSDL2TTF_VENDORED=ON -DSDL2TTF_HARFBUZZ=OFF

# the sysroot view of the SDL prefix (what the repo's FindSDL2.cmake reads)
RUN ln -s /opt/psc/sdl2/include/SDL2 /opt/psc/sysroot/usr/include/SDL2 && \
    for f in /opt/psc/sdl2/lib/libSDL2*.so*; do ln -s "$f" "/opt/psc/sysroot/usr/lib/$(basename "$f")"; done
RUN ab-validate psc

# ---------------------------------------------------------------------------------------------------------
FROM psc AS all
# what ci/build.sh and the toolchain files read
ENV AB_PSC_TOOLCHAIN=/opt/psc \
    AB_MINGW_SDL2=/opt/mingw-sdl2 \
    AB_COVERS_DB_DIR=/opt/autobleem/db \
    AB_BUILD_IMAGE_STAGE=all
WORKDIR /src
CMD ["/bin/bash"]
