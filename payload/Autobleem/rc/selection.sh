#!/bin/sh

# What happens after autobleem-gui exits. boot.sh runs this from a copy on tmpfs, with the cwd on tmpfs,
# and starts the launcher over when it returns 0; anything else ends in a reboot, which brings AutoBleem
# back up through Sony's boot standby. AutoBleem::run() writes AB_SELECTION into autobleem_cfg.sh on its
# way out (LaunchService::writeSelectionScript):
#   4  exit to RetroArch (the launcher's L2+R2 system menu): RetroArch's own menu, then the launcher again
#   7  power off: the standby below, then the launcher again when the power button wakes the console
# After a crash, or a first boot where it never ran, the file is not there - then there is no selection
# and the reboot is what happens. The file is removed once read, so a crash after a standby is not
# taken for another power off.

SEL_RETROARCH=4
SEL_POWEROFF=7

RC=/media/Autobleem/rc
LOG=/media/System/Logs/standby.log

AB_SELECTION=0
[ -f $RC/autobleem_cfg.sh ] && . $RC/autobleem_cfg.sh
rm -f $RC/autobleem_cfg.sh
echo Selection: $AB_SELECTION

# the emulator the launch script execs; a fresh copy on tmpfs every boot
echo Custom PCSX
cp -f /media/Autobleem/bin/emu/pcsx-ab /tmp/pcsx
[ -f /tmp/pcsx ] && chmod +x /tmp/pcsx

# The console's "power off", the way Sony's own power_manage does it - suspend to RAM, the power button
# wakes it - but with the stick unmounted first, so it can be pulled while the console is "off" without
# coming back dirty (Windows' "scan and fix"). The red LED alone is the AutoBleem standby; the USB bus is
# reset by the resume and the stick comes back a few seconds later (the same name as before, or the
# other one), and is mounted again exactly as usb_watch mounted it. Nothing here touches the console's
# own storage. Returns 0 with the stick mounted again, 1 when it is not back within 30 s.
standby() {
    DEV=$(awk '$2 == "/media" { print $1 }' /proc/mounts | head -1)
    rm -f /media/System/.session # the session ended cleanly - checkstick.sh may clear the flag next boot
    sync
    n=0
    until umount /media; do
        n=$((n + 1))
        if [ $n -ge 5 ]; then
            echo "standby: /media is busy - holders:" >> $LOG
            for d in /proc/[0-9]*; do
                p=${d#/proc/}
                case "$(readlink $d/cwd 2>/dev/null)" in /media*) echo "  $p cwd $(cat $d/comm)" >> $LOG ;; esac
                ls -l $d/fd 2>/dev/null | grep -q /media && echo "  $p fd $(cat $d/comm)" >> $LOG
            done
            return 1
        fi
        sleep 1
    done
    # a flag the kernel could not clear (it never touches one it found set at mount): ours to clear, see
    # checkstick.sh
    if [ -f /tmp/ab_stick_owned ] && [ -x /tmp/abfatflag ]; then
        /tmp/abfatflag "$DEV" clean
        sync
    fi

    echo 0 > /sys/class/leds/green/brightness
    echo 1 > /sys/class/leds/red/brightness
    sync
    echo mem > /sys/power/state
    echo 1 > /sys/class/leds/green/brightness
    echo 0 > /sys/class/leds/red/brightness

    # the AutoBleem picture from now until the launcher's window is up (it removes /tmp/.abload itself,
    # as after RetroArch) - the ten seconds of the bus, the mount and the launcher's start were black
    if [ -x /tmp/absplash ] && [ -f /tmp/autobleem.jpg ]; then
        touch /tmp/.abload
        LD_LIBRARY_PATH=/tmp/lib /tmp/absplash /tmp/autobleem.jpg --until-gone /tmp/.abload --timeout 40 &
    fi

    sleep 3 # the USB bus re-enumerates after the resume
    i=0
    while [ $i -lt 30 ]; do
        DEV="$(blkid | grep "^/dev/sd\(a\|b\)1:" | grep -E "LABEL=\"SONY.{0,4}\"" | awk -F: '{print $1}' | head -1)"
        [ -n "$DEV" ] && mount "$DEV" /media && break
        DEV=""
        i=$((i + 1))
        sleep 1
    done
    if [ -z "$DEV" ]; then
        rm -f /tmp/.abload
        return 1
    fi
    rm -f /tmp/ab_stick_owned # a fresh mount: the kernel owns the flag again (clean at mount, or not ours)
    touch /media/System/.session
    return 0
}

case "$AB_SELECTION" in
"$SEL_RETROARCH")
    $RC/retroarch.sh
    exit 0
    ;;
"$SEL_POWEROFF")
    if standby; then
        exit 0
    fi
    # /media busy, or no stick after the wake: the reboot brings whatever is plugged in back up
    ;;
esac

sync
umount /media 2>/dev/null
sync
reboot
