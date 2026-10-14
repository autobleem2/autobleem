#!/bin/sh
#
# AUTOBLEEM-31: the console's memory policy, sourced by boot.sh once per boot (and safe to run again: the boot loop's
# next round, a restart). 1 GB of RAM and no swap, with vm.overcommit_memory=0 (heuristic): an emulator that
# reserves a big address range up front (the N64 and Dreamcast cores) is refused. As Project Eris does:
#
#   - vm.overcommit_memory=1 (always allow the reservation)
#   - one zram swap device per CPU (zram0..N-1, hot_add for those the kernel does not have yet), together half of
#     MemTotal, priority 100 - compressed swap in RAM
#
# RAM, /proc and /sys only: nothing on the stick, the eMMC or /data is written. A kernel without zram
# (no /sys/class/zram-control and no /sys/block/zram0) keeps the overcommit setting and gets no swap, quietly.
# Needs mkswap and swapon: the busybox applets (the firmware's /sbin/swapon is busybox's - not verified on a
# console, see docs/psc-memory.md), or the full ones; neither found = no swap, one line in the log.
#
# The places are variables so a test can move them: AB_MEM_PROC (/proc), AB_MEM_SYS (/sys), AB_MEM_DEV (/dev).

: "${AB_MEM_PROC:=/proc}"
: "${AB_MEM_SYS:=/sys}"
: "${AB_MEM_DEV:=/dev}"

# ab_mem_tool NAME: runs the tool NAME - itself, or the busybox applet of that name; fails when there is neither
ab_mem_tool() {
    ab_mt=$1
    shift
    if command -v "$ab_mt" > /dev/null 2>&1; then
        "$ab_mt" "$@"
    elif command -v busybox > /dev/null 2>&1 && busybox --list 2> /dev/null | grep -qx "$ab_mt"; then
        busybox "$ab_mt" "$@"
    else
        return 127
    fi
}

ab_memory_setup() {
    ab_oc=$AB_MEM_PROC/sys/vm/overcommit_memory
    ab_msg="overcommit_memory=$(cat "$ab_oc" 2> /dev/null)"
    if [ -w "$ab_oc" ] && [ "$(cat "$ab_oc" 2> /dev/null)" != 1 ]; then
        echo 1 > "$ab_oc" 2> /dev/null
        ab_msg="overcommit_memory=$(cat "$ab_oc" 2> /dev/null) (was 0)"
    fi

    ab_cpus=$(grep -c '^processor' "$AB_MEM_PROC/cpuinfo" 2> /dev/null)
    case "$ab_cpus" in '' | 0 | *[!0-9]*) ab_cpus=1 ;; esac
    ab_kb=$(sed -n 's/^MemTotal: *\([0-9]*\) kB.*/\1/p' "$AB_MEM_PROC/meminfo" 2> /dev/null)
    case "$ab_kb" in '' | *[!0-9]*) ab_kb=0 ;; esac
    ab_size=$((ab_kb * 1024 / 2 / ab_cpus))

    if [ ! -d "$AB_MEM_SYS/class/zram-control" ] && [ ! -d "$AB_MEM_SYS/block/zram0" ]; then
        ab_msg="$ab_msg, no zram in this kernel - no swap"
    elif [ "$ab_size" -le 0 ]; then
        ab_msg="$ab_msg, MemTotal unknown - no swap"
    else
        ab_on=0
        ab_i=0
        while [ "$ab_i" -lt "$ab_cpus" ]; do
            ab_z=zram$ab_i
            # already a swap (an earlier round): nothing to do, never a second swapon
            if grep -qs "^$AB_MEM_DEV/$ab_z " "$AB_MEM_PROC/swaps"; then
                ab_on=$((ab_on + 1))
                ab_i=$((ab_i + 1))
                continue
            fi
            if [ ! -d "$AB_MEM_SYS/block/$ab_z" ] && [ -r "$AB_MEM_SYS/class/zram-control/hot_add" ]; then
                cat "$AB_MEM_SYS/class/zram-control/hot_add" > /dev/null 2>&1
            fi
            if [ -d "$AB_MEM_SYS/block/$ab_z" ]; then
                # a device that has a size already (set up, not swapped) keeps it: disksize only takes a write on a reset one
                [ "$(cat "$AB_MEM_SYS/block/$ab_z/disksize" 2> /dev/null)" = 0 ] &&
                    echo "$ab_size" > "$AB_MEM_SYS/block/$ab_z/disksize" 2> /dev/null
                if ab_mem_tool mkswap -L "$ab_z" "$AB_MEM_DEV/$ab_z" > /dev/null 2>&1 &&
                    ab_mem_tool swapon -p 100 "$AB_MEM_DEV/$ab_z" > /dev/null 2>&1; then
                    ab_on=$((ab_on + 1))
                fi
            fi
            ab_i=$((ab_i + 1))
        done
        ab_msg="$ab_msg, zram swap on $ab_on of $ab_cpus devices, $((ab_size / 1048576)) MiB each (MemTotal ${ab_kb} kB)"
    fi
    echo "$(date) boot: $ab_msg" >> "${AB_LOG_DIR:-/tmp}/boot.log" 2> /dev/null
    return 0
}
