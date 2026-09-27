#!/usr/bin/env bash
# ci/build.sh TARGET... - build, check and package AutoBleem for one or more targets. Runs inside the
# autobleem-build image (docker/run.sh ci/build.sh psc) or on any Linux host with the same toolchains; the
# CI workflows call nothing else. Every target configures into the build directory the make_*.sh scripts
# use, so the two are interchangeable, builds incrementally, validates, and leaves what ships in dist/<target>/.
#
#   native   build_sys/     Debug + -Wall -Wextra, ctest, the language files, clang-format --check, clang-tidy
#   psc      build_psc/     the PlayStation Classic (toolchains/psc, AB_PSC_TOOLCHAIN) -> autobleem-psc-<v>.zip
#   rpi      build_rpi/     Raspberry Pi 32-bit (toolchains/rpi) -> dist/rpi/ (binary + abpad + resources, staged
#                           the way .github/workflows/publish-launcher.yml does; no install.sh, no package - see below)
#   rpi64    build_rpi64/   Raspberry Pi 64-bit (toolchains/rpi64) -> dist/rpi64/, same shape as rpi
#   pcusb    build_pcusb/   the 32-bit PC USB stick (toolchains/pcusb, i386 Debian) -> dist/pcusb/, same shape;
#                           its unit tests run too (i386 runs on the host)
#   win      build_mingw/   Windows (toolchains/mingw) -> autobleem-win-<v>.zip + UpdateRoms-<v>.zip, and the
#            product (build_mingw_product/, AB_TARGET=win) -> autobleem-win-product-<v>.zip + AutoBleemSetup-<v>.exe
#   all      every one of the above, in that order
#
# pcsx-ab, the PS1 emulator the console package ships, is built ahead of psc only, from its own checkout
# (AB_PCSX_DIR, default ../pcsx-ab or ../pcsx-rearmed-develop; github.com/autobleem/pcsx-ab2) with its own
# ci/build.sh, and the stripped result is staged into build_psc/emu-stage/ - never the tracked
# payload/Autobleem/bin/emu/ tree, so a build never leaves the checkout dirty (D21). make_psc_package.sh
# copies the staged emulator into the package in place of the checked-in one when it is there. AB_NO_PCSX=1
# skips the fresh build - the checked-in payload/ binaries ship, for a developer without that checkout; the
# CI always builds it. DOCS-5 (2026-09-27): rpi/rpi64/pcusb no longer build or stage an emulator, or run a
# packaging script, at all - autobleem2/autobleem-appliance owns payload_linux/ (its install.sh, its own
# make_rpi_package.sh/make_pc_image.sh/make_rpi_image.sh/biospack.py) and does that assembly from a
# launcher release plus its own pcsx-ab build. This gate builds, checks and stages the launcher binary only.
#
#   AB_JOBS=N       parallel jobs (default: nproc)
#   AB_PCSX_DIR=D   the pcsx-ab checkout;  AB_NO_PCSX=1  use the checked-in emulator binaries
#   AB_NO_SCCACHE=1 no compiler cache (sccache is put in front of every compiler when the image has it)
#   AB_NO_LINT=1    skip clang-tidy in the native target (it is the slow part)
#   AB_NO_UPX=1     leave the shipped binaries unpacked
#   AB_CLEAN=1      wipe each target's build directory first
set -euo pipefail
cd "$(dirname "$0")/.."
REPO="$PWD"
JOBS="${AB_JOBS:-$(nproc)}"

# --- the version the package names carry --------------------------------------------------------------------
# What the environment says first (make_psc.sh-style AB_GIT_*: the caller's facts about a tree that has no
# .git, or - the server's clone, tagless and "dirty" from the pcsx-ab binaries this very script copies into
# payload*/ - a tree whose own git facts would mislabel it), else git describe (v2.0.0-pre0, or
# v2.0.0-pre0-12-gabc1234 past the tag, "-dirty" appended), else "dev". cmake/generate_version.cmake makes
# the same decision, in the same order, for core/version.h.
version() {
    local v
    if [ -n "${AB_GIT_HASH:-}" ]; then
        v="${AB_GIT_VERSION:-dev}-${AB_GIT_HASH}"
        [ "${AB_GIT_DIRTY:-}" = true ] && v="$v-dirty"
    else
        v="$(git describe --tags --exclude nightly --always --dirty 2>/dev/null || true)"
        [ -n "$v" ] || v="${AB_GIT_VERSION:-dev}"
    fi
    echo "$v"
}
VERSION="$(version)"

