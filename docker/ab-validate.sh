#!/usr/bin/env bash
# ab-validate <check> - the build-time checks docker/Dockerfile runs at the end of each toolchain layer,
# so a broken toolchain fails the image build where it broke, not an hour later in the first real build.
# Each check compiles a small program the way the corresponding target is compiled (a C++14 program with
# std::thread, iostream and the four SDL2 libraries), then looks at the result.
#
#   ab-validate native          host g++ + the SDL2 dev packages; the program is run
#   ab-validate pi              arm-linux-gnueabihf and aarch64-linux-gnu g++ + the multiarch SDL2 packages
#   ab-validate mingw           x86_64-w64-mingw32-g++-posix + /opt/mingw-sdl2; the result is a PE32+ exe
#   ab-validate psc-compiler    the Stretch gcc-6 cross compiler links a C++ program against its sysroot
#   ab-validate psc             ...and against the SDL2 family built into /opt/psc/sdl2; the binary is
#                               ARMv8, needs nothing above GLIBC_2.24 / GLIBCXX_3.4.22, has no RPATH
#
# A binary built for the console is checked the same way at build time by tools/check_psc_binary.sh.
set -euo pipefail

check="${1:-}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# a C++14 program with the things the launcher leans on: threads, streams, regex, and the SDL2 family
cat > "$work/test.cpp" <<'EOF'
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <thread>
int main(int, char **) {
    SDL_version v;
    SDL_GetVersion(&v);
    auto text = std::make_unique<std::string>("SDL " + std::to_string(v.major) + "." + std::to_string(v.minor) +
                                              "." + std::to_string(v.patch));
    std::thread t([&] { std::cout << *text << ", image " << SDL_IMAGE_MAJOR_VERSION << "." << SDL_IMAGE_MINOR_VERSION
                                  << ", mixer " << SDL_MIXER_MAJOR_VERSION << "." << SDL_MIXER_MINOR_VERSION
                                  << ", ttf " << SDL_TTF_MAJOR_VERSION << "." << SDL_TTF_MINOR_VERSION << std::endl; });
    t.join();
    return std::regex_match("ok", std::regex("o.")) ? 0 : 1;
}
EOF
# the same without SDL, for a compiler check before SDL exists
cat > "$work/plain.cpp" <<'EOF'
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <thread>
int main(int, char **) {
    auto text = std::make_unique<std::string>("no SDL");
    std::thread t([&] { std::cout << *text << std::endl; });
    t.join();
    return std::regex_match("ok", std::regex("o.")) ? 0 : 1;
}
EOF

# the highest version of a symbol family the binary needs, e.g. "2.24" for GLIBC
highest() { "$1" -V "$2" | grep -o "$3_[0-9][0-9.]*" | sed "s/$3_//" | sort -V | tail -1; }
assert_not_newer() { # assert_not_newer NAME HAVE MAX
    if [ -n "$2" ] && [ "$(printf '%s\n%s\n' "$2" "$3" | sort -V | tail -1)" != "$3" ]; then
        echo "FAIL: $1 needs $2, newer than $3" >&2; exit 1
    fi
    echo "  $1: ${2:-none} (max $3)"
}

