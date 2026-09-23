#!/usr/bin/env bash
# Build AutoBleem for the PlayStation Classic on the build server, which has Sony's armv8-sony-linux-gnueabihf
# toolchain (GCC 8.2.0, crosstool-NG) and the console's sysroot at /opt/toolchain - the toolchain AutoBleem
# has always been released with. The tree is rsynced up, configured there with
# toolchains/psc/PSCtoolchainV8.cmake, built, and autobleem-gui comes back into build_psc/dist/.
# The same shape as pcsx-ab's make_psc.sh, so the two projects build side by side on the same server.
#
# Needs a "Host psc-build" entry in ~/.ssh/config (HostName, User, IdentityFile) with key login working -
# "ssh psc-build true" must not prompt - and rsync on both ends. MSYS2's ssh and Git for Windows' ssh have
# different homes (C:/msys64/home/<you> vs C:/Users/<you>), so the entry and the key go in both. The server
# side needs CMake >= 3.16, which is in ~/opt/cmake there (the distro's is 3.10).
#
#   ./make_psc.sh            rebuild what changed (the remote build dir is kept; rsync only sends changes)
#   ./make_psc.sh --clean    wipe the remote build dir first, a full rebuild
#   ./make_psc.sh -k         the old spelling of the default, still accepted
#   AB_PSC_HOST=other-host ./make_psc.sh
set -e

# ---------------------------------------------------------------------------------------------
# Superseded by the Docker image. This script builds against the 2019 Sony toolchain at
# /opt/toolchain, whose sysroot carries SDL2 **2.0.4** - while the console actually runs our own
# **2.0.12**, unpacked from Autobleem/lib/libs.tar.gz at boot. So lib_ableem may use anything SDL
# 2.0.12 has (the screenshot capture uses SDL_CreateRGBSurfaceWithFormat, which is 2.0.5) and this
# toolchain cannot link it. The failure is a pile of undefined references, which reads like a broken
# tree rather than a stale sysroot - hence this notice rather than letting it run into that.
#
# The image's own toolchain builds SDL2 2.0.12 from source, the same version the console runs, so it
# has no such gap, and it is what releases are built with:
#
#     ssh psc-build; cd ~/autobleem; docker/run.sh ci/build.sh psc
#
# AB_FORCE_SONY_TOOLCHAIN=1 runs this anyway - it will work again if that sysroot ever gets an SDL2
# of 2.0.5 or newer.
# ---------------------------------------------------------------------------------------------
if [ -z "${AB_FORCE_SONY_TOOLCHAIN:-}" ]; then
    echo "make_psc.sh builds against the 2019 Sony toolchain, whose SDL2 is 2.0.4 - older than the" >&2
    echo "2.0.12 the console actually runs, and too old to link what lib_ableem uses today." >&2
    echo >&2
    echo "Build the console in the image instead:" >&2
    echo "    docker/run.sh ci/build.sh psc        (on psc-build, in ~/autobleem)" >&2
    echo >&2
    echo "AB_FORCE_SONY_TOOLCHAIN=1 runs this anyway." >&2
    exit 1
fi
cd "$(dirname "$0")"

HOST="${AB_PSC_HOST:-psc-build}"                            # a Host entry in ~/.ssh/config (see above)
REMOTE_DIR="${AB_PSC_DIR:-autobleem}"                       # relative to the server user's home
REMOTE_CMAKE='$HOME/opt/cmake/bin/cmake'
TOOLCHAIN=/opt/toolchain                                    # AB_PSC_TOOLCHAIN on the server
JOBS="${AB_PSC_JOBS:-2}"
SSH="ssh -o BatchMode=yes $HOST"
export RSYNC_RSH="ssh -o BatchMode=yes"

# Only what the build reads. usb/ is the 150 MB smoke-test tree, payload*/ and db/ are release data the build
# never touches, !refactor/ holds sources that were already vendored, toolchains/rpi/sdl2-devkit is the other
# cross target's headers.
echo "==> syncing to $HOST:$REMOTE_DIR"
rsync -az --delete \
    --exclude '/build_*' --exclude '/.git' --exclude '/.vscode' --exclude '/dist' \
    --exclude '/usb' --exclude '/db' --exclude '/payload' --exclude '/payload_linux' --exclude '/!refactor' --exclude '/psctools' \
    --exclude '/toolchains/rpi/sdl2-devkit' \
    --exclude '*.o' --exclude '*.so' --exclude '*.exe' --exclude '*.dll' \
    ./ "$HOST:$REMOTE_DIR/"

