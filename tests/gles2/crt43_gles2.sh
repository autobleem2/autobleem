#!/usr/bin/env bash
#
# The CRT 4:3 gate on the console's renderer: the launcher at 720x480 on SDL 2.0.18 (autobleem_sdl - the console's SDL,
# built here for this machine) with the GLES2 renderer (Mesa, software), the boot splash on, every 4:3 screen walked
# with the CRT margin 0, 5 and 10 %. The VM sandbox runs the image's SDL 2.26 with desktop GL and no splash, so it
# cannot see what only the console's SDL does: CRT 4:3 round 2 died on the console's first 4:3 frame
# (SDL_SetTextureScaleMode on a GLES2 stand-in texture, SDL 2.0.18) and passed in the sandbox.
#
#   tests/gles2/crt43_gles2.sh [--sdl DIR]    on the build machine (docker, the build image)
#       --sdl DIR   an autobleem_sdl checkout with its upstream/SDL submodule (default ../autobleem_sdl)
#   AB_BUILD_IMAGE         the image (default ghcr.io/autobleem2/autobleem-build:develop)
#   AB_GLES2_SECONDS=N     how long each margin's walk repeats (default 60; the proof before a console round is 300)
#   AB_GLES2_MARGINS="0 5 10"
#
# Builds into build_gles2/ (SDL once per set of patches, then the launcher, absplash and test_crt_frame with ASan),
# lays build_gles2/usb with tools/make_usb.py, then per margin: test_crt_frame, absplash on the 4:3 twin, and the
# launcher walked through the DebugDriver until the time is up. Prints "ok: ..." / "FAIL: ..." per check, exit 1 if
# any failed. The walk's pictures are not compared - this gate is about the console's renderer not crashing; the looks
# are the sandbox shots' job.
#
set -uo pipefail
cd "$(dirname "$0")/../.."
REPO=$PWD

if [ "${1:-}" != "--inside" ]; then
    sdl="../autobleem_sdl"
    [ "${1:-}" = "--sdl" ] && sdl="${2:?--sdl DIR}"
    sdl="$(cd "$sdl" && pwd)" || { echo "no autobleem_sdl checkout at $sdl (--sdl DIR)" >&2; exit 2; }
    [ -f "$sdl/upstream/SDL/configure" ] || { echo "$sdl/upstream/SDL is empty - git submodule update --init" >&2; exit 2; }
    image="${AB_BUILD_IMAGE:-ghcr.io/autobleem2/autobleem-build:develop}"
    exec docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -e AB_GLES2_SECONDS -e AB_GLES2_MARGINS \
        -v "$REPO:$REPO" -v "$sdl:$sdl:ro" -w "$REPO" "$image" bash tests/gles2/crt43_gles2.sh --inside "$sdl"
fi

SDL_SRC="${2:?}"
OUT="$REPO/build_gles2"
SECONDS_EACH="${AB_GLES2_SECONDS:-60}"
MARGINS="${AB_GLES2_MARGINS:-0 5 10}"
FAILS=0
ok() { echo "ok: $*"; }
bad() { echo "FAIL: $*"; FAILS=$((FAILS + 1)); }
mkdir -p "$OUT"