case "$check" in
    native)
        g++ -std=c++14 -o "$work/native" "$work/test.cpp" \
            $(pkg-config --cflags --libs sdl2 SDL2_image SDL2_mixer SDL2_ttf) -pthread
        "$work/native"
        clang-format --version
        clang-tidy --version | head -2
        ;;

    pi)
        arm-linux-gnueabihf-g++ -std=c++14 -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard -Os -s \
            -o "$work/armhf" "$work/test.cpp" -lSDL2 -lSDL2_image -lSDL2_mixer -lSDL2_ttf -pthread
        file "$work/armhf" | grep -q 'ELF 32-bit LSB.*ARM, EABI5' || { file "$work/armhf"; exit 1; }
        assert_not_newer GLIBC "$(highest arm-linux-gnueabihf-readelf "$work/armhf" GLIBC)" 2.36
        aarch64-linux-gnu-g++ -std=c++14 -march=armv8-a -Os -s \
            -o "$work/arm64" "$work/test.cpp" -lSDL2 -lSDL2_image -lSDL2_mixer -lSDL2_ttf -pthread
        file "$work/arm64" | grep -q 'ELF 64-bit LSB.*ARM aarch64' || { file "$work/arm64"; exit 1; }
        assert_not_newer GLIBC "$(highest aarch64-linux-gnu-readelf "$work/arm64" GLIBC)" 2.36
        echo "  armhf: $(arm-linux-gnueabihf-g++ --version | head -1)"
        echo "  arm64: $(aarch64-linux-gnu-g++ --version | head -1)"
        ;;

    mingw)
        PKG_CONFIG_LIBDIR=/opt/mingw-sdl2/lib/pkgconfig \
        x86_64-w64-mingw32-g++-posix -std=c++14 -O2 -o "$work/win.exe" "$work/test.cpp" \
            $(PKG_CONFIG_LIBDIR=/opt/mingw-sdl2/lib/pkgconfig pkg-config --cflags --libs sdl2 SDL2_image SDL2_mixer SDL2_ttf)
        file "$work/win.exe" | grep -q 'PE32+ executable.*x86-64' || { file "$work/win.exe"; exit 1; }
        ls /opt/mingw-sdl2/bin/SDL2.dll /opt/mingw-sdl2/bin/SDL2_image.dll /opt/mingw-sdl2/bin/SDL2_mixer.dll \
           /opt/mingw-sdl2/bin/SDL2_ttf.dll >/dev/null
        echo "  $(x86_64-w64-mingw32-g++-posix --version | head -1)"
        ;;

    psc-compiler)
        libc=/opt/psc/sysroot/lib/arm-linux-gnueabihf/libc.so.6
        grep -aqE 'GNU C Library .* version 2\.24' "$libc" || { echo "FAIL: the sysroot's libc is not 2.24" >&2; exit 1; }
        /opt/psc/bin/armv8-sony-linux-gnueabihf-g++ --version | head -1
        /opt/psc/bin/armv8-sony-linux-gnueabihf-g++ -std=c++14 -march=armv8-a -mfpu=neon-vfpv4 -mfloat-abi=hard -Os -s \
            -o "$work/plain" "$work/plain.cpp" -pthread -Wl,--verbose 2>&1 | grep -E '^(attempt to open|opened script)' \
            | grep -E 'libc\.so|libstdc\+\+|crt1' | head -8 | sed 's/^/  /' || true
        /opt/psc/bin/armv8-sony-linux-gnueabihf-g++ -std=c++14 -march=armv8-a -mfpu=neon-vfpv4 -mfloat-abi=hard -Os -s \
            -o "$work/plain" "$work/plain.cpp" -pthread
        file "$work/plain" | grep -q 'ELF 32-bit LSB.*ARM, EABI5' || { file "$work/plain"; exit 1; }
        grep -q 'Tag_CPU_arch: v8' <<< "$(arm-linux-gnueabihf-readelf -A "$work/plain")" || { arm-linux-gnueabihf-readelf -A "$work/plain"; exit 1; }
        assert_not_newer GLIBC   "$(highest arm-linux-gnueabihf-readelf "$work/plain" GLIBC)"   2.24
        assert_not_newer GLIBCXX "$(highest arm-linux-gnueabihf-readelf "$work/plain" GLIBCXX)" 3.4.22
        # the console's own libstdc++ is 6.0.22 (GLIBCXX_3.4.22): the sysroot must say so
        ls /opt/psc/sysroot/usr/lib/arm-linux-gnueabihf/libstdc++.so.6.0.22 >/dev/null
        ;;

    psc)
        /opt/psc/bin/armv8-sony-linux-gnueabihf-g++ -std=c++14 -march=armv8-a -mfpu=neon-vfpv4 -mfloat-abi=hard -Os -s \
            -o "$work/psc" "$work/test.cpp" -lSDL2 -lSDL2_image -lSDL2_mixer -lSDL2_ttf -pthread
        file "$work/psc" | grep -q 'ELF 32-bit LSB.*ARM, EABI5' || { file "$work/psc"; exit 1; }
        grep -q 'Tag_CPU_arch: v8' <<< "$(arm-linux-gnueabihf-readelf -A "$work/psc")" || exit 1
        assert_not_newer GLIBC   "$(highest arm-linux-gnueabihf-readelf "$work/psc" GLIBC)"   2.24
        assert_not_newer GLIBCXX "$(highest arm-linux-gnueabihf-readelf "$work/psc" GLIBCXX)" 3.4.22
        if arm-linux-gnueabihf-readelf -d "$work/psc" | grep -qE 'RPATH|RUNPATH'; then
            echo "FAIL: RPATH/RUNPATH in the binary" >&2; exit 1
        fi
        echo "  NEEDED: $(arm-linux-gnueabihf-readelf -d "$work/psc" | grep -o 'Shared library: \[[^]]*\]' | sed 's/Shared library: //' | tr '\n' ' ')"
        # the SDL2 family as the console will see it: the sonames libs.tar.gz carries
        for so in libSDL2-2.0.so.0 libSDL2_image-2.0.so.0 libSDL2_mixer-2.0.so.0 libSDL2_ttf-2.0.so.0; do
            test -e "/opt/psc/sdl2/lib/$so" || { echo "FAIL: /opt/psc/sdl2/lib/$so missing" >&2; exit 1; }
            file -L "/opt/psc/sdl2/lib/$so" | grep -q 'ARM, EABI5' || { file -L "/opt/psc/sdl2/lib/$so"; exit 1; }
        done
        # SDL2's video backends, from the library itself
        backends="$(strings -a /opt/psc/sdl2/lib/libSDL2-2.0.so.0 | grep -xE 'wayland|x11|KMSDRM|dummy' | sort -u | tr '
' ' ')"
        echo "  SDL2 video backends: $backends"
        [[ " $backends " == *" wayland "* ]] || { echo "FAIL: no wayland backend in libSDL2" >&2; exit 1; }
        [[ " $backends " != *" x11 "* ]] || { echo "FAIL: x11 backend in libSDL2" >&2; exit 1; }
        # ...and its audio backends: ALSA is the console's sound, and a broken libasound.so in the sysroot
        # once made SDL's configure drop it without a word
        audio="$(strings -a /opt/psc/sdl2/lib/libSDL2-2.0.so.0 | grep -xE 'alsa|pulseaudio|oss|disk|dummy' | sort -u | tr '
' ' ')"
        echo "  SDL2 audio backends: $audio"
        [[ " $audio " == *" alsa "* ]] || { echo "FAIL: no alsa backend in libSDL2" >&2; exit 1; }
        ;;

    *)
        echo "usage: ab-validate {native|pi|mingw|psc-compiler|psc}" >&2
        exit 2
        ;;
esac
echo "ab-validate $check: ok"
