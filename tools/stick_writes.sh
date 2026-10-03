#!/bin/sh
# What a scenario writes to the stick / data partition - the measuring tape of autobleem-main's docs/archive/quiet-stick-plan.md.
# Runs on the target (the console over its RNDIS/ssh, a Pi or the PC stick over ssh), busybox sh is enough.
#
#   stick_writes.sh start [MOUNT]   remember the partition's written-sectors counter, and watch the files
#                                   with inotifywait when the machine has it (the Pi: inotify-tools)
#   ... do the scenario: boot, idle, a rescan, a game and back, a standby ...
#   stick_writes.sh stop            KiB written since start, and the files touched (when watched) - a file
#                                   kept open and appended to (a log, a database) shows as MODIFY
#
# MOUNT defaults to /media (the console), else /media/autobleem (a Pi / the PC stick). The counter is the
# partition's own (/sys/class/block/<part>/stat, field 7: 512-byte sectors written) - it counts everything,
# filesystem metadata and the FAT included, which is what the flash sees. State lives in /tmp.
set -u

STATE=/tmp/stick_writes.state
EVENTS=/tmp/stick_writes.events

pick_mount() {
    if [ -n "${1:-}" ]; then echo "$1"; return; fi
    for m in /media /media/autobleem; do
        grep -q " $m " /proc/mounts && { echo "$m"; return; }
    done
    echo "no data partition mounted at /media or /media/autobleem - pass the mount point" >&2
    exit 1
}

sectors() {
    dev=$(awk -v m="$1" '$2 == m { print $1 }' /proc/mounts | tail -1)
    part=${dev##*/}
    [ -r "/sys/class/block/$part/stat" ] || { echo "no /sys/class/block/$part/stat for $dev" >&2; exit 1; }
    awk '{ print $7 }' "/sys/class/block/$part/stat"
}

case "${1:-}" in
start)
    MOUNT=$(pick_mount "${2:-}")
    sync
    echo "MOUNT=$MOUNT" > "$STATE"
    echo "START=$(sectors "$MOUNT")" >> "$STATE"
    rm -f "$EVENTS"
    if command -v inotifywait > /dev/null 2>&1; then
        inotifywait -m -r -q -e modify,close_write,moved_to,create,delete --format '%e %w%f' "$MOUNT" > "$EVENTS" 2>/dev/null &
        echo "WATCH=$!" >> "$STATE"
    fi
    echo "measuring writes to $MOUNT - run the scenario, then: $0 stop"
    ;;
stop)
    [ -f "$STATE" ] || { echo "not started" >&2; exit 1; }
    . "$STATE"
    sync
    END=$(sectors "$MOUNT")
    [ -n "${WATCH:-}" ] && kill "$WATCH" 2>/dev/null
    echo "written to $MOUNT: $(( (END - START) / 2 )) KiB"
    if [ -f "$EVENTS" ]; then
        echo "files ($(wc -l < "$EVENTS") events):"
        # one line per file, how many times and which events
        awk '{ ev = $1; $1 = ""; sub(/^ /, ""); n[$0]++; e[$0] = e[$0] (index(e[$0], ev) ? "" : " " ev) }
             END { for (f in n) printf "%5d %s  [%s ]\n", n[f], f, e[f] }' "$EVENTS" | sort -rn
    fi
    rm -f "$STATE"
    ;;
*)
    sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'
    exit 1
    ;;
esac
