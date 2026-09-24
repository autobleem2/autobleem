#!/usr/bin/env bash
#
# PS1 launcher for AutoBleem on a Raspberry Pi - the Pi's version of the console's rc/launch.sh, running the
# same emulator: pcsx-ab (the Pi build from pcsx-rearmed-develop's make_rpi.sh, shipped in Autobleem/bin/emu
# with its GPU plugins). The run directory is put together the way the console's script does it, so pcsx-ab
# finds everything where it expects: .pcsx -> the game's !SaveStates folder (pcsx.cfg, memory cards, save
# states), bios -> System/Bios, plugins -> the emu's plugins.
#
# LaunchService::launchPcsx passes, in this order:
#   $1 ssFolder   the game's !SaveStates folder      $6 resume   1 to load the resume state, 0 for a cold boot
#   $2 cdfile     the .cue/.pbp/.chd to run          $7 aspect   1 widescreen, 0 4:3
#   $3 lang                                          $8 filter   0 Off, 1 Linear, 2 Sharp
#   $4 region     (the console's script ignores it   $9 pad      always "NA"
#                 and passes -region 4; so does this)
#   $5 gameFolder the game's own folder            $10 emulator  pcsx-ab (bin/emu) or pcsx-abnxt (bin/emunxt)
#                                                  $11 language  config.ini's (English, Polski, ...): pcsx-abnxt's
#                                                                own screens speak it (its lang/<Name>.txt);
#                                                                the classic pcsx-ab takes no such option
#
# BIOS: pcsx.cfg says "Bios = SET_BY_PCSX", and pcsx-ab resolves that to bios/romw.bin - or bios/romJP.bin
# for a game whose serial starts with SLP/SCP - so System/Bios needs both (the console copies romw.bin as
# romJP.bin). Without them pcsx-ab runs on its HLE BIOS, which many games tolerate and some do not.
set -uo pipefail

SS_FOLDER="${1:-}"
CD_FILE="${2:-}"
LANG_ID="${3:-2}"
GAME_FOLDER="${5:-}"
RESUME="${6:-0}"
ASPECT="${7:-0}"
FILTER="${8:-0}"
LANGUAGE="${11:-}"

RC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DATA_MOUNT="$(cd "$RC_DIR/../.." && pwd)"
# which emulator: pcsx-ab (Autobleem/bin/emu, the one AutoBleem has always shipped) or pcsx-abnxt
# (Autobleem/bin/emunxt, the next one); the binary is pcsx-ab in either folder
case "${10:-pcsx-abnxt}" in
  pcsx-abnxt) EMU_DIR="$DATA_MOUNT/Autobleem/bin/emunxt" ;;
  *)          EMU_DIR="$DATA_MOUNT/Autobleem/bin/emu" ;;
esac
if [ ! -f "$EMU_DIR/pcsx-ab" ]; then
  echo "AUTOBLEEM: no $EMU_DIR/pcsx-ab - falling back to Autobleem/bin/emu"
  EMU_DIR="$DATA_MOUNT/Autobleem/bin/emu"
fi
echo "AUTOBLEEM: emulator $EMU_DIR"
# FILTER is the game's filter as pcsx-abnxt numbers it (0 Off, 1 Linear, 2 Sharp) and nxt gets it as is;
# the classic pcsx-ab counts the other way round (0 bilinear, 1 nearest) and has no Sharp, so Sharp is Off
if [ "$EMU_DIR" != "$DATA_MOUNT/Autobleem/bin/emunxt" ]; then
    if [ "$FILTER" = "1" ]; then FILTER=0; else FILTER=1; fi
fi
BIOS_DIR="$DATA_MOUNT/System/Bios"
RA_DIR="$DATA_MOUNT/RetroArch"
RUN_DIR=/tmp/runpcsx

echo "AUTOBLEEM: starting PS1 game"
echo "Cmd: $*"