if [ "${1:-}" = "--clean" ]; then
    $SSH "rm -rf $REMOTE_DIR/build_psc"
fi

# CMAKE_SYSTEM_PROCESSOR comes from the toolchain file ("arm"), which is what selects the root CMakeLists'
# console branch (armv8 flags, the Sony sysroot, ABLEEM_EMBEDDED_TARGET on, tests off).
# The tree goes up without .git, so core/version.h (cmake/generate_version.cmake) is told what it is.
AB_GIT_HASH=$(git rev-parse --short HEAD 2>/dev/null || echo unknown)
AB_GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)
AB_GIT_VERSION=$(git describe --tags --abbrev=0 2>/dev/null || true)
AB_GIT_DIRTY=false; git diff-index --quiet HEAD -- 2>/dev/null || AB_GIT_DIRTY=true
GIT_ENV="AB_GIT_HASH=$AB_GIT_HASH AB_GIT_BRANCH=$AB_GIT_BRANCH AB_GIT_VERSION=$AB_GIT_VERSION AB_GIT_DIRTY=$AB_GIT_DIRTY"

echo "==> configuring and building on $HOST"
$SSH "cd $REMOTE_DIR && $REMOTE_CMAKE -S . -B build_psc -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=toolchains/psc/PSCtoolchainV8.cmake -DAB_PSC_TOOLCHAIN=$TOOLCHAIN \
    && env $GIT_ENV $REMOTE_CMAKE --build build_psc -j $JOBS"

# What the console can load. Its stock glibc is 2.24 and its libstdc++ is 6.0.22 (GLIBCXX_3.4.22), older
# than the toolchain's own, so a C++ library feature that needs a newer symbol version links here and
# fails to load there; and an RPATH/RUNPATH would point at the server's sysroot. Checked on the server
# with the toolchain's readelf before the binary comes back (AutoBleem-NG's docker-validate.sh gates).
# the launcher, its two helpers (absplash, abfatflag - src/tools/) and the console tools under apps/, each
# where its build leaves it
BINARIES="autobleem-gui absplash abfatflag"
echo "==> checking the binaries against the console's glibc 2.24 / GLIBCXX 3.4.22, no RPATH"
for bin in $BINARIES; do
    $SSH "cd $REMOTE_DIR && bash tools/check_psc_binary.sh build_psc/$bin $TOOLCHAIN" || {
        echo "    $bin would not load on the console - not fetching anything"; exit 1; }
done

# The console binaries are already stripped (-s is in the console CPU flags), so they come back as they are.
# tar rather than rsync for the way back: rsync insists on POSIX modes, which NTFS under MSYS2 refuses.
echo "==> fetching results"
rm -rf build_psc/dist
mkdir -p build_psc/dist
$SSH "cd $REMOTE_DIR/build_psc && tar czf - $BINARIES" | tar xzf - --no-same-permissions -C build_psc/dist
# UPX takes the stripped binary to a third of its size (3.1 MB -> 1 MB on the Pi build); the console
# unpacks it in memory at start. AB_NO_UPX=1 skips it - a packed binary is no use to gdb.
if [ -z "${AB_NO_UPX:-}" ] && command -v upx >/dev/null 2>&1; then
    echo "==> packing with upx"
    for bin in $BINARIES; do
        [ "$bin" = abfatflag ] && continue # 10 KB, which upx refuses
        upx -q --best --lzma build_psc/dist/$bin
    done
fi
# the tools go straight into the payload's Apps folders (with their resources), the launcher stays in
# dist/ for the release script to pick up
echo "==> payload/Apps: pscbios, abflashkit"
for tool in pscbios abflashkit; do
    cp build_psc/dist/apps/$tool/$tool payload/Apps/$tool/$tool
    cp -r apps/$tool/resources/. payload/Apps/$tool/
done
echo "==> build_psc/dist:"
find build_psc/dist -type f | xargs ls -l
