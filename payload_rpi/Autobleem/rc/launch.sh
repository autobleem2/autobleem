#!/usr/bin/env bash
#
# PS1 launcher for AutoBleem on a Raspberry Pi - the Pi's version of the console's rc/launch.sh.
#
# LaunchService::launchPcsx passes, in this order:
#   $1 ssFolder   the game's !SaveStates folder      $6 resume   1 to load the resume state, 0 for a cold boot
#   $2 cdfile     the .cue/.pbp/.chd to run          $7 aspect   1 widescreen, 0 4:3
#   $3 lang                                          $8 filter   1 bilinear, 0 nearest
#   $4 region                                        $9 pad      always "NA"
#   $5 gameFolder the game's own folder
#
# pcsx-ab is not ported to the Pi yet. While it is missing this hands the game to RetroArch's pcsx_rearmed
# core, which plays it but ignores AutoBleem's own save-state slots (they are pcsx-ab's format, not
# RetroArch's) - so "Resume" will start the game from the beginning. Drop a Pi build of pcsx-ab into
# Autobleem/bin/emu/ and this script uses it instead, with no other change anywhere.
set -uo pipefail

SS_FOLDER="${1:-}"
CD_FILE="${2:-}"
LANG_ID="${3:-2}"
REGION="${4:-4}"
GAME_FOLDER="${5:-}"
RESUME="${6:-0}"
ASPECT="${7:-0}"
FILTER="${8:-0}"

RC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DATA_MOUNT="$(cd "$RC_DIR/../.." && pwd)"
EMU_DIR="$DATA_MOUNT/Autobleem/bin/emu"
RA_DIR="$DATA_MOUNT/retroarch"

echo "AUTOBLEEM: starting PS1 game"
echo "Cmd: $*"

# the per-game pcsx.cfg belongs next to the save states, the way the console's launch.sh puts it there
if [ -f "$GAME_FOLDER/pcsx.cfg" ] && [ -n "$SS_FOLDER" ]; then
    cp -f "$GAME_FOLDER/pcsx.cfg" "$SS_FOLDER/pcsx.cfg"
fi

#*******************************
# pcsx-ab, once it is ported
#*******************************
if [ -x "$EMU_DIR/pcsx-ab" ]; then
    echo "Using pcsx-ab"

    RUN_DIR=/tmp/runpcsx
    rm -rf "$RUN_DIR"
    mkdir -p "$RUN_DIR"
    cd "$RUN_DIR" || exit 1

    ln -s "$SS_FOLDER" "$RUN_DIR/.pcsx"
    ln -s "$DATA_MOUNT/System/Bios" "$RUN_DIR/bios"
    ln -s "$EMU_DIR/plugins" "$RUN_DIR/plugins"

    if [ "$RESUME" = "0" ]; then
        "$EMU_DIR/pcsx-ab" -filter "$FILTER" -ratio "$ASPECT" -lang "$LANG_ID" -region "$REGION" \
            -enter 1 -cdfile "$CD_FILE"
    else
        "$EMU_DIR/pcsx-ab" -filter "$FILTER" -ratio "$ASPECT" -lang "$LANG_ID" -region "$REGION" \
            -enter 1 -load "$RESUME" -cdfile "$CD_FILE"
    fi

    echo FINISHED
    exit 0
fi

#*******************************
# RetroArch fallback
#*******************************
CORE=""
for candidate in \
    /usr/lib/arm-linux-gnueabihf/libretro/pcsx_rearmed_libretro.so \
    /usr/lib/libretro/pcsx_rearmed_libretro.so \
    "$RA_DIR/cores/pcsx_rearmed_libretro.so"; do
    [ -f "$candidate" ] && { CORE="$candidate"; break; }
done

if [ -z "$CORE" ]; then
    echo "AUTOBLEEM: no pcsx_rearmed core and no pcsx-ab - cannot run this game" >&2
    echo "Install one with: sudo apt install libretro-pcsx-rearmed" >&2
    exit 1
fi

echo "Using RetroArch core: $CORE"
exec retroarch --config "$RA_DIR/retroarch.cfg" -L "$CORE" "$CD_FILE"
