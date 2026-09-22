#!/bin/sh
#
# Sourced by an App's run.sh before it starts its program - the Pi's version of the console's
# rc/app_env.sh. The console's job there is the shared libraries RetroBoot's apps need; a Pi has a
# real distribution underneath it and needs none of that, so what is left here is the virtual gamepad
# (docs/virtual-gamepad-plan.md) and the environment an App is entitled to expect.
#
# An App's run.sh does:
#
#     #!/bin/sh
#     . "$(dirname "$0")/../../Autobleem/rc/app_env.sh"
#     cd "$AB_APP_DIR" || exit 1
#     exec ./the-game
#
# It is sourced, not run, so "$0" here is the *App's* run.sh - which is what lets the data partition
# be found without anything being hardcoded, the same way the other rc scripts find it. An App is
# assumed to live at <root>/Apps/<name>/, which is what the launcher's Apps set means by one.

AB_APP_DIR=$(cd "$(dirname "$0")" && pwd)
AB_ROOT=$(cd "$AB_APP_DIR/../.." && pwd)
AB_LOG_DIR="$AB_ROOT/System/Logs"
export AB_APP_DIR AB_ROOT
mkdir -p "$AB_LOG_DIR" 2>/dev/null

# ---------------------------------------------------------------------------------------------
# The virtual gamepad.
#
# abpadd reads the pads through SDL's GameController API with our own database - the same code and
# the same file the launcher uses, so a pad resolves in here exactly as it does out there - and
# libabpad.so, preloaded, shows the App a pad it understands whichever SDL API it reads. Neither is
# required: an App whose folder has no pad.ini and a tree with no abpad installed simply run as they
# always did.
#
# The daemon is given this shell's pid to watch, so it goes when the App goes - including when the
# App is exec'd over this shell, which keeps the same pid, and including when the App crashes.
# ---------------------------------------------------------------------------------------------
AB_PAD_DIR="$AB_ROOT/Autobleem/bin/abpad"
if [ -x "$AB_PAD_DIR/abpadd" ] && [ -f "$AB_PAD_DIR/libabpad.so" ]; then
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
    [ -f "$AB_PAD_DIR/pad.default.ini" ] && export AB_PAD_DEFAULTS="$AB_PAD_DIR/pad.default.ini"
    [ -f "$AB_APP_DIR/pad.ini" ] && export AB_PAD_PROFILE="$AB_APP_DIR/pad.ini"

    # For an App the preload cannot reach - one statically linked against SDL - the mapping the
    # daemon actually resolved. Deliberately not our gamecontrollerdb.txt: a file given this way
    # overrides SDL's built-in table, and for a pad SDL already knows the built-in entry is the right
    # one while ours may be a stale line for another of that pad's modes.
    [ -f /tmp/abpad.state.mappings ] && export SDL_GAMECONTROLLERCONFIG_FILE=/tmp/abpad.state.mappings
fi