# --- SDL 2.0.18 with our patches, for this machine: the offscreen video driver (EGL) and GLES2 -------------------
SDL_PREFIX="$OUT/sdl/install"
stamp="$(cd "$SDL_SRC" && cat VERSION patches/*.patch | sha256sum | cut -c1-16)"
if [ "$(cat "$OUT/sdl/stamp" 2>/dev/null)" != "$stamp" ]; then
    echo "== SDL 2.0.18 ($stamp)"
    rm -rf "$OUT/sdl"
    mkdir -p "$OUT/sdl"
    cp -r "$SDL_SRC/upstream/SDL" "$OUT/sdl/src"
    rm -rf "$OUT/sdl/src/.git"
    chmod -R u+w "$OUT/sdl/src"
    for p in "$SDL_SRC"/patches/*.patch; do
        patch -s -d "$OUT/sdl/src" -p1 --no-backup-if-mismatch < "$p" || { echo "patch $p failed" >&2; exit 1; }
    done
    (cd "$OUT/sdl/src" && ./configure --prefix="$SDL_PREFIX" --disable-static --enable-video-offscreen \
        --enable-video-opengles --enable-video-opengles2 --disable-video-wayland --disable-video-x11 \
        --disable-video-kmsdrm --disable-video-vulkan --disable-pulseaudio --disable-pipewire --disable-rpath \
        > "$OUT/sdl/configure.log" 2>&1 && make -j"$(nproc)" > "$OUT/sdl/make.log" 2>&1 && make install \
        > "$OUT/sdl/install.log" 2>&1) || { echo "SDL build failed - $OUT/sdl/*.log" >&2; exit 1; }
    echo "$stamp" > "$OUT/sdl/stamp"
fi
grep -qE '#define SDL_PATCHLEVEL +18' "$SDL_PREFIX/include/SDL2/SDL_version.h" || { echo "not SDL 2.0.18" >&2; exit 1; }

# --- the launcher, absplash and the frame test against it (ASan: a stale texture handle is a report, not luck) -----
echo "== launcher (build_gles2/ab)"
# (the _DEBUG flags: the root CMakeLists sets CMAKE_CXX_FLAGS itself for Debug; -I first, so <SDL2/SDL.h> is 2.0.18's)
san="-I$SDL_PREFIX/include -g -O1 -fno-omit-frame-pointer -fsanitize=address"
cmake -S . -B "$OUT/ab" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DAB_TARGET=dev -DAB_ENABLE_CHD=ON \
    -DCMAKE_PREFIX_PATH="$SDL_PREFIX" -DCMAKE_C_FLAGS_DEBUG="$san" -DCMAKE_CXX_FLAGS_DEBUG="$san" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address -L$SDL_PREFIX/lib -Wl,-rpath,$SDL_PREFIX/lib" \
    > "$OUT/configure.log" 2>&1 || { tail -20 "$OUT/configure.log"; exit 1; }
ninja -C "$OUT/ab" autobleem-gui absplash test_crt_frame > "$OUT/build.log" 2>&1 || { tail -30 "$OUT/build.log"; exit 1; }
FRAME_TEST="$(find "$OUT/ab" -name test_crt_frame -type f -perm -u+x | head -1)"

USB="$OUT/usb"
python3 tools/make_usb.py "$USB" --fresh --build "$OUT/ab" > "$OUT/make_usb.log" 2>&1 || { tail "$OUT/make_usb.log"; exit 1; }
APP="$USB/Autobleem/bin/autobleem"
cp "$OUT/ab/autobleem-gui" "$OUT/ab/absplash" "$APP/"
mkdir -p "$USB/System/Logs"

# the console's renderer, as the console runs it: GLES2 through EGL, a 720x480 window
export SDL_VIDEODRIVER=offscreen SDL_RENDER_DRIVER=opengles2 SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
    AB_WINDOW_SIZE=720x480 AB_HEADLESS=1 AB_INPUT_ISOLATED=1 AB_MAX_FPS=30 AB_ROOT="$USB" \
    ASAN_OPTIONS=detect_leaks=0 LD_LIBRARY_PATH="$SDL_PREFIX/lib"
unset AB_NO_SPLASH # the boot splash is the first 4:3 frame on the console

# one walk through every 4:3 screen the CRT shots cover (the carousel, the game menu and editor, Options, memory cards,
# resume slots, the button guide, the set picker, the quick menu, the System menu, About and the Surprise game)
OPEN="home; wait_idle 800; press down; wait 1200; wait_idle 800; press left; wait 400; press left; wait 400; press left; wait 400; press left; wait 600; wait_idle 500"
# (each `screen` puts the screen shown into the walk's log: walk-m<margin>.log lists what was walked)
WALK="wait_screen GuiLauncher; wait_idle 800; screen; $OPEN; screen; press cross; wait 2500; wait_idle 800; screen; press circle; wait 2000; wait_idle 800"
WALK="$WALK; $OPEN; press right; wait 800; wait_idle 500; press cross; wait 2500; wait_idle 800; screen; press circle; wait 2000; wait_idle 800"
WALK="$WALK; $OPEN; press right; wait 600; press right; wait 800; wait_idle 500; press cross; wait 2500; wait_idle 800; screen; press circle; wait 2000; wait_idle 800"
WALK="$WALK; $OPEN; press right; wait 600; press right; wait 600; press right; wait 800; wait_idle 500; press cross; wait 2500; wait_idle 800; screen; press circle; wait 2000; wait_idle 800"
WALK="$WALK; home; wait_idle 800; press triangle; wait 1800; wait_idle 800; screen; press circle; wait 1800; wait_idle 800"
WALK="$WALK; home; wait_idle 800; press select; wait 1800; wait_idle 800; screen; press circle; wait 1800; wait_idle 800"
WALK="$WALK; home; wait_idle 800; press up; wait 1800; wait_idle 800; screen; press circle; wait 1800; wait_idle 800"
WALK="$WALK; home; down l2; down r2; wait 1800; wait_idle 800; screen; up r2; up l2; wait 500; press circle; wait 1200"
WALK="$WALK; home; menu About; wait_screen GuiAbout; wait 2500; press right; wait 1200; press right; wait 1200; press start; wait 5000; screen; press start; wait 7000; screen; press circle; wait 1500; press circle; wait 1500; home"

PORT=7791
for margin in $MARGINS; do
    echo "== margin $margin %"
    cfg="$APP/config.ini"
    grep -v -i -E '^(outputmode|crtmargin|theme)=' "$cfg" > "$cfg.new" 2>/dev/null
    printf 'Outputmode=720x480\nCrtmargin=%s\nTheme=ab2.0.0\n' "$margin" >> "$cfg.new"
    mv "$cfg.new" "$cfg"

    # the frame test on this renderer (it must run, not skip)
    log="$OUT/frame-m$margin.log"
    if (cd "$OUT" && "$FRAME_TEST" -s > "$log" 2>&1); then
        if grep -q "renderer opengles2" "$log"; then ok "test_crt_frame on opengles2"; else bad "test_crt_frame did not run on opengles2 ($log)"; fi
    else
        bad "test_crt_frame ($log)"
    fi

    # absplash on the 4:3 twin with the theme's spinner (the console's boot and game pictures)
    log="$OUT/absplash-m$margin.log"
    (cd "$APP" && ./absplash "$APP/splash/autobleem.jpg" --seconds 3 --theme "$USB/Themes/ab2.0.0" --anim sweep \
        > "$log" 2>&1)
    rc=$?
    if [ $rc = 0 ] && grep -q "safe margin $margin%" "$log" && grep -q "the 4:3 twin" "$log"; then
        ok "absplash 4:3, margin $margin %"
    else
        bad "absplash rc $rc, margin $margin % ($log)"
    fi

    # the launcher, walked until the time is up
    log="$OUT/launcher-m$margin.log"
    rm -f "$USB/System/Logs/autobleem.log" "$OUT/walk-m$margin.log"
    (cd "$APP" && AB_DEBUG_PORT=$PORT exec ./autobleem-gui "$USB" > "$log" 2>&1) &
    pid=$!
    walks=0
    end=$((SECONDS + SECONDS_EACH))
    for _ in $(seq 120); do # the driver's port opens once the launcher is up (slower with ASan)
        python3 tools/ab_drive.py screen --port $PORT > /dev/null 2>&1 && break
        kill -0 $pid 2> /dev/null || break
        sleep 1
    done
    while [ $SECONDS -lt $end ] && kill -0 $pid 2> /dev/null; do
        if python3 tools/ab_drive.py run "$WALK" --port $PORT >> "$OUT/walk-m$margin.log" 2>&1; then
            walks=$((walks + 1))
        else
            kill -0 $pid 2> /dev/null && { bad "walk failed with the launcher still up ($OUT/walk-m$margin.log)"; break; }
        fi
    done
    if kill -0 $pid 2> /dev/null; then
        python3 tools/ab_drive.py run quit --port $PORT > /dev/null 2>&1
        for _ in $(seq 100); do kill -0 $pid 2> /dev/null || break; sleep 0.1; done
        kill $pid 2> /dev/null
    fi
    wait $pid
    rc=$?
    if grep -q -E "AddressSanitizer|SIGSEGV|Segmentation" "$log" || { [ $rc != 0 ] && [ $rc != 143 ]; }; then
        bad "launcher margin $margin %: exit $rc after $walks walks ($log)"
        grep -m1 -A12 "ERROR: AddressSanitizer" "$log"
    elif [ $walks = 0 ]; then
        bad "launcher margin $margin %: no walk finished ($OUT/walk-m$margin.log)"
    else
        ok "launcher margin $margin %: $walks walks of every 4:3 screen in ${SECONDS_EACH}s, exit $rc"
    fi
    grep -q "compiled against SDL 2.0.18, linked against SDL 2.0.18" "$log" && grep -q "Renderer: opengles2" "$log" &&
        grep -q "4:3 output 720x480" "$log" || bad "the launcher did not run on SDL 2.0.18 / opengles2 / 720x480 ($log)"
    # config.ini's margin is in place before the first frame (the boot splash): its line before the audio's (display())
    if [ "$margin" != 5 ]; then
        m=$(grep -n "CRT margin $margin%" "$log" | head -1 | cut -d: -f1)
        a=$(grep -n "Audio subsystem initialized" "$log" | head -1 | cut -d: -f1)
        if [ -n "$m" ] && [ -n "$a" ] && [ "$m" -lt "$a" ]; then ok "Crtmargin=$margin applied before the first frame"
        else bad "Crtmargin=$margin not applied before the first frame ($log)"; fi
    fi
done

[ $FAILS = 0 ] && echo "crt43 gles2: all ok" || echo "crt43 gles2: $FAILS failed"
[ $FAILS = 0 ]
