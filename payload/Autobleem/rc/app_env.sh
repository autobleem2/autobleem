#!/bin/sh
#
# Sourced before an App's program starts - by rc/app_run.sh (a multi-platform App without a run.sh of its
# own) or by the App's own run.sh - on every Linux target: the console, the Pis, the PC stick. One file for
# all of them (docs/app-format-plan.md); what differs is found, not configured. An App's run.sh does:
#
#     #!/bin/sh
#     . "$(dirname "$0")/../../Autobleem/rc/app_env.sh"
#     cd "$AB_APP_DIR" || exit 1
#     exec "$AB_APP_EXEC" "$@"        # a multi-platform App: what its app.ini names for this machine
#     exec ./the-game                  # an App of the old kind: its one binary
#
# It is sourced, not run, so "$0" is the App's run.sh - which is what lets the data root be found without
# anything being hardcoded when the launcher did not say (an App is assumed to live at <root>/Apps/<name>/).
#
# What it sets up: the App's folder and root, which binary (app_resolve.sh), the libraries, a home on the
# stick, and the virtual gamepad - one section each below.

# ---------------------------------------------------------------------------------------------
# Where. The launcher exports AB_APP_DIR and AB_ROOT (and the resolved AB_APP_*); a run.sh started by hand
# finds them from its own path.
# ---------------------------------------------------------------------------------------------
[ -n "$AB_APP_DIR" ] || AB_APP_DIR=$(cd "$(dirname "$0")" && pwd)
[ -n "$AB_ROOT" ] || AB_ROOT=$(cd "$AB_APP_DIR/../.." && pwd)
# the launcher's logs dir (RAM unless the logs are kept - rc/ab_log.sh); an App started by hand logs on the stick
[ -n "$AB_LOG_DIR" ] || AB_LOG_DIR="$AB_ROOT/System/Logs"
export AB_APP_DIR AB_ROOT AB_LOG_DIR
mkdir -p "$AB_LOG_DIR" 2>/dev/null

# ---------------------------------------------------------------------------------------------
# Which binary. The launcher resolved the App's app.ini already; by hand it is resolved here, by the same
# rule (app_resolve.sh). An App of the old kind (no Exec= in its ini) resolves to nothing and runs its own
# binary as it always did.
# ---------------------------------------------------------------------------------------------
if [ -f "$AB_ROOT/Autobleem/rc/app_resolve.sh" ]; then
    . "$AB_ROOT/Autobleem/rc/app_resolve.sh"
    [ -n "$AB_APP_EXEC" ] || ab_resolve_app
    # VirtualPad= in app.ini: whether this App runs with the virtual pad mapper below (absent = yes); the
    # launcher passes it as AB_APP_VIRTUAL_PAD, by hand it is read here
    if [ -z "$AB_APP_VIRTUAL_PAD" ] && [ -f "$AB_APP_DIR/app.ini" ]; then
        case "$(ab_ini_value virtualpad | tr 'A-Z' 'a-z')" in
            false | no | 0 | off) AB_APP_VIRTUAL_PAD=0 ;;
            *) AB_APP_VIRTUAL_PAD=1 ;;
        esac
    fi
fi
[ -n "$AB_APP_VIRTUAL_PAD" ] || AB_APP_VIRTUAL_PAD=1
export AB_APP_VIRTUAL_PAD

