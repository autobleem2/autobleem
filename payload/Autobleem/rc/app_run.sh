#!/bin/sh
#
# The generic start of a multi-platform App (docs/app-format-plan.md): what the launcher runs for an App whose
# app.ini names its binaries (Exec=bin/{key}/...) and has no run.sh of its own. The launcher has resolved the
# ini already and passes the answer in the environment (AB_APP_DIR, AB_APP_EXEC, AB_APP_ARGS, ...); by hand,
# give the App's folder:
#
#     Autobleem/rc/app_run.sh /media/Apps/opentyrian
#
# and app_env.sh resolves it the same way. Either way this sets up what every App gets (app_env.sh: the
# libraries, a home on the stick, the virtual gamepad) and then becomes the App's program, in its folder.

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
# Args= follows the shell's quoting ("two words" is one argument)
eval "exec \"\$AB_APP_EXEC\" $AB_APP_ARGS"
