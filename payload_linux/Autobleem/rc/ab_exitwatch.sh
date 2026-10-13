#!/bin/sh
#
# The one way out beside a program that reads the pads itself (RetroArch on the console, the Pi and the PC stick;
# rc/app_env.sh uses the same daemon mode for an App with VirtualPad=false): holding Start+Select, or Home (the Guide
# button), for two seconds ends the program - SIGTERM, which lets RetroArch save its SRAM and quit cleanly, and SIGKILL
# three seconds later if it is still there. The pads are only read, never grabbed: the program keeps every press.
# It is `abpadd --exit-only --pads` (apps/abpad/src/daemon/abpadd.cpp, exitWatch).
#
# Sourced; the same file is in payload/ and payload_linux/ (one rule, two trees).
#
#   ab_exitwatch_start ROOT PID [--no-reset]   ROOT is the stick's root (the folder with Autobleem/); PID the program,
#                                              which may be this shell when it is about to exec the program (the pid
#                                              stays). --no-reset leaves the console's Reset button to the program.
#   ab_exitwatch_stop                          the watcher ends by itself when the program does; this is for a script
#                                              that carries on after the program (the console's launch_rb.sh)
#   ab_exitwatch_fired                         true when the watcher ended the program: its status is then not a crash

AB_EXITWATCH_PID=
AB_EXITWATCH_LOG=${AB_RUNTIME_DIR:-/tmp/autobleem}/abpadd-exit.log

ab_exitwatch_start() {
    AB_EXITWATCH_PID=
    ab_ew_dir="$1/Autobleem/bin/abpad"
    [ -x "$ab_ew_dir/abpadd" ] || return 0
    mkdir -p "$(dirname "$AB_EXITWATCH_LOG")" 2>/dev/null
    : > "$AB_EXITWATCH_LOG" 2>/dev/null || AB_EXITWATCH_LOG=/dev/null
    # the launcher's own libraries (the console's SDL2 is in /tmp/lib) and pad database, never the program's: a pad
    # must resolve here exactly as it does in the launcher (rc/app_env.sh says why)
    ab_ew_libs=
    [ -d /tmp/lib ] && ab_ew_libs=/tmp/lib
    ab_ew_db=
    [ -f /etc/autobleem/gamecontrollerdb.txt ] && ab_ew_db=/etc/autobleem/gamecontrollerdb.txt
    if [ -f "$1/Autobleem/bin/autobleem/gamecontrollerdb.txt" ]; then
        ab_ew_db="${ab_ew_db:+$ab_ew_db:}$1/Autobleem/bin/autobleem/gamecontrollerdb.txt"
    fi
    env LD_LIBRARY_PATH="$ab_ew_libs" AB_PAD_DB="$ab_ew_db" \
        "$ab_ew_dir/abpadd" --exit-only --pads ${3:+$3} --watch-pid "$2" > "$AB_EXITWATCH_LOG" 2>&1 &
    AB_EXITWATCH_PID=$!
}

ab_exitwatch_stop() {
    if [ -n "$AB_EXITWATCH_PID" ]; then
        kill "$AB_EXITWATCH_PID" 2>/dev/null
        AB_EXITWATCH_PID=
    fi
}

ab_exitwatch_fired() {
    grep -q "the hotkey has been held" "$AB_EXITWATCH_LOG" 2>/dev/null
}