# ---------------------------------------------------------------------------------------------
# The libraries.
#
# The console: what the apps need beyond its firmware - SDL2_image/mixer/ttf, freetype, png, vorbis, ...
# (the site's libs pack, unpacked by the installer into Autobleem/lib/apps) - linked into /tmp/applib on
# first use, with the shorter soname links the loader looks for (the stick is FAT, it cannot hold a
# symlink), and made the library path. Was RetroBoot's init_libs.sh / RB_LIBRARY_PATH. Only the console has
# that folder; a Pi or a PC has a real distribution underneath and needs none of it.
#
# A multi-platform App built for the console (AB_APP_KEY=psc) is built against the launcher's own SDL2 family
# (autobleem-main docs/decisions.md, "Third-party App ports"), which is in /tmp/lib - unpacked at boot from
# Autobleem/lib/libs.tar.gz - so that goes ahead of the pack. An App of the old kind keeps the path it had.
#
# Every target: the App's own libraries for this platform (Lib= in its ini, AB_APP_LIB) go first.
# ---------------------------------------------------------------------------------------------
APPLIB=/tmp/applib
APPLIB_SRC="$AB_ROOT/Autobleem/lib/apps"
if [ -d "$APPLIB_SRC" ]; then
    if [ ! -d "$APPLIB" ]; then
        mkdir -p "$APPLIB"
        for lib in "$APPLIB_SRC"/*.so*; do
            [ -f "$lib" ] || continue
            name=$(basename "$lib")
            ln -sf "$lib" "$APPLIB/$name"
            # libfoo.so.1.2.3 -> libfoo.so.1.2, libfoo.so.1, libfoo.so
            short=$name
            while extn=$(echo "$short" | sed -n '/\.[0-9][0-9]*$/s/.*\(\.[0-9][0-9]*\)$/\1/p'); [ -n "$extn" ]; do
                short=$(basename "$short" "$extn")
                [ -e "$APPLIB/$short" ] || ln -sf "$lib" "$APPLIB/$short"
            done
        done
    fi
    LD_LIBRARY_PATH=$APPLIB
fi
if [ "$AB_APP_KEY" = psc ] && [ -d /tmp/lib ]; then
    LD_LIBRARY_PATH="/tmp/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
if [ -n "$AB_APP_LIB" ]; then
    LD_LIBRARY_PATH="$AB_APP_LIB${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export LD_LIBRARY_PATH

# ---------------------------------------------------------------------------------------------
# A home on the stick.
#
# An App left alone writes its settings and saves wherever the distribution tells it to - Chocolate
# Doom announces "Using /root/.local/share/chocolate-doom/ for configuration and saves" - which on a
# console means writing to the machine's own storage, and that is not ours to write to. It matters
# less on a Pi or a PC, where the root filesystem is the user's own, but the behaviour is the same
# everywhere on purpose: an App's saves belong beside the games, on the partition that gets backed
# up and carried about, not in a dot-directory of whatever account the launcher happens to run as.
# ---------------------------------------------------------------------------------------------
HOME="$AB_ROOT/Home"
XDG_DATA_HOME="$HOME/.local/share"
XDG_CONFIG_HOME="$HOME/.config"
XDG_CACHE_HOME="${AB_RUNTIME_DIR:-/tmp/autobleem}/cache" # a cache is not a save: RAM, not the stick
XDG_STATE_HOME="$HOME/.local/state"
export HOME XDG_DATA_HOME XDG_CONFIG_HOME XDG_CACHE_HOME XDG_STATE_HOME
mkdir -p "$XDG_DATA_HOME" "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" "$XDG_STATE_HOME" 2>/dev/null

# ---------------------------------------------------------------------------------------------
# The virtual gamepad (docs/virtual-gamepad-plan.md).
#
# abpadd reads the pads through SDL's GameController API with our own database - the same code and
# the same file the launcher uses, so a pad resolves in here exactly as it does out there - and
# libabpad.so, preloaded, shows the App a pad it understands whichever SDL API it reads. Neither is
# required: an App whose folder has no pad.ini and a tree with no abpad installed simply run as they
# always did, and an App whose app.ini says VirtualPad=false (it reads the pads its own way, or has no
# use for one) gets neither - on the console only the daemon's Reset watch (--exit-only).
#
# Either way out ends the App: holding Start+Select (the shim asks, the daemon terminates, then
# kills), and on the console a press of Reset (the daemon asks through the shim, then terminates and
# kills - see ResetWatch in abpadd.cpp).
#
# The daemon is given this shell's pid to watch, so it goes when the App goes - including when the
# App is exec'd over this shell, which keeps the same pid, and including when the App crashes.
# ---------------------------------------------------------------------------------------------
AB_PAD_DIR="$AB_ROOT/Autobleem/bin/abpad"
if [ "$AB_APP_VIRTUAL_PAD" != 0 ] && [ -x "$AB_PAD_DIR/abpadd" ] && [ -f "$AB_PAD_DIR/libabpad.so" ]; then
    "$AB_PAD_DIR/abpadd" --watch-pid $$ > "$AB_LOG_DIR/abpadd.log" 2>&1 &

    # the daemon lets a pad settle before publishing (a multi-mode pad is taken over by hidapi a
    # second or two after it is first opened), so wait for it rather than have the App ask too early
    ab_waited=0
    while [ ! -f /tmp/abpad.state ] && [ $ab_waited -lt 60 ]; do
        ab_waited=$((ab_waited + 1))
        sleep 0.1
    done

    AB_PAD_LOG="$AB_LOG_DIR/abpad.log"
    export AB_PAD_LOG
    export LD_PRELOAD="$AB_PAD_DIR/libabpad.so"

    # the defaults, then this App's own on top - either may be absent
    AB_PAD_DEFAULTS_FILE="$AB_ROOT"/Autobleem/rc/pad.default.ini
    [ -f "$AB_PAD_DEFAULTS_FILE" ] && export AB_PAD_DEFAULTS="$AB_PAD_DEFAULTS_FILE"
    [ -f "$AB_APP_DIR/pad.ini" ] && export AB_PAD_PROFILE="$AB_APP_DIR/pad.ini"

    # For an App the preload cannot reach - one statically linked against SDL - the mapping the
    # daemon actually resolved. Deliberately not our gamecontrollerdb.txt: a file given this way
    # overrides SDL's built-in table, and for a pad SDL already knows the built-in entry is the right
    # one while ours may be a stale line for another of that pad's modes.
    [ -f /tmp/abpad.state.mappings ] && export SDL_GAMECONTROLLERCONFIG_FILE=/tmp/abpad.state.mappings
elif [ -d /usr/sony ] && [ -x "$AB_PAD_DIR/abpadd" ]; then
    # The console's Reset button ends every App (the owner's rule, 2026-09-25): an App that reads the
    # pads itself still gets the daemon, in the mode that only watches Reset (no SDL, no preload).
    "$AB_PAD_DIR/abpadd" --exit-only --watch-pid $$ > "$AB_LOG_DIR/abpadd.log" 2>&1 &
fi