# the per-game pcsx.cfg belongs next to the save states, the way the console's launch.sh puts it there
if [ -f "$GAME_FOLDER/pcsx.cfg" ] && [ -n "$SS_FOLDER" ]; then
    cp -f "$GAME_FOLDER/pcsx.cfg" "$SS_FOLDER/pcsx.cfg"
fi

#*******************************
# pcsx-ab
#*******************************
if [ -f "$EMU_DIR/pcsx-ab" ]; then
    # the console copies the emulator to /tmp/pcsx and runs it from there; doing the same keeps the two scripts
    # alike, and makes the exec bit a non-question whatever the data partition's mount options are
    cp -f "$EMU_DIR/pcsx-ab" /tmp/pcsx
    chmod +x /tmp/pcsx

    rm -rf "$RUN_DIR"
    mkdir -p "$RUN_DIR" "$BIOS_DIR"
    cd "$RUN_DIR" || exit 1

    ln -s "$SS_FOLDER" "$RUN_DIR/.pcsx"
    ln -s "$BIOS_DIR" "$RUN_DIR/bios"
    ln -s "$EMU_DIR/plugins" "$RUN_DIR/plugins"
    # the in-game menu's skin, if the emu ships one (the console's package does not)
    [ -d "$EMU_DIR/skin" ] && ln -s "$EMU_DIR/skin" "$RUN_DIR/skin"
    # pcsx-abnxt's own screens: its language files, and the launcher's fonts folder for a language whose
    # glyphs its own font lacks (Chinese)
    [ -d "$EMU_DIR/lang" ] && ln -s "$EMU_DIR/lang" "$RUN_DIR/lang"
    [ -d "$DATA_MOUNT/Autobleem/bin/autobleem/fonts" ] && ln -s "$DATA_MOUNT/Autobleem/bin/autobleem/fonts" "$RUN_DIR/fonts"
    LANG_OPT=()
    if [ "$EMU_DIR" = "$DATA_MOUNT/Autobleem/bin/emunxt" ] && [ -n "$LANGUAGE" ]; then
        LANG_OPT=(-language "$LANGUAGE")
    fi

    [ -f "$BIOS_DIR/romw.bin" ] || echo "AUTOBLEEM: no $BIOS_DIR/romw.bin - pcsx-ab will use its HLE BIOS"

    if [ "$RESUME" = "0" ]; then
        /tmp/pcsx -filter "$FILTER" -ratio "$ASPECT" -lang "$LANG_ID" -region 4 -enter 1 "${LANG_OPT[@]}" -cdfile "$CD_FILE"
    else
        /tmp/pcsx -filter "$FILTER" -ratio "$ASPECT" -lang "$LANG_ID" -region 4 -enter 1 -load "$RESUME" "${LANG_OPT[@]}" -cdfile "$CD_FILE"
    fi

    echo FINISHED
    exit 0
fi

#*******************************
# RetroArch fallback
#*******************************
# Only if the package somehow shipped without pcsx-ab. pcsx_rearmed plays the game but cannot read
# AutoBleem's save-state slots, so "Resume" starts from the beginning.
echo "AUTOBLEEM: no $EMU_DIR/pcsx-ab - falling back to RetroArch" >&2

# the cores the installer downloaded first, then the distribution's under its multiarch dir (whichever
# architecture this is: arm-linux-gnueabihf, aarch64-linux-gnu, i386-linux-gnu)
CORE=""
for candidate in \
    "$RA_DIR/cores/pcsx_rearmed_libretro.so" \
    /usr/lib/*-linux-gnu*/libretro/pcsx_rearmed_libretro.so \
    /usr/lib/libretro/pcsx_rearmed_libretro.so; do
    [ -f "$candidate" ] && { CORE="$candidate"; break; }
done

if [ -z "$CORE" ]; then
    echo "AUTOBLEEM: no pcsx_rearmed core either - cannot run this game" >&2
    exit 1
fi

echo "Using RetroArch core: $CORE"
exec retroarch --config "$RA_DIR/retroarch.cfg" -L "$CORE" "$CD_FILE"
