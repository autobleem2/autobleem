#!/bin/sh
#
# Sourced before an App's program starts - by rc/app_run.sh (a multi-platform App without a run.sh of its
# own) or by the App's own run.sh - on every Linux target: the console, the Pis, the PC stick. One file for
# all of them (autobleem-main docs/archive/app-format-plan.md); what differs is found, not configured. An App's run.sh does:
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
#
# The daemon's own library path and gamecontrollerdb.txt are the launcher's, never the App's: an App
# built for the console links the launcher's SDL2 too (see "The libraries" above), but an App with a
# library path of its own - a RetroBoot port, one of the old kind - would otherwise hand abpadd a
# different SDL2 off $LD_LIBRARY_PATH/$AB_APP_LIB, which does not know our pad database and resolves a
# pad differently from the launcher, the one thing this daemon exists not to do. So abpadd is started
# with its own environment: /tmp/lib (the launcher's SDL2) on the console, nothing extra elsewhere -
# the App's LD_LIBRARY_PATH/AB_APP_LIB above is left untouched for the App itself - and AB_PAD_DB
# points at the same gamecontrollerdb.txt file(s) the launcher loads (Env::padMappingFiles(): the
# kernel's first when there is one, then the shipped one in the resources dir).
# ---------------------------------------------------------------------------------------------
AB_PAD_DIR="$AB_ROOT/Autobleem/bin/abpad"
AB_PAD_RUNTIME_DIR=${AB_RUNTIME_DIR:-/tmp/autobleem}
AB_PAD_LD_LIBRARY_PATH=
[ -d /tmp/lib ] && AB_PAD_LD_LIBRARY_PATH=/tmp/lib
AB_PAD_DB=
[ -f /etc/autobleem/gamecontrollerdb.txt ] && AB_PAD_DB=/etc/autobleem/gamecontrollerdb.txt
if [ -f "$AB_ROOT/Autobleem/bin/autobleem/gamecontrollerdb.txt" ]; then
    AB_PAD_DB="${AB_PAD_DB:+$AB_PAD_DB:}$AB_ROOT/Autobleem/bin/autobleem/gamecontrollerdb.txt"
fi

# abpadd's own log: the stick may be read-only (or "Keep logs on the stick" points AB_LOG_DIR there
# with no write access) - a redirection to a directory that cannot be written stops the shell from
# starting the command at all, which used to leave every App without a pad. Fall back to the runtime
# dir (RAM, always writable) rather than let that failure silently drop the daemon.
AB_ABPAD_LOG_DIR=$AB_LOG_DIR
if ! mkdir -p "$AB_ABPAD_LOG_DIR" 2>/dev/null || ! : > "$AB_ABPAD_LOG_DIR/.wtest" 2>/dev/null; then
    AB_ABPAD_LOG_DIR="$AB_PAD_RUNTIME_DIR/logs"
    mkdir -p "$AB_ABPAD_LOG_DIR" 2>/dev/null
else
    rm -f "$AB_ABPAD_LOG_DIR/.wtest"
fi

# The pad output this App wants (PadMode= in app.ini, or the user's choice, which the launcher passes as
# AB_APP_PAD_MODE):
#   psc, x360             the console's own pad / a standard Xbox 360 pad, through the shim. The shim reads
#                         AB_PAD_VIRTUAL over any profile, so there is exactly one answer whatever pad.ini says.
#   psc-kernel, x360-kernel   the same pad as a real input device (abpadd --kernel): for an App no preload reaches
#                         (a static SDL, raw /dev/input). No shim then - the App finds the virtual pad and nothing else.
# Empty = the App's old behaviour (the profile's virtual =, x360 by default).
if [ -z "$AB_APP_PAD_MODE" ] && command -v ab_ini_value > /dev/null 2>&1; then
    AB_APP_PAD_MODE=$(ab_ini_value padmode | tr 'A-Z' 'a-z')
fi
AB_PAD_KERNEL=
case "$AB_APP_PAD_MODE" in
    psc) AB_PAD_VIRTUAL=psc; export AB_PAD_VIRTUAL ;;
    x360) AB_PAD_VIRTUAL=x360; export AB_PAD_VIRTUAL ;;
    psc-kernel) AB_PAD_KERNEL=psc ;;
    x360-kernel) AB_PAD_KERNEL=x360 ;;