banner() { echo; echo "==> $*"; }
configure() { # configure BUILD_DIR ARGS...
    local dir="$1"; shift
    [ -n "${AB_CLEAN:-}" ] && rm -rf "$dir"
    # a cache made for another source path (the tree moved, or a container saw it elsewhere) or with
    # another generator (make_psc.sh's Unix Makefiles on the server) is no use
    if [ -f "$dir/CMakeCache.txt" ]; then
        local cached gen
        cached="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$dir/CMakeCache.txt" | tail -1)"
        gen="$(sed -n 's/^CMAKE_GENERATOR:INTERNAL=//p' "$dir/CMakeCache.txt" | tail -1)"
        if [ "$cached" != "$REPO" ] || [ "$gen" != "Ninja" ]; then
            echo "    $dir was configured for $cached with $gen - starting it over"
            rm -rf "$dir"
        fi
    fi
    cmake -S . -B "$dir" -G Ninja "${LAUNCHER[@]}" "$@"
}
# sccache in front of every compiler when it is there (the image has it; docker/run.sh mounts the cache
# from the host) - one launcher for the native, Pi, MinGW and console compilers, each keyed by its own
# binary. AB_NO_SCCACHE=1 builds without. The stats at the end of a run say what it did.
LAUNCHER=()
if [ -z "${AB_NO_SCCACHE:-}" ] && command -v sccache >/dev/null 2>&1; then
    LAUNCHER=(-DCMAKE_C_COMPILER_LAUNCHER=sccache -DCMAKE_CXX_COMPILER_LAUNCHER=sccache)
    export AB_SCCACHE=1
    sccache --start-server >/dev/null 2>&1 || true
    sccache --zero-stats >/dev/null 2>&1 || true
fi
sccache_stats() {
    [ -n "${AB_SCCACHE:-}" ] || return 0
    banner "sccache"
    sccache --show-stats 2>/dev/null | grep -E "Compile requests|Cache hits|Cache misses|Non-cacheable|Cache size|Cache location" || true
}
dist_reset() { rm -rf "dist/$1"; mkdir -p "dist/$1"; }
dist_note() { # dist_note TARGET - what was built, for the artifact
    { echo "AutoBleem $VERSION - $1"; echo "built $(date -u '+%Y-%m-%d %H:%M:%S UTC') on $(uname -m) $(cat /etc/os-release 2>/dev/null | sed -n 's/^PRETTY_NAME="\(.*\)"/\1/p')"; } \
        > "dist/$1/BUILD.txt"
}

# --- pcsx-ab: the emulator, ahead of the launcher --------------------------------------------------------
pcsx_dir() {
    if [ -n "${AB_PCSX_DIR:-}" ]; then echo "$AB_PCSX_DIR"; return; fi
    local d
    for d in ../pcsx-ab ../pcsx-ab2 ../pcsx-rearmed-develop; do
        if [ -f "$d/ci/build.sh" ]; then (cd "$d" && pwd); return; fi
    done
}
# pcsx-abnxt, the next emulator (github.com/autobleem/pcsx-abnxt), ships next to pcsx-ab as Autobleem/bin/emunxt
# (emunxt-arm64 for the 64-bit Pi) - Options -> "PS1 Emulator" picks; AB_PCSXNXT_DIR names its checkout
pcsxnxt_dir() {
    if [ -n "${AB_PCSXNXT_DIR:-}" ]; then echo "$AB_PCSXNXT_DIR"; return; fi
    if [ -f ../pcsx-abnxt/ci/build.sh ]; then (cd ../pcsx-abnxt && pwd); fi
}
build_pcsx() { # build_pcsx psc|rpi|rpi64|pcusb DEST [nxt] - pcsx-ab (or pcsx-abnxt) for the target into the
               # staging folder DEST (build_<target>/emu-stage/<name> - never a tracked payload*/ path, D21)
    local target="$1" dest="$2" which="${3:-ab}" dir name=pcsx-ab
    [ "$which" = nxt ] && name=pcsx-abnxt
    if [ -n "${AB_NO_PCSX:-}" ]; then
        echo "    AB_NO_PCSX: the checked-in emulator ships (no fresh $name staged)"
        rm -rf "$dest"   # a stale stage from an earlier run in a reused build dir must not ship instead
        return
    fi
    if [ "$which" = nxt ]; then dir="$(pcsxnxt_dir)"; else dir="$(pcsx_dir)"; fi
    if [ -z "$dir" ]; then
        echo "$name checkout not found (AB_PCSX_DIR / AB_PCSXNXT_DIR, or ../pcsx-ab / ../pcsx-abnxt next to this tree; AB_NO_PCSX=1 ships the checked-in binaries)" >&2
        exit 1
    fi
    if ! grep -q "$target)" "$dir/ci/build.sh"; then
        echo "    $name at $dir has no $target target yet - the checked-in binaries ship"
        rm -rf "$dest"   # ditto - a stale stage from an earlier run must not ship instead
        return
    fi
    banner "$name $target: $dir"
    (cd "$dir" && AB_JOBS="$JOBS" bash ci/build.sh "$target")
    local built="$dir/build_$target/dist"
    [ -f "$built/pcsx-ab" ] || { echo "no $built/pcsx-ab after the build" >&2; exit 1; }
    rm -rf "$dest"
    mkdir -p "$dest"
    cp -a "$built/." "$dest/"
    echo "    -> $dest: $(cd "$dest" && echo pcsx-ab plugins/*.so)"
}

