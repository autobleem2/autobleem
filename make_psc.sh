#!/usr/bin/env bash
# Build AutoBleem for the PlayStation Classic on the build server, which has Sony's armv8-sony-linux-gnueabihf
# toolchain (GCC 8.2.0, crosstool-NG) and the console's sysroot at /opt/toolchain - the toolchain AutoBleem
# has always been released with. The tree is rsynced up, configured there with
# toolchains/psc/PSCtoolchainV8.cmake, built, and autobleem-gui + starter come back into build_psc/dist/.
# The same shape as pcsx-ab's make_psc.sh, so the two projects build side by side on the same server.
#
# Needs a "Host psc-build" entry in ~/.ssh/config (HostName, User, IdentityFile) with key login working -
# "ssh psc-build true" must not prompt - and rsync on both ends. MSYS2's ssh and Git for Windows' ssh have
# different homes (C:/msys64/home/<you> vs C:/Users/<you>), so the entry and the key go in both. The server
# side needs CMake >= 3.16, which is in ~/opt/cmake there (the distro's is 3.10).
#
#   ./make_psc.sh            full rebuild on the server
#   ./make_psc.sh -k         keep the remote build dir, rebuild what changed
#   AB_PSC_HOST=other-host ./make_psc.sh
set -e
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
    --exclude '/usb' --exclude '/db' --exclude '/payload' --exclude '/payload_rpi' --exclude '/!refactor' \
    --exclude '/toolchains/rpi/sdl2-devkit' \
    --exclude '*.o' --exclude '*.so' --exclude '*.exe' --exclude '*.dll' \
    ./ "$HOST:$REMOTE_DIR/"

if [ "${1:-}" != "-k" ]; then
    $SSH "rm -rf $REMOTE_DIR/build_psc"
fi

# CMAKE_SYSTEM_PROCESSOR comes from the toolchain file ("arm"), which is what selects the root CMakeLists'
# console branch (armv8 flags, the Sony sysroot, ABLEEM_EMBEDDED_TARGET on, tests off).
echo "==> configuring and building on $HOST"
$SSH "cd $REMOTE_DIR && $REMOTE_CMAKE -S . -B build_psc -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=toolchains/psc/PSCtoolchainV8.cmake -DAB_PSC_TOOLCHAIN=$TOOLCHAIN \
    && $REMOTE_CMAKE --build build_psc -j $JOBS"

# The console binaries are already stripped (-s is in the console CPU flags), so they come back as they are.
# tar rather than rsync for the way back: rsync insists on POSIX modes, which NTFS under MSYS2 refuses.
echo "==> fetching results"
rm -rf build_psc/dist
mkdir -p build_psc/dist
$SSH "cd $REMOTE_DIR/build_psc && tar czf - autobleem-gui starter" | tar xzf - --no-same-permissions -C build_psc/dist
echo "==> build_psc/dist:"
ls -l build_psc/dist