esac
if [ -n "$AB_PAD_KERNEL" ]; then unset AB_PAD_VIRTUAL; fi
export AB_APP_PAD_MODE AB_PAD_KERNEL

# The d-pad and the stick standing in for each other (Dpad2Analog= / Analog2Dpad= in app.ini, 1 or 0, or the user's
# choice, which the launcher passes as AB_APP_DPAD2ANALOG / AB_APP_ANALOG2DPAD): Dpad2Analog - the d-pad also moves
# the left stick; Analog2Dpad - the left stick also presses the d-pad (on the console pad output: feeds its d-pad).
# One that is not set keeps the profile's default (both on); neither set = the profile decides, as before.
if command -v ab_ini_value > /dev/null 2>&1; then
    [ -n "$AB_APP_DPAD2ANALOG" ] || AB_APP_DPAD2ANALOG=$(ab_ini_value dpad2analog)
    [ -n "$AB_APP_ANALOG2DPAD" ] || AB_APP_ANALOG2DPAD=$(ab_ini_value analog2dpad)
fi
ab_flag() {
    case "$(echo "$1" | tr 'A-Z' 'a-z')" in
        1 | true | yes | on) echo 1 ;;
        0 | false | no | off) echo 0 ;;
    esac
}
AB_APP_DPAD2ANALOG=$(ab_flag "$AB_APP_DPAD2ANALOG")
AB_APP_ANALOG2DPAD=$(ab_flag "$AB_APP_ANALOG2DPAD")
if [ -n "$AB_APP_DPAD2ANALOG$AB_APP_ANALOG2DPAD" ]; then
    case "${AB_APP_DPAD2ANALOG:-1}${AB_APP_ANALOG2DPAD:-1}" in
        11) AB_PAD_MOVEMENT=both ;;
        10) AB_PAD_MOVEMENT=dpad-to-stick ;;
        01) AB_PAD_MOVEMENT=stick-to-dpad ;;
        *) AB_PAD_MOVEMENT=as-is ;;
    esac
    export AB_PAD_MOVEMENT
fi
export AB_APP_DPAD2ANALOG AB_APP_ANALOG2DPAD

# the held pads' nodes (the kernel pad) and the Reset button are hidden from the App: abpadd writes the list, the App
# is started through "$AB_PAD_DIR/abpadd" --hide-run "$AB_PAD_HIDE_LIST" -- <program> (rc/app_run.sh, rc/pe_run.sh)
AB_PAD_HIDE_LIST=/tmp/abpad.state.hide
AB_PAD_HIDE= # 1 once the daemon has written the list for this run
export AB_PAD_HIDE_LIST AB_PAD_HIDE

if [ "$AB_APP_VIRTUAL_PAD" != 0 ] && [ -x "$AB_PAD_DIR/abpadd" ] && [ -f "$AB_PAD_DIR/libabpad.so" ]; then
    # a block left by the last run would pass for the new daemon being ready (the wait below looks for the file)
    rm -f /tmp/abpad.state /tmp/abpad.state.mappings "$AB_PAD_HIDE_LIST"
    env LD_LIBRARY_PATH="$AB_PAD_LD_LIBRARY_PATH" AB_PAD_DB="$AB_PAD_DB" \
        "$AB_PAD_DIR/abpadd" ${AB_PAD_KERNEL:+--kernel $AB_PAD_KERNEL} --watch-pid $$ > "$AB_ABPAD_LOG_DIR/abpadd.log" 2>&1 &

    # the daemon lets a pad settle before publishing (a multi-mode pad is taken over by hidapi a
    # second or two after it is first opened), so wait for it rather than have the App ask too early
    ab_waited=0
    while [ ! -f /tmp/abpad.state ] && [ $ab_waited -lt 100 ]; do
        ab_waited=$((ab_waited + 1))
        sleep 0.1
    done

    AB_PAD_LOG="$AB_ABPAD_LOG_DIR/abpad.log"
    export AB_PAD_LOG
    # one App's lines: the shim and abpadd --hide-run append to it, so without this the trail of an App the shim never
    # reached (a kernel-pad App) showed the last shim App's lines as its own
    : > "$AB_PAD_LOG" 2>/dev/null
    if [ -n "$AB_PAD_KERNEL" ]; then
        # the kernel pad: no shim in front of the App's SDL (it would translate a pad that is already the right
        # one), and no hidapi in it either - that would find the real pad through /dev/hidraw, past the grab
        export SDL_JOYSTICK_HIDAPI=0
    else
        export LD_PRELOAD="$AB_PAD_DIR/libabpad.so"
    fi
    # the held pads (the kernel pad) and the console's Reset button (any mode: a program that grabs every event node
    # would take Reset from the daemon) - only when the daemon listed something
    if [ -s "$AB_PAD_HIDE_LIST" ]; then
        AB_PAD_HIDE=1
        export AB_PAD_HIDE
    fi

    # the defaults, then this App's own on top - either may be absent
    AB_PAD_DEFAULTS_FILE="$AB_ROOT"/Autobleem/rc/pad.default.ini
    [ -f "$AB_PAD_DEFAULTS_FILE" ] && export AB_PAD_DEFAULTS="$AB_PAD_DEFAULTS_FILE"
    [ -f "$AB_APP_DIR/pad.ini" ] && export AB_PAD_PROFILE="$AB_APP_DIR/pad.ini"

    # For an App the preload cannot reach - one statically linked against SDL - the mapping the
    # daemon actually resolved. Deliberately not our gamecontrollerdb.txt: a file given this way
    # overrides SDL's built-in table, and for a pad SDL already knows the built-in entry is the right
    # one while ours may be a stale line for another of that pad's modes.
    if [ -z "$AB_PAD_KERNEL" ] && [ -f /tmp/abpad.state.mappings ]; then export SDL_GAMECONTROLLERCONFIG_FILE=/tmp/abpad.state.mappings; fi