# --- native: the gate --------------------------------------------------------------------------------------
build_native() {
    banner "native: configure + build (build_sys)"
    configure build_sys -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DAB_ENABLE_CHD=ON
    ninja -C build_sys -j "$JOBS"
    banner "native: tests"
    ctest --test-dir build_sys --output-on-failure -j "$JOBS"
    banner "native: language files"
    python3 tools/lang_tools.py validate
    banner "native: clang-format"
    bash tools/format.sh --check
    if [ -z "${AB_NO_LINT:-}" ]; then
        banner "native: clang-tidy"
        bash tools/lint.sh -p build_sys
    fi
    dist_reset native
    dist_note native
    cp build_sys/autobleem-gui dist/native/
}

# --- psc: the console ----------------------------------------------------------------------------------------
build_psc() {
    local toolchain="${AB_PSC_TOOLCHAIN:-/opt/psc}"
    # DOCS-4 (2026-09-27): the autobleem-build image stages one canonical copy of these at /opt/ab (APPS-6) -
    # use it when it is there (every run inside the image, so every CI run and every `docker/run.sh` dev
    # build). The vendored toolchains/psc/PSCtoolchainV8.cmake and tools/check_psc_binary.sh stay only as the
    # fallback make_psc.sh needs: it builds on the bare Sony-toolchain server over ssh, no image there.
    local ab_toolchain_cmake=toolchains/psc/PSCtoolchainV8.cmake
    local ab_check_psc_binary=tools/check_psc_binary.sh
    if [ -f /opt/ab/toolchains/psc/PSCtoolchainV8.cmake ]; then
        ab_toolchain_cmake=/opt/ab/toolchains/psc/PSCtoolchainV8.cmake
    fi
    if [ -f /opt/ab/tools/check_psc_binary.sh ]; then
        ab_check_psc_binary=/opt/ab/tools/check_psc_binary.sh
    fi
    build_pcsx psc build_psc/emu-stage/emu
    build_pcsx psc build_psc/emu-stage/emunxt nxt
    banner "psc: configure + build (build_psc, toolchain $toolchain, $ab_toolchain_cmake)"
    configure build_psc -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="$ab_toolchain_cmake" -DAB_PSC_TOOLCHAIN="$toolchain"
    ninja -C build_psc -j "$JOBS"
    banner "psc: the binaries against the console's glibc 2.24 / GLIBCXX 3.4.22, no RPATH ($ab_check_psc_binary)"
    local bin
    for bin in autobleem-gui absplash abfatflag abupdate abfetch apps/abpad/abpadd apps/abpad/libabpad.so; do
        bash "$ab_check_psc_binary" "build_psc/$bin" "$toolchain"
    done
    banner "psc: package"
    dist_reset psc
    bash tools/make_psc_package.sh --build-dir build_psc --out dist/psc --version "$VERSION"
    dist_note psc
}

