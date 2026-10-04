#!/bin/sh
#
# The start of a PE App (an App made from a PE mod package by the mods scanner processor): its generated run.sh
# calls
#
#     Autobleem/rc/pe_run.sh <app folder>
#
# This sets up what every App gets (rc/app_env.sh: libraries, a home on the stick, the virtual pad), refuses a
# launcher the compat list blocks (rc/pe_compat.ini), builds the RAM environment the mod's own launch.sh expects
# (rc/pe_env.sh) and runs that launch.sh, unchanged, as a child - then takes everything down again and exits with
# the child's status. A TERM (the Reset button: rc/abpad asks, then SIGTERM, then SIGKILL after 3 s) stops the mod
# and its children, with a second of grace, and still cleans up.

if [ -z "$1" ] || ! AB_APP_DIR=$(cd "$1" 2>/dev/null && pwd); then
    echo "usage: $0 <app folder>" >&2
    exit 1
fi
export AB_APP_DIR
RC_DIR=$(cd "$(dirname "$0")" && pwd)
: "${AB_ROOT:=$(cd "$AB_APP_DIR/../.." && pwd)}"
: "${AB_RUNTIME_DIR:=/tmp/autobleem}"
: "${AB_LOG_DIR:=$AB_RUNTIME_DIR/logs}"
export AB_ROOT AB_RUNTIME_DIR AB_LOG_DIR
PE_RC_DIR=${PE_RC_DIR:-$RC_DIR}
mkdir -p "$AB_LOG_DIR/pe" 2>/dev/null

# ---------------------------------------------------------------------------------------------
# The compat list: a launcher that block=1 names is refused - on screen (the launcher says it when it is back: it
# reads <runtime>/app-message.txt, the App's title and the reason) and in the log.
# ---------------------------------------------------------------------------------------------
. "$RC_DIR/pe_env.sh" || exit 1

PE_LAUNCHER=$(pe_cfg_get "$AB_APP_DIR/launcher.cfg" launcher_filename)
[ -n "$PE_LAUNCHER" ] || PE_LAUNCHER=$(basename "$AB_APP_DIR" | sed 's/^pe-//')
if [ "$(pe_ini_get "$PE_RC_DIR/pe_compat.ini" "$PE_LAUNCHER" block)" = 1 ]; then
    PE_REASON=$(pe_ini_get "$PE_RC_DIR/pe_compat.ini" "$PE_LAUNCHER" reason)
    PE_TITLE=$(pe_cfg_get "$AB_APP_DIR/launcher.cfg" launcher_title)
    [ -n "$PE_TITLE" ] || PE_TITLE=$PE_LAUNCHER
    pe_log "refused $PE_LAUNCHER: blocked by the compat list ($PE_REASON)"
    echo "pe_run.sh: $PE_TITLE is blocked: $PE_REASON" >&2
    mkdir -p "$AB_RUNTIME_DIR" 2>/dev/null
    printf '%s\n%s\n' "$PE_TITLE" "$PE_REASON" > "$AB_RUNTIME_DIR/app-message.txt"
    exit 1
fi

# ---------------------------------------------------------------------------------------------
# What every App gets, then the PE tree around the mod.
# ---------------------------------------------------------------------------------------------
# The pad output (docs: PadMode): the user's choice (AB_APP_PAD_MODE from the launcher), else the App's own PadMode=
# (proc_pe writes it from pad= in pe_compat.ini), else this launcher's pad= in pe_compat.ini (an App converted
# before PadMode existed), else the console's own pad. app_env.sh then turns it into the shim's one answer.
if [ -z "$AB_APP_PAD_MODE" ]; then
    AB_APP_PAD_MODE=$(sed -n 's/^[Pp][Aa][Dd][Mm][Oo][Dd][Ee][[:space:]]*=[[:space:]]*//p' "$AB_APP_DIR/app.ini" 2>/dev/null | tail -n 1 | tr -d '\r' | tr 'A-Z' 'a-z')
