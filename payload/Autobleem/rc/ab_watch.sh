#!/bin/sh
#
# CONSOLE-15 evidence: a watch log for the console's "sleep"/Wi-Fi problems. One line every 30 s into RAM
# ($AB_RUNTIME_DIR/watch.log); nothing is written to the stick unless the line shows an anomaly (the Wi-Fi link
# or its IP lost/back, the "PM: suspend" count in dmesg changed, the USB device set changed, the RTC and the
# uptime drifting apart = the console was suspended) - then that line is appended to System/Logs/watch.log (1 MB cap, one rotation: watch.log.1)
# (quiet-stick rule, rc/ab_log.sh). Read-only on /sys and /proc.
#
#   ab_watch.sh start   from boot.sh, before every start of the launcher: copies itself to tmpfs and runs there
#                       in the background (idempotent: a running watch is left alone)
#   ab_watch.sh stop    selection.sh, before it unmounts /media (standby, poweroff, reboot): ends the watch
#
# The running copy lives in /tmp with the cwd there and never reads the stick: /media stays free for umount.
# Line: <date> up=<uptime s> rtc=<rtc since_epoch> drift=<rtc delta - uptime delta> link=<C|N|none> sig=<dBm>
#       oper=<operstate> ip=<y|n> susp=<count> usb=<bus-dev=vid:pid ...> rear=<dev=runtime_status ...> [JUMP] [ANOMALY]
# A JUMP (|drift| > 5 s) between two lines = the clock moved or the console was suspended; on a suspend the
# uptime (CLOCK_MONOTONIC) stands still while the RTC runs on.

: "${AB_ROOT:=/media}"
: "${AB_RUNTIME_DIR:=/tmp/autobleem}"
PIDFILE=$AB_RUNTIME_DIR/ab_watch.pid
RUN=/tmp/ab_watch_run.sh
WLOG=$AB_RUNTIME_DIR/watch.log
INTERVAL=30
KEEP_RAM=1000 # lines kept in RAM (~8 h)

watch_alive() {
    wp=$(cat "$PIDFILE" 2>/dev/null)
    case "$wp" in '' | *[!0-9]*) return 1 ;; esac
    tr '\0' ' ' < /proc/$wp/cmdline 2>/dev/null | grep -q ab_watch_run
}

case "${1:-}" in
start)
    watch_alive && exit 0
    mkdir -p "$AB_RUNTIME_DIR"
    cp -f "$0" $RUN || exit 1
    (cd /tmp && exec sh $RUN run < /dev/null > /dev/null 2>&1) &
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
    echo "usage: ab_watch.sh start|stop" >&2
    exit 2
    ;;
esac

# ---- the running copy (from /tmp) ----
cd /tmp || exit 1
mkdir -p "$AB_RUNTIME_DIR"
echo $$ > "$PIDFILE"
trap 'kill $slp 2>/dev/null; rm -f "$PIDFILE"; exit 0' TERM INT
slp=

# a bounded call: busybox's timeout takes the seconds first (newer) or after -t (older); none = unbounded
TMO=
if timeout 1 true 2> /dev/null; then
    TMO="timeout 5"
elif timeout -t 1 true 2> /dev/null; then
    TMO="timeout -t 5"
fi
tmo() { $TMO "$@"; }

prev_up=
prev_rtc=
prev_sig=
n=0
echo "$(date) ab_watch start pid $$ - $(uname -r)" >> "$WLOG"
while true; do
    up=$(cut -d' ' -f1 /proc/uptime 2> /dev/null)
    up=${up%%.*}
    rtc=$(cat /sys/class/rtc/rtc0/since_epoch 2> /dev/null)
    drift=
    jump=
    if [ -n "$prev_up" ] && [ -n "$rtc" ] && [ -n "$prev_rtc" ] && [ -n "$up" ]; then
        drift=$(( (rtc - prev_rtc) - (up - prev_up) ))
        [ $drift -gt 5 ] || [ $drift -lt -5 ] && jump=" JUMP"
    fi
    prev_up=$up
    prev_rtc=$rtc

    link=none
    sig=
    ip=n
    oper=$(cat /sys/class/net/wlan0/operstate 2> /dev/null)
    if [ -d /sys/class/net/wlan0 ]; then
        l=$(tmo iw dev wlan0 link 2> /dev/null)
        case "$l" in
        Connected*) link=C ;;
        *) link=N ;;
        esac
        sig=$(echo "$l" | sed -n 's/^[[:space:]]*signal:[[:space:]]*\([-0-9]*\).*/\1/p' | head -1)
        tmo ip -4 addr show wlan0 2> /dev/null | grep -q 'inet ' && ip=y
    fi

    susp=$(dmesg 2> /dev/null | grep -c 'PM: suspend')

    usb=
    rear=
    for d in /sys/bus/usb/devices/*; do
        b=${d##*/}
        case "$b" in *:*) continue ;; esac
        [ -e "$d/idVendor" ] || continue
        usb="$usb $b=$(cat $d/idVendor 2> /dev/null):$(cat $d/idProduct 2> /dev/null)"
        case "$(readlink -f "$d" 2> /dev/null)" in
        */musb-hdrc.0.auto/*) rear="$rear $b=$(cat $d/power/runtime_status 2> /dev/null)" ;;
        esac
    done

    # what an anomaly is: not the signal strength, the uptime or the runtime status
    sig_all="link=$link ip=$ip susp=$susp usb=$usb jump=$jump"
    mark=
    if [ -n "$prev_sig" ] && [ "$sig_all" != "$prev_sig" ]; then
        mark=" ANOMALY"
    fi
    line="$(date '+%m-%d %H:%M:%S') up=$up rtc=$rtc drift=$drift link=$link sig=$sig oper=$oper ip=$ip susp=$susp usb=$usb rear=$rear$jump$mark"
    echo "$line" >> "$WLOG"
    prev_sig=$sig_all

    if [ -n "$mark" ] && grep -qs " $AB_ROOT " /proc/mounts; then
        mkdir -p "$AB_ROOT/System/Logs" 2> /dev/null
        # only the anomaly line is appended; 1 MB cap, one rotation (watch.log.1)
        SW=$AB_ROOT/System/Logs/watch.log
        sz=$(wc -c < "$SW" 2> /dev/null)
        [ "${sz:-0}" -gt 1048576 ] && mv -f "$SW" "$SW.1" 2> /dev/null
        echo "$line" >> "$SW" 2> /dev/null
    fi

    n=$((n + 1))
    if [ $n -ge 100 ]; then
        n=0
        tail -n $KEEP_RAM "$WLOG" > /tmp/ab_watch.tmp 2> /dev/null && cat /tmp/ab_watch.tmp > "$WLOG"
        rm -f /tmp/ab_watch.tmp
    fi

    sleep $INTERVAL &
    slp=$!
    wait $slp
done
