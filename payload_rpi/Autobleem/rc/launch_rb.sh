#!/usr/bin/env bash
#
# RetroArch launcher for AutoBleem on a Raspberry Pi - the Pi's version of the console's rc/launch_rb.sh,
# which hands off to RetroBoot. There is no RetroBoot here: this runs the distribution's RetroArch directly.
#
# LaunchService::launchRetroArch passes:
#   $1 the file to run
#   $2 the core. For AutoBleem's own PS1 games that is "NEON" or "PEOPS" (the two pcsx GPU plugins, both of
#      which are pcsx_rearmed here); for a game that came out of a RetroArch playlist it is the core path the
#      playlist named - RetroArchService resolves it to RetroArch/cores/<name>.so from the core info files.
set -uo pipefail

# glibc 2.41 will not dlopen a core marked as needing an executable stack (libretro's buildbot cores are)
# without this; autobleem.service sets it for the whole session, this is for a shell that did not come
# through it
export GLIBC_TUNABLES="${GLIBC_TUNABLES:-glibc.rtld.execstack=2}"

GAME_FILE="${1:-}"
CORE="${2:-}"

RC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DATA_MOUNT="$(cd "$RC_DIR/../.." && pwd)"
RA_DIR="$DATA_MOUNT/RetroArch"   # RetroArch's standard tree, laid out by install.sh
RA_CONFIG="$RA_DIR/retroarch.cfg"

echo "AUTOBLEEM: starting RetroArch"
echo "Image: $GAME_FILE"
echo "Core:  $CORE"

#*******************************
# core_path
#*******************************
# turns whatever LaunchService passed into a .so on this machine
core_path() {
    local name="$1"

    # already a path to a core
    if [ -f "$name" ]; then
        echo "$name"
        return 0
    fi

    case "$name" in
        NEON|PEOPS|"") name=pcsx_rearmed ;;     # both pcsx GPU plugins are the one libretro core here
    esac
    name="${name%_libretro.so}"

    # the cores the installer downloaded from libretro's buildbot first, then the distribution's
    local dir
    for dir in "$RA_DIR/cores" /usr/lib/arm-linux-gnueabihf/libretro /usr/lib/libretro; do
        [ -f "$dir/${name}_libretro.so" ] && { echo "$dir/${name}_libretro.so"; return 0; }
    done
    return 1
}

if ! CORE_SO="$(core_path "$CORE")"; then
    echo "AUTOBLEEM: no libretro core found for '$CORE'" >&2
    exit 1
fi

echo "Using core: $CORE_SO"
exec retroarch --config "$RA_CONFIG" -L "$CORE_SO" "$GAME_FILE"