elif [ -d /usr/sony ] && [ -x "$AB_PAD_DIR/abpadd" ]; then
    # The console's Reset button ends every App (the owner's rule, 2026-09-25): an App that reads the
    # pads itself still gets the daemon, in the mode that only watches Reset (no SDL, no preload) -
    # still needs the launcher's own library path to find libSDL2 at all.
    env LD_LIBRARY_PATH="$AB_PAD_LD_LIBRARY_PATH" \
        "$AB_PAD_DIR/abpadd" --exit-only --watch-pid $$ > "$AB_ABPAD_LOG_DIR/abpadd.log" 2>&1 &
fi

# ---------------------------------------------------------------------------------------------
# The App's processes, and the one way they end (rc/app_run.sh, rc/pe_run.sh).
#
# An App is its program and everything that program starts, however it starts it: a child, a child's child, a
# program whose parent has already gone (it is then init's, and no walk down from the App finds it any more), one in
# a session of its own. So the runner starts the program with a mark in its environment, AB_APP_ID - inherited by
# everything below it - and the App's processes are the ones that carry the mark, and everything below them.
#
# The runner does not exec the program: it stays its parent, so it is the one that ends the App, and the launcher
# (which waits for the runner) never comes back while any of it runs:
#   - the program ends by itself: whatever it left running is stopped (a mod's launch.sh that ended while the game it
#     started played on, holding the screen - OpenJazz on the console, 2026-10-05);
#   - a TERM/INT/HUP (Reset, Start+Select: abpadd TERMs the runner, which it watches, and KILLs it 1.5 to 3 s later)
#     stops all of it.
# Either way: TERM to every process of the App, a second for them to go, KILL to what is left, then up to 2 s for the
# kernel to take them (one stuck in a driver may not go: the log names it). The runner's own pid, abpadd and the
# runner's helpers do not carry the mark and are never touched.
#
#   ab_app_start PROGRAM ARGS...   starts it in the background (AB_APP_PID), its stdin the runner's; the program is
#                                  a file to run, not a shell function
#   ab_app_wait                    waits for it, then stops what it left; returns its status (143 after a stop)
#   ab_app_on_term                 the TERM/INT/HUP trap: trap ab_app_on_term TERM INT HUP
# AB_APP_LOG may name a function that takes a line for the log (pe_run.sh: pe_log); stderr otherwise.
# ---------------------------------------------------------------------------------------------
AB_APP_PID=
AB_APP_STOPPED=

ab_app_log() {
    if [ -n "$AB_APP_LOG" ]; then "$AB_APP_LOG" "$*"; else echo "app: $*" >&2; fi
}

# ab_app_live PIDS: the ones that still run (gone and zombies left out); /proc, no forks
ab_app_live() {
    ab_live=
    for ab_p in "$@"; do
        read -r ab_line < "/proc/$ab_p/stat" 2>/dev/null || continue
        set -- ${ab_line##*) } # after the command name (which may hold blanks): "S ppid pgrp ..."
        case "$1" in Z | X | x) continue ;; esac
        ab_live="$ab_live $ab_p"
    done
    echo $ab_live
}

