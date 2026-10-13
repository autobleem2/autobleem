#!/bin/sh
#
# AUTOBLEEM-28: the stick pulled while the launcher - or a game, RetroArch, an App - runs. Every half second the
# watch looks for the stick's partition; when it is gone while /media is still mounted, everything below boot.sh's
# loop is stopped at once (SIGSTOP: nothing writes another byte, nothing reacts to the others dying), then killed,
# and the dead mount is let go (umount -l). The launcher's hand-over file gets selection 9 (no stick): boot.sh's loop
# runs selection.sh, which shows the insert-stick picture, waits for the stick in any port (5 minutes, then the
# standby; POWER = the standby at once), checks it (it was pulled while mounted - rc/ab_fsck.sh), mounts it and
# lets the loop start the launcher again. A resume in place is not possible: every open file of the old mount is dead.
#
#   ab_stickwatch.sh start PID   boot.sh, before every start of the launcher (PID = the loop's shell): copies itself
#                                to tmpfs and runs there in the background (a running watch is left alone)
#   ab_stickwatch.sh stop        selection.sh's standby, before it unmounts the stick itself
#
# The running copy lives in /tmp with the cwd there and never reads the stick. When /media is unmounted by anyone
# else (the standby), or holds another device, the watch just ends. Nothing on the console's own storage is written.

: "${AB_RUNTIME_DIR:=/tmp/autobleem}"
PIDFILE=$AB_RUNTIME_DIR/ab_stickwatch.pid
RUN=/tmp/ab_stickwatch_run.sh
SEL_NOSTICK=9

watch_alive() {
    wp=$(cat "$PIDFILE" 2>/dev/null)
    case "$wp" in '' | *[!0-9]*) return 1 ;; esac
    tr '\0' ' ' < /proc/$wp/cmdline 2>/dev/null | grep -q ab_stickwatch_run
}

case "${1:-}" in
start)
    watch_alive && exit 0
    mkdir -p "$AB_RUNTIME_DIR"
    cp -f "$0" $RUN || exit 1
    (cd /tmp && exec sh $RUN run "${2:-}" < /dev/null > /dev/null 2>&1) &
    exit 0
    ;;
stop)
    watch_alive || { rm -f "$PIDFILE"; exit 0; }
    kill "$wp" 2>/dev/null
    n=0
    while [ -d /proc/$wp ] && [ $n -lt 30 ]; do
        n=$((n + 1))
        sleep 0.1 2> /dev/null || sleep 1
    done
    rm -f "$PIDFILE"
    exit 0
    ;;
run) ;;
*)
    echo "usage: ab_stickwatch.sh start LOOP_PID | stop" >&2
    exit 2
    ;;
esac

# ---- the running copy (from /tmp) ----
cd /tmp || exit 1
LOOP=$2
WLOG=$AB_RUNTIME_DIR/stickwatch.log
DEV=$(awk '$2 == "/media" { print $1 }' /proc/mounts | head -1)
case "$DEV" in /dev/*) ;; *) exit 0 ;; esac
NAME=${DEV##*/}
echo $$ > "$PIDFILE"
trap 'rm -f "$PIDFILE"; exit 0' TERM INT
nap() { usleep 500000 2> /dev/null || sleep 0.5 2> /dev/null || sleep 1; }

while true; do
    # /media let go or holding another device: the standby (or someone) unmounted it - nothing to watch
    grep -q "^$DEV /media " /proc/mounts || { rm -f "$PIDFILE"; exit 0; }
    [ -e /sys/class/block/$NAME ] || break
    nap
done

# ---- the stick is gone with /media still mounted ----
touch /tmp/.abnostick
log() {
    echo "$(date) up=$(cut -d' ' -f1 /proc/uptime) $*" >> "$WLOG"
    echo "ab_stickwatch: $*" > /dev/kmsg 2> /dev/null
}
log "$DEV pulled while mounted - stopping everything below the loop (pid $LOOP)"

# every process below the loop's shell (but this watch), and anything else running from the stick (a daemon that
# left its parent): stopped at once, then killed
ppids=$(for s in /proc/[0-9]*/stat; do
    p=${s#/proc/}
    echo "${p%/stat} $(sed 's/.*) [A-Za-z] \([0-9]*\).*/\1/' "$s" 2> /dev/null)"
done)
victims=
frontier=$LOOP
while [ -n "$frontier" ]; do
    next=
    for f in $frontier; do
        for k in $(echo "$ppids" | awk -v f="$f" '$2 == f { print $1 }'); do
            [ "$k" = $$ ] && continue
            next="$next $k"
        done
    done
    victims="$victims$next"
    frontier=$next
done
for d in /proc/[0-9]*; do
    p=${d#/proc/}
    [ "$p" = 1 ] || [ "$p" = $$ ] || [ "$p" = "$LOOP" ] && continue
    case " $victims " in *" $p "*) continue ;; esac
    case "$(readlink $d/exe 2> /dev/null)" in /media/*) victims="$victims $p" ;; esac
done
[ -n "$victims" ] && kill -STOP $victims 2> /dev/null
log "stopped:$(for p in $victims; do printf ' %s(%s)' $p "$(cat /proc/$p/comm 2> /dev/null)"; done)"

# the loop's next selection: no stick (the stopped launcher can no longer write its own over it)
mkdir -p "$AB_RUNTIME_DIR"
echo "AB_SELECTION=$SEL_NOSTICK" > "$AB_RUNTIME_DIR/autobleem_cfg.sh"

[ -n "$victims" ] && kill -KILL $victims 2> /dev/null
rm -f /tmp/.abload /tmp/.abupdating # an absplash waiting on them is gone with the rest
sync
umount -l /media 2> /dev/null
log "killed, /media let go (umount -l): $(grep -c ' /media ' /proc/mounts) mount(s) left"
rm -f "$PIDFILE"
exit 0
