#!/bin/sh
# Diagnostic for the "dirty stick" question (2026-09-22), round 2: the Sony scripts that run the payload
# and do the standby, the power_manage interface, and a remount test with nothing of ours writing to the
# stick. Run from boot.sh (./psc_mount_debug.sh boot); the log is built on tmpfs and copied to
# /media/System/Logs/mount_debug_<tag>.txt at the end.

TAG=${1:-boot}
OUT=/tmp/mount_debug_$TAG.txt

pr() { tr -c '[:print:]\n' '\n' < "$1" | grep -a "$2" | sort -u; }

{
    echo "=== $(date) tag=$TAG pid=$$ ppid=$PPID"
    echo; echo "=== holders of /media before the test"
    for d in /proc/[0-9]*; do
        pid=${d#/proc/}
        for l in cwd exe root; do
            case "$(readlink $d/$l 2>/dev/null)" in /media*) echo "$pid $l $(cat $d/comm) -> $(readlink $d/$l)";; esac
        done
        ls -l $d/fd 2>/dev/null | grep /media | sed "s|^|$pid fd $(cat $d/comm 2>/dev/null) |"
    done

    echo; echo "=== remount test (log on tmpfs; the bind mount first)"
    sync
    umount /etc/udev/rules.d/20-joystick.rules 2>&1 && echo "bind umount: OK" || echo "bind umount: FAILED"
    if mount -o remount,ro /media 2>&1; then
        echo "remount,ro: OK"; grep " /media " /proc/mounts
        echo "boot sector byte 0x41 while ro: $(dd if=/dev/sda1 bs=1 skip=65 count=1 2>/dev/null | od -A n -t x1)"
        mount -o remount,rw /media 2>&1 && echo "remount,rw: OK" || echo "remount,rw: FAILED"
        grep " /media " /proc/mounts
        echo "boot sector byte 0x41 after rw: $(dd if=/dev/sda1 bs=1 skip=65 count=1 2>/dev/null | od -A n -t x1)"
    else
        echo "remount,ro: FAILED"
    fi
    echo "umount test (expected to fail on cwd only):"; umount /media 2>&1 && { echo "umount: OK (!)"; mount /dev/sda1 /media 2>&1 && echo "mounted again"; } || echo "umount: FAILED"
    mount -o bind /media/Autobleem/rc/20-joystick.rules /etc/udev/rules.d/20-joystick.rules 2>&1 && echo "bind mount restored"

    echo; echo "=== FAT[1] entry (ClnShutBit = bit 27)"
    # reserved sectors at 0x0e (2 bytes), bytes/sector at 0x0b
    rs=$(dd if=/dev/sda1 bs=1 skip=14 count=2 2>/dev/null | od -A n -t u2 | tr -d ' ')
    bps=$(dd if=/dev/sda1 bs=1 skip=11 count=2 2>/dev/null | od -A n -t u2 | tr -d ' ')
    echo "reserved sectors=$rs bytes/sector=$bps"
    dd if=/dev/sda1 bs=$bps skip=$rs count=1 2>/dev/null | od -A n -t x1 -N 8

    echo; echo "=== /usr/bin/usb_watch"; cat /usr/bin/usb_watch
    echo; echo "=== /usr/bin/start_pman"; cat /usr/bin/start_pman
    echo; echo "=== /usr/sony/bin/sonyapp"; cat /usr/sony/bin/sonyapp
    echo; echo "=== /tmp/diag"; ls -laR /tmp/diag 2>&1 | head -60
    for f in /tmp/diag/028c*/start /tmp/diag/028c*/*.sh; do [ -f "$f" ] && { echo "--- $f"; cat "$f"; }; done
    echo; echo "=== update log"; cat /tmp/diag/.tmp/.update_*.log 2>&1 | tail -40
    echo; echo "=== /data/power"; ls -la /data/power 2>&1
    for f in /data/power/*; do [ -f "$f" ] && [ $(stat -c %s "$f") -lt 2000 ] && echo "--- $f: $(cat "$f" 2>&1)"; done
    echo; echo "=== power_manage strings"; pr /bin/power_manage "power\|suspend\|reboot\|control\|wake\|usb\|led\|/data\|/sys"
    echo; echo "=== systemd units mentioning usb_watch / pman / power"; grep -l "usb_watch\|start_pman\|power_manage\|usb" /lib/systemd/system/* /etc/systemd/system/* 2>/dev/null
    for f in $(grep -l "usb_watch\|start_pman\|power_manage" /lib/systemd/system/* /etc/systemd/system/* 2>/dev/null); do echo "--- $f"; cat "$f"; done
    echo; echo "=== udev rules mentioning media/usb"; grep -l "media\|usb_watch\|sd" /etc/udev/rules.d/* /lib/udev/rules.d/*usb* /lib/udev/rules.d/*media* 2>/dev/null
    for f in /etc/udev/rules.d/*; do echo "--- $f"; cat "$f"; done
    echo; echo "=== systemctl status of the unit that runs usb_watch"
    systemctl status --no-pager -l 2>&1 | head -60
    echo; echo "=== end"
} > "$OUT" 2>&1
mkdir -p /media/System/Logs
cp "$OUT" /media/System/Logs/mount_debug_$TAG.txt
sync