fi
[ -n "$AB_APP_PAD_MODE" ] || AB_APP_PAD_MODE=$(pe_ini_get "$PE_RC_DIR/pe_compat.ini" "$PE_LAUNCHER" pad)
[ -n "$AB_APP_PAD_MODE" ] || AB_APP_PAD_MODE=psc
export AB_APP_PAD_MODE
# the d-pad / stick flags the same way: the user's choice, else the App's Dpad2Analog=/Analog2Dpad= (app_env.sh reads
# those), else this launcher's dpad2analog=/analog2dpad= in pe_compat.ini
pe_app_ini_flag() {
    awk -v want="$1" '{ sub(/\r$/, "") } index($0, "=") > 0 {
            k = substr($0, 1, index($0, "=") - 1); v = substr($0, index($0, "=") + 1)
            gsub(/^[ \t]+|[ \t]+$/, "", k); gsub(/^[ \t]+|[ \t]+$/, "", v)
            if (tolower(k) == want) found = v
        } END { printf "%s", found }' "$AB_APP_DIR/app.ini" 2>/dev/null
}
if [ -z "$AB_APP_DPAD2ANALOG" ] && [ -z "$(pe_app_ini_flag dpad2analog)" ]; then
    AB_APP_DPAD2ANALOG=$(pe_ini_get "$PE_RC_DIR/pe_compat.ini" "$PE_LAUNCHER" dpad2analog)
fi
if [ -z "$AB_APP_ANALOG2DPAD" ] && [ -z "$(pe_app_ini_flag analog2dpad)" ]; then
    AB_APP_ANALOG2DPAD=$(pe_ini_get "$PE_RC_DIR/pe_compat.ini" "$PE_LAUNCHER" analog2dpad)
fi
export AB_APP_DPAD2ANALOG AB_APP_ANALOG2DPAD

. "$RC_DIR/app_env.sh"

PE_SHELL=sh
command -v bash > /dev/null 2>&1 && PE_SHELL=bash # the mods are #!/bin/sh scripts that use source, [[ ]], arrays

PE_CLEANED=
PE_CHILD=
PE_TERMED=
if ! pe_prepare; then
    pe_log "could not build the environment for $PE_LAUNCHER"
    pe_cleanup
    exit 1
fi

# A TERM/INT/HUP (Reset, Start+Select) stops the mod and everything it started: TERM, a second, KILL
pe_on_term() {
    PE_TERMED=1
    [ -n "$PE_CHILD" ] || return 0
    pe_log "told to stop - stopping the mod"
    pe_signal TERM "$PE_CHILD"
    pe_n=0
    while [ "$pe_n" -lt 5 ] && kill -0 "$PE_CHILD" 2>/dev/null; do
        sleep 0.2
        pe_n=$((pe_n + 1))
    done
    kill -0 "$PE_CHILD" 2>/dev/null && pe_signal KILL "$PE_CHILD"
}
trap pe_on_term TERM INT HUP

pe_log "starting $PE_LAUNCHER with $PE_SHELL (PROJECT_ERIS_PATH=$PE_BOUND_PATH)"
pe_trail
cd "$PE_LAUNCHTMP" || cd "$AB_APP_DIR" || exit 1
if [ "$AB_PAD_HIDE" = 1 ]; then
    # the kernel pad: the mod starts where the held pads' nodes are /dev/null, so the virtual pad is the only one it
    # finds (abpadd --hide-run execs the shell: the pid stays the mod's)
    "$AB_PAD_DIR/abpadd" --hide-run "$AB_PAD_HIDE_LIST" -- "$PE_SHELL" ./launch.sh &
else
    "$PE_SHELL" ./launch.sh &
fi
PE_CHILD=$!
wait "$PE_CHILD"
PE_RC=$?
# a trap interrupts wait: go on until the child is really gone
while kill -0 "$PE_CHILD" 2>/dev/null; do
    wait "$PE_CHILD"
    PE_RC=$?
done
[ -z "$PE_TERMED" ] || PE_RC=143
pe_log "the mod ended with $PE_RC"
pe_trail_end

trap - TERM INT HUP
cd /
pe_cleanup
exit "$PE_RC"