# --- rpi / rpi64: the Pi -------------------------------------------------------------------------------------
# DOCS-5 (2026-09-27): this used to package a full installable tarball too (tools/make_rpi_package.sh,
# staging pcsx-ab/pcsx-abnxt in ahead of it via build_pcsx). Both the packaging script and the payload_linux/
# tree it staged now live solely in autobleem2/autobleem-appliance (its own duplicate had drifted from this
# one - DOCS-6 folded the drift back into it before this repo's copies were removed). So this gate now
# stops exactly where .github/workflows/publish-launcher.yml's own staging does: the binary, abpad and
# src/resources/ into dist/<target>/, no install skeleton, no tarball, no emulator bundled - the appliance
# assembles the actual package from a launcher release plus its own build.
stage_launcher() { # stage_launcher DIR TARGET - dist/<target>/Autobleem/bin/{autobleem,abpad}, no package
    local dir="$1" target="$2"
    local bin="dist/$target/Autobleem/bin"   # its own line: local expands every word before it assigns any
    dist_reset "$target"
    mkdir -p "$bin/autobleem" "$bin/abpad"
    cp "$dir/autobleem-gui" "$bin/autobleem/"
    cp -a src/resources/. "$bin/autobleem/"
    rm -f "$bin/autobleem/internal.db"   # an appliance has no built-in games (AB_HAS_INTERNAL_GAMES is psc/dev only)
    if [ -f "$dir/apps/abpad/abpadd" ] && [ -f "$dir/apps/abpad/libabpad.so" ]; then
        cp "$dir/apps/abpad/abpadd" "$dir/apps/abpad/libabpad.so" "$bin/abpad/"
    else
        echo "    (no abpad in $dir/apps/abpad - Apps would run without the virtual gamepad)"
    fi
    dist_note "$target"
}

build_rpi() { # build_rpi armhf|arm64
    local arch="$1" dir toolchain proc
    case "$arch" in
        armhf) dir=build_rpi;   toolchain=toolchains/rpi/RPitoolchain.cmake;     proc=arm ;;
        arm64) dir=build_rpi64; toolchain=toolchains/rpi64/RPi64toolchain.cmake; proc=aarch64 ;;
    esac
    banner "rpi $arch: configure + build ($dir)"
    configure "$dir" -DCMAKE_SYSTEM_PROCESSOR="$proc" -DCMAKE_BUILD_TYPE=Release -DAB_RPI_DEBUG=OFF \
        -DCMAKE_TOOLCHAIN_FILE="$toolchain"
    ninja -C "$dir" -j "$JOBS"
    banner "rpi $arch: check"
    file "$dir/autobleem-gui"
    case "$arch" in
        armhf) file "$dir/autobleem-gui" | grep -q 'ELF 32-bit LSB.*ARM, EABI5' ;;
        arm64) file "$dir/autobleem-gui" | grep -q 'ELF 64-bit LSB.*ARM aarch64' ;;
    esac
    local target=rpi; [ "$arch" = arm64 ] && target=rpi64
    banner "rpi $arch: stage"
    stage_launcher "$dir" "$target"
}

build_pcusb() {
    banner "pcusb: configure + build (build_pcusb)"
    configure build_pcusb -DCMAKE_BUILD_TYPE=Release -DAB_PCUSB_DEBUG=OFF -DAB_ENABLE_CHD=ON         -DCMAKE_TOOLCHAIN_FILE=toolchains/pcusb/PcUsbToolchain.cmake
    ninja -C build_pcusb -j "$JOBS"
    banner "pcusb: check"
    file build_pcusb/autobleem-gui
    file build_pcusb/autobleem-gui | grep -q 'ELF 32-bit LSB.*Intel 80386'
    # i386 runs on this host: the suites are a gate here too (their scratch dirs carry the pid, -j is safe)
    banner "pcusb: tests"
    ctest --test-dir build_pcusb --output-on-failure -j "$JOBS"
    banner "pcusb: stage"
    stage_launcher build_pcusb pcusb
}