# ab_app_procs [all]: the App's processes that still run - the ones with the mark, everything below them, and with
# "all" the program itself (it may have cleared its environment)
ab_app_procs() {
    ab_found=" "
    [ "$1" = all ] && [ -n "$AB_APP_PID" ] && ab_found=" $AB_APP_PID "
    if [ -n "$AB_APP_ID" ]; then
        # the mark is "x<runner pid>x<seconds>x": no App's mark is the start of another's
        for ab_e in $(grep -l -F "AB_APP_ID=$AB_APP_ID" /proc/[0-9]*/environ 2>/dev/null); do
            ab_p=${ab_e#/proc/}
            ab_p=${ab_p%/environ}
            case "$ab_found" in *" $ab_p "*) ;; *) ab_found="$ab_found$ab_p " ;; esac
        done
    fi
    ab_more=1
    while [ "$ab_more" = 1 ] && [ "$ab_found" != " " ]; do
        ab_more=0
        for ab_st in /proc/[0-9]*/stat; do
            ab_p=${ab_st#/proc/}
            ab_p=${ab_p%/stat}
            case "$ab_found" in *" $ab_p "*) continue ;; esac
            read -r ab_line < "$ab_st" 2>/dev/null || continue
            set -- ${ab_line##*) }
            case "$ab_found" in *" $2 "*) ab_found="$ab_found$ab_p " ab_more=1 ;; esac
        done
    done
    ab_app_live $ab_found
}

# ab_app_names PIDS: "pid (command line)" each, for the log
ab_app_names() {
    for ab_p in "$@"; do
        printf '%s (%s) ' "$ab_p" "$(tr '\0' ' ' < "/proc/$ab_p/cmdline" 2>/dev/null | cut -c1-80)"
    done
}

# ab_app_stop [all] WHY: TERM, a second, KILL, up to 2 s more - see above
ab_app_stop() {
    ab_mode=$1
    ab_left=$(ab_app_procs "$ab_mode")
    [ -n "$ab_left" ] || return 0
    ab_app_log "$2 - TERM to $(ab_app_names $ab_left)"
    kill -TERM $ab_left 2>/dev/null
    # the known ones only while waiting (cheap); the whole App again before the KILL
    ab_n=0
    while [ -n "$ab_left" ] && [ "$ab_n" -lt 10 ]; do
        sleep 0.1
        ab_left=$(ab_app_live $ab_left)
        ab_n=$((ab_n + 1))
    done
    ab_left=$(ab_app_procs "$ab_mode")
    [ -n "$ab_left" ] || return 0
    ab_app_log "still running a second after the TERM - KILL to $(ab_app_names $ab_left)"
    kill -KILL $ab_left 2>/dev/null
    ab_n=0
    while [ -n "$ab_left" ] && [ "$ab_n" -lt 20 ]; do
        sleep 0.1
        ab_left=$(ab_app_live $ab_left)
        ab_n=$((ab_n + 1))
    done
    [ -z "$ab_left" ] || ab_app_log "still there after the KILL (held in the kernel): $(ab_app_names $ab_left)"
}

ab_app_on_term() {
    AB_APP_STOPPED=1
    [ -n "$AB_APP_PID" ] || return 0
    ab_app_stop all "told to stop"
}

ab_app_start() {
    AB_APP_ID="x$$x$(date +%s 2>/dev/null)x"
    # the explicit stdin: a background command of a shell without job control would get /dev/null (none open: as is)
    if { : <&0; } 2>/dev/null; then
        AB_APP_ID=$AB_APP_ID "$@" <&0 &
    else
        AB_APP_ID=$AB_APP_ID "$@" &
    fi
    AB_APP_PID=$!
    # a TERM that came before the pid was known
    [ -z "$AB_APP_STOPPED" ] || ab_app_stop all "told to stop while starting"
}

ab_app_wait() {
    wait "$AB_APP_PID"
    ab_rc=$?
    # a trap interrupts wait: go on until the program is really gone
    while kill -0 "$AB_APP_PID" 2>/dev/null; do
        wait "$AB_APP_PID"
        ab_rc=$?
    done
    ab_app_stop "" "the program ended ($ab_rc) and left these running"
    [ -z "$AB_APP_STOPPED" ] || ab_rc=143
    return "$ab_rc"
}
