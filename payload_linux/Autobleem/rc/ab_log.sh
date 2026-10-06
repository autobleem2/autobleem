#!/bin/sh
#
# Sourced by the rc scripts (and, on a Pi or the PC stick, by the session): where this run's logs go
# (docs/quiet-stick-plan.md). The stick is written only when the user's state changes - a log is kept in
# RAM and reaches the stick only when something went wrong, or when a tester asked for it.
#
#   AB_ROOT          the data root; /media (the console) unless set before sourcing
#   AB_RUNTIME_DIR   RAM for the run's logs and hand-over files - tmpfs: /tmp/autobleem on the console,
#                    /run/autobleem under systemd on the Linux targets. Kept when already set.
#   AB_LOG_DIR       <runtime>/logs, or System/Logs on the stick when the logs are kept - the System/Logs/keep
#                    marker (a tester makes it from a PC; the launcher's Options row "Keep logs on the stick"
#                    makes it too) or AB_KEEP_LOGS=1. The launcher's own choice wins: it writes it to
#                    <runtime>/log_dir, for the scripts that run after it, and exports it to what it starts.
#
#   ab_persist_logs REASON
#                    copies the logs to System/Logs/crash-<n>/ with REASON and the kernel's last lines - after
#                    a crash, and before the reboot that would empty tmpfs. The last three are kept; the
#                    launcher says so once at its next start (the .new marker).
#
# Keep this file identical in payload/ and payload_linux/ (and autobleem-appliance's payload_linux/).

: "${AB_ROOT:=/media}"
: "${AB_RUNTIME_DIR:=/tmp/autobleem}"
if [ -s "$AB_RUNTIME_DIR/log_dir" ]; then
    AB_LOG_DIR=$(cat "$AB_RUNTIME_DIR/log_dir")
elif [ -z "${AB_LOG_DIR:-}" ]; then
    if [ -f "$AB_ROOT/System/Logs/keep" ] || [ "${AB_KEEP_LOGS:-}" = 1 ]; then
        AB_LOG_DIR=$AB_ROOT/System/Logs
    else
        AB_LOG_DIR=$AB_RUNTIME_DIR/logs
    fi
fi
mkdir -p "$AB_LOG_DIR" 2>/dev/null
export AB_ROOT AB_RUNTIME_DIR AB_LOG_DIR

# ab_timeout CMD...: CMD bounded to 5 s where the system has a timeout (busybox's takes the seconds first, or
# after -t in an older one), plain otherwise - the status calls below must not hang a crash capture or a wake
if timeout 1 true 2>/dev/null; then
    ab_timeout() { timeout 5 "$@"; }
elif timeout -t 1 true 2>/dev/null; then
    ab_timeout() { timeout -t 5 "$@"; }
else
    ab_timeout() { "$@"; }
fi

# ab_config_get KEY: the value of KEY in the launcher's config.ini (<root>/Autobleem/bin/autobleem/config.ini - the
# file the launcher writes, with "Outputmode=" style keys; an older or hand-made file may spell the key in any
# case, with CRLF line ends). The last such line wins; empty when there is none. Not /media/System/config.ini:
# nothing writes that on a stick (the Windows install's data folder has one, never a console).
ab_config_get() {
    tr -d '\r' < "$AB_ROOT/Autobleem/bin/autobleem/config.ini" 2>/dev/null | grep -i "^$1=" | tail -1 | cut -d= -f2-
}

ab_persist_logs() {
    ab_keep=$AB_ROOT/System/Logs
    mkdir -p "$ab_keep" 2>/dev/null || return 1
    ab_n=$(cat "$ab_keep/crash.count" 2>/dev/null)
    case "$ab_n" in '' | *[!0-9]*) ab_n=0 ;; esac
    ab_n=$((ab_n + 1))
    echo "$ab_n" > "$ab_keep/crash.count"
    ab_dir=$ab_keep/crash-$ab_n
    mkdir -p "$ab_dir"
    [ "$AB_LOG_DIR" != "$ab_keep" ] && cp -f "$AB_LOG_DIR"/* "$ab_dir"/ 2>/dev/null
    ab_rc=$(cat "$AB_RUNTIME_DIR/autobleem_exit" 2>/dev/null) # run.sh's status (rc/autobleem.sh): 139 SEGV, 137 KILL, 134 ABRT
    {
        date
        echo "${1:-}"
        echo "autobleem.sh exit status: ${ab_rc:-unknown}"
        echo
        dmesg 2>/dev/null | tail -150
    } > "$ab_dir/reason.txt"
    # CONSOLE-15 evidence: the whole picture of the machine at the crash (a few tens of KB, three crashes kept)
    dmesg > "$ab_dir/dmesg.txt" 2>/dev/null
    dmesg 2>/dev/null | grep -E 'PM: |Freezing|Restarting tasks|musb|failed to suspend|while active|-71|-110' > "$ab_dir/suspend.txt"
    {
        date
        cat /proc/uptime
        cat /sys/class/rtc/rtc0/since_epoch
        cat /proc/loadavg
        echo "autobleem.sh exit status: ${ab_rc:-unknown}"
    } > "$ab_dir/uptime.txt" 2>/dev/null
    cat /proc/meminfo > "$ab_dir/meminfo.txt" 2>/dev/null
    {
        echo "== iw dev wlan0 link"
        ab_timeout iw dev wlan0 link
        echo "== ip addr show wlan0"
        ab_timeout ip addr show wlan0
        echo "== ip route"
        ab_timeout ip route
        echo "== wpa_cli -i wlan0 status"
        ab_timeout wpa_cli -i wlan0 status
    } > "$ab_dir/net.txt" 2>&1
    [ -f "$AB_RUNTIME_DIR/watch.log" ] && cp -f "$AB_RUNTIME_DIR/watch.log" "$ab_dir/watch.log" 2>/dev/null
    # the extensions' crash guard is in RAM too: where the next launcher looks for it after the reboot
    if [ -f "$AB_RUNTIME_DIR/extensions.active" ]; then
        mkdir -p "$AB_ROOT/System/Extensions"
        cp -f "$AB_RUNTIME_DIR/extensions.active" "$AB_ROOT/System/Extensions/.active"
    fi
    touch "$ab_dir/.new"
    for ab_old in "$ab_keep"/crash-*; do
        ab_k=${ab_old##*-}
        case "$ab_k" in '' | *[!0-9]*) continue ;; esac
        [ "$ab_k" -le $((ab_n - 3)) ] && rm -rf "$ab_old"
    done
    sync
}
