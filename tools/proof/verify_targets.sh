#!/bin/bash
# Workspace check (not committed): every cross target configures and builds - the launcher, its helpers,
# the hello extension - the way the CI's jobs do, minus the emulators and the packaging. Run in the image.
set -u
export AB_GIT_HASH=verify AB_GIT_BRANCH=feature/extensions AB_GIT_VERSION=v2.0.0-alpha2 AB_GIT_DIRTY=true
fail=0
build() { # dir toolchain [extra cmake args]
    local dir="$1" tc="$2"; shift 2
    echo "=== $dir"
    if cmake -S . -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE=Release -DAB_BUILD_TESTS=OFF \
            -DCMAKE_TOOLCHAIN_FILE="$tc" "$@" >"$dir.configure.log" 2>&1 &&
       ninja -C "$dir" >"$dir.build.log" 2>&1; then
        echo "ok: $(ls "$dir"/extensions/hello/bin/*/ 2>/dev/null | tr '\n' ' ') $(ls -la "$dir"/autobleem-gui* | awk '{print $5}' | tr '\n' ' ')"
    else
        echo "FAILED - tail of the log:"; tail -25 "$dir.configure.log" "$dir.build.log" 2>/dev/null | grep -v "^$" | tail -25
        fail=1
    fi
}
build build_v_psc toolchains/psc/PSCtoolchainV8.cmake -DAB_PSC_TOOLCHAIN=/opt/psc
build build_v_rpi toolchains/rpi/RPitoolchain.cmake
build build_v_rpi64 toolchains/rpi64/RPi64toolchain.cmake
build build_v_win toolchains/mingw/MinGWtoolchain.cmake -DAB_TARGET=win
[ -f build_v_psc/extensions/hello/bin/psc/hello.so ] && bash tools/check_psc_binary.sh build_v_psc/extensions/hello/bin/psc/hello.so /opt/psc
exit $fail