# --- win: Windows --------------------------------------------------------------------------------------------
build_win() {
    banner "win: configure + build (build_mingw)"
    configure build_mingw -DCMAKE_BUILD_TYPE=Release -DAB_ENABLE_CHD=ON \
        -DCMAKE_TOOLCHAIN_FILE=toolchains/mingw/MinGWtoolchain.cmake
    ninja -C build_mingw -j "$JOBS"
    file build_mingw/autobleem-gui.exe | grep -q 'PE32+ executable.*x86-64'
    if command -v wine64 >/dev/null 2>&1 || command -v wine >/dev/null 2>&1; then
        banner "win: tests under wine"
        ctest --test-dir build_mingw --output-on-failure -j "$JOBS"
    else
        echo "    (no wine: the test executables are built, not run - the native target runs the suites)"
    fi
    banner "win: package"
    dist_reset win
    bash tools/make_win_package.sh --build-dir build_mingw --out dist/win --version "$VERSION"

    # the Windows product: the same toolchain with AB_TARGET=win (the full-screen GUI exe, the zero-argument
    # start, the direct launches), its program folder with the two PS1 emulators (the site's win64 packages -
    # the emulators are built on the PC, not here) and AutoBleemWinSetup, then the NSIS installer over it
    banner "win: the product (build_mingw_product)"
    configure build_mingw_product -DCMAKE_BUILD_TYPE=Release -DAB_ENABLE_CHD=ON -DAB_TARGET=win \
        -DCMAKE_TOOLCHAIN_FILE=toolchains/mingw/MinGWtoolchain.cmake
    ninja -C build_mingw_product -j "$JOBS"
    file build_mingw_product/autobleem-gui.exe | grep -q 'PE32+ executable.*x86-64'
    # (not grep -q: it would quit at the match and objdump's SIGPIPE fails the pipeline under pipefail)
    objdump -p build_mingw_product/autobleem-gui.exe | grep 'Subsystem.*Windows GUI' >/dev/null
    # an extension carries the GCC runtime inside, as the exe does: the product ships neither DLL
    while IFS= read -r dll; do
        if objdump -p "$dll" | grep -E 'DLL Name: (libstdc\+\+-6|libgcc_s_seh-1)\.dll' >/dev/null; then
            echo "$dll needs the GCC runtime DLLs, which the product does not ship" >&2
            exit 1
        fi
    done < <(find build_mingw_product -path '*/extensions/*' -name '*.dll')
    bash tools/make_win_package.sh --build-dir build_mingw --product build_mingw_product --out dist/win --version "$VERSION"
    if command -v makensis >/dev/null 2>&1; then
        banner "win: the installer"
        makensis -V2 -DVERSION="$VERSION" -DSTAGE="$PWD/build_mingw_product/package/AutoBleem" \
            -DOUT="$PWD/dist/win/AutoBleemSetup-$VERSION.exe" -DICON="$PWD/src/win/autobleem.ico" \
            installer/windows/autobleem.nsi
        ls -la "dist/win/AutoBleemSetup-$VERSION.exe"
    else
        echo "    (no makensis - the installer is not built; the product zip is)"
    fi
    # the console's stick installer (apps/installer, a plain Win32 program) as the exe alone: the bundle
    # with the console package next to it is made once the psc target's package exists
    # (tools/make_installer_bundle.sh --exe, the workflow's site job), stripped the way the bundle script
    # does it - never UPX-packed: Defender quarantines a packed, unsigned exe as Trojan:Win32/Wacatac.C!ml
    if [ -f build_mingw/apps/installer/AutoBleemInstaller.exe ]; then
        cp build_mingw/apps/installer/AutoBleemInstaller.exe dist/win/AutoBleemInstaller.exe
        x86_64-w64-mingw32-strip dist/win/AutoBleemInstaller.exe 2>/dev/null || strip dist/win/AutoBleemInstaller.exe || true
    fi
    dist_note win
}

# --- main ----------------------------------------------------------------------------------------------------
[ $# -gt 0 ] || { sed -n '2,20p' "$0"; exit 2; }
targets=()
for t in "$@"; do
    case "$t" in
        all) targets+=(native psc rpi rpi64 pcusb win) ;;
        native|psc|rpi|rpi64|pcusb|win) targets+=("$t") ;;
        *) echo "unknown target: $t (native, psc, rpi, rpi64, pcusb, win, all)" >&2; exit 2 ;;
    esac
done
echo "AutoBleem $VERSION - targets: ${targets[*]} - $JOBS jobs"
start=$(date +%s)
for t in "${targets[@]}"; do
    t0=$(date +%s)
    case "$t" in
        native) build_native ;;
        psc)    build_psc ;;
        rpi)    build_rpi armhf ;;
        rpi64)  build_rpi arm64 ;;
        pcusb)  build_pcusb ;;
        win)    build_win ;;
    esac
    echo "==> $t done in $(( $(date +%s) - t0 )) s"
done
banner "done in $(( $(date +%s) - start )) s:"
for t in "${targets[@]}"; do
    find "dist/$t" -type f | sort | while read -r f; do printf '    %-50s %s\n' "$f" "$(du -h "$f" | cut -f1)"; done
done
sccache_stats
