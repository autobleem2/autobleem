#!/bin/sh
#
# The generic start of a multi-platform App (autobleem-main docs/archive/app-format-plan.md): what the launcher runs for an App whose
# app.ini names its binaries (Exec=bin/{key}/...) and has no run.sh of its own. The launcher has resolved the
# ini already and passes the answer in the environment (AB_APP_DIR, AB_APP_EXEC, AB_APP_ARGS, ...); by hand,
# give the App's folder:
#
#     Autobleem/rc/app_run.sh /media/Apps/opentyrian
#
# and app_env.sh resolves it the same way. Either way this sets up what every App gets (app_env.sh: the
# libraries, a home on the stick, the virtual gamepad), then runs the App's program, in its folder, and waits.

if [ -n "$1" ]; then
    AB_APP_DIR=$(cd "$1" && pwd) || exit 1
    export AB_APP_DIR
    unset AB_APP_EXEC
fi
if [ -z "$AB_APP_DIR" ]; then
    echo "usage: $0 <app folder>" >&2
    exit 1
fi

. "$(dirname "$0")/app_env.sh"

if [ -z "$AB_APP_EXEC" ]; then
    echo "app_run.sh: $AB_APP_DIR has nothing this machine can run" >&2
    exit 1
fi
cd "$AB_APP_DIR" || exit 1
# Not exec'd: this shell stays the App's parent, so Reset / Start+Select (a TERM to it) and the App's own end stop
# everything the App started, and the launcher comes back only when all of it is gone (app_env.sh, "The App's
# processes"). Args= follows the shell's quoting ("two words" is one argument).
trap ab_app_on_term TERM INT HUP
if [ "$AB_PAD_HIDE" = 1 ]; then
    # the App starts where the held pads (the kernel pad) and the Reset button cannot be seen (rc/app_env.sh)
    eval "ab_app_start \"\$AB_PAD_DIR/abpadd\" --hide-run \"\$AB_PAD_HIDE_LIST\" -- \"\$AB_APP_EXEC\" $AB_APP_ARGS"
else
    eval "ab_app_start \"\$AB_APP_EXEC\" $AB_APP_ARGS"
fi
ab_app_wait
exit $?
