#!/bin/sh

# What happens after autobleem-gui exits. boot.sh runs this from a copy on tmpfs, with the cwd on tmpfs,
# and starts the launcher over when it returns 0; anything else ends in a reboot, which brings AutoBleem
# back up through Sony's boot standby. AutoBleem::run() writes AB_SELECTION into autobleem_cfg.sh on its
# way out (LaunchService::writeSelectionScript):
#   4  exit to RetroArch (the launcher's L2+R2 system menu): RetroArch's own menu, then the launcher again
#   6  install the update the launcher downloaded into System/Updates (a console with a network - the
#      AutoBleem kernel's WiFi): abupdate lays it over the stick, then the new launcher starts
#   7  power off: the standby below, then the launcher again when the power button wakes the console
# After a crash, or a first boot where it never ran, the file is not there - then there is no selection
# and the reboot is what happens. The file is removed once read, so a crash after a standby is not
# taken for another power off.

SEL_RETROARCH=4
SEL_UPDATE=6
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
    SLOG=/tmp/standby.log
    echo "$(date) standby: dev=$DEV" > $SLOG
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

    # The AutoBleem kernel's overlay starts a USB network (RNDIS, /etc/autobleem/rndis) on the power port at
    # boot. While that gadget is up the port keeps the system awake, so suspend-to-RAM was refused at once -
    # the write returned straight away, which read as a wake here and started the launcher over. Off for the
    # standby, back on after it. The stock firmware has no gadget up; nothing changes there.
    GADGET=/sys/class/android_usb/android0/enable
    GADGET_ON=0
    if [ -w $GADGET ] && [ "$(cat $GADGET 2>/dev/null)" = 1 ]; then
        GADGET_ON=1
        echo 0 > $GADGET
        echo "$(date) USB gadget (RNDIS) off for the standby" >> $SLOG
        sleep 1
    fi

    echo 0 > /sys/class/leds/green/brightness
    echo 1 > /sys/class/leds/red/brightness
    sync
    # a refused suspend fails the write (EBUSY, a wakeup source held); a real one returns after the wake.
    # Refused: what held it goes to the log, and two more tries for a passing wakelock.
    try=1
    until echo mem > /sys/power/state 2>> $SLOG; do
        echo "$(date) the kernel refused to suspend (try $try) - wakelocks: $(cat /sys/power/wake_lock 2>/dev/null)" >> $SLOG
        dmesg | tail -20 | grep -iE 'PM:|suspend|wakeup|wake_lock|abort|active' >> $SLOG
        [ $try -ge 3 ] && break
        try=$((try + 1))
        sleep 2
    done
    echo 1 > /sys/class/leds/green/brightness
    echo 0 > /sys/class/leds/red/brightness
    if [ $GADGET_ON = 1 ]; then
        # the overlay's own script brings it back as it came up at boot (the gadget, rndis0's address, ssh)
        if [ -x /etc/autobleem/rndis ] || [ -f /etc/autobleem/rndis ]; then
            bash /etc/autobleem/rndis restart > /dev/null 2>&1
        else
            echo 1 > $GADGET
        fi
        echo "$(date) USB gadget (RNDIS) back on" >> $SLOG
    fi

    # the AutoBleem picture from now until the launcher's window is up (it removes /tmp/.abload itself,
    # as after RetroArch) - the ten seconds of the bus, the mount and the launcher's start were black
    echo "$(date) resumed" >> $SLOG
    if [ -x /tmp/absplash ] && [ -f /tmp/autobleem.jpg ]; then
        touch /tmp/.abload
        LD_LIBRARY_PATH=/tmp/lib /tmp/absplash /tmp/autobleem.jpg --until-gone /tmp/.abload --timeout 40 > /tmp/absplash.log 2>&1 &
    else
        echo "no absplash on tmpfs" >> $SLOG
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
    echo "$(date) mounted $DEV after $i s" >> $SLOG
    { cat $SLOG; [ -f /tmp/absplash.log ] && sed 's/^/  absplash: /' /tmp/absplash.log; } >> $LOG
    return 0
}

# The update the launcher downloaded: abupdate (the PC installer's own code, autobleem-core's InstallerJob)
# replaces what the package ships - Autobleem/bin, the rc scripts, the console tools, the shipped themes -
# and keeps everything of the user's. It runs from tmpfs because Autobleem/bin/autobleem is what it
# replaces; the AutoBleem picture is on the screen meanwhile. Either way the launcher comes back: the new
# one, or - on a failure, which update.log explains - the old one with System/Updates kept.
update() {
    ULOG=/media/System/Logs/update.log
    cp -f /media/Autobleem/bin/autobleem/abupdate /tmp/abupdate 2>/dev/null && chmod +x /tmp/abupdate
    if [ ! -x /tmp/abupdate ]; then
        echo "$(date) no abupdate on the stick - the update is not installed" >> $ULOG
        return
    fi
    # abfetch (the launcher's own downloader) and its CA bundle go along: abupdate fetches UpdateRoms with
    # it, and the folder they are in is being replaced
    cp -f /media/Autobleem/bin/autobleem/abfetch /tmp/abfetch 2>/dev/null && chmod +x /tmp/abfetch
    cp -f /media/Autobleem/bin/autobleem/cacert.pem /tmp/cacert.pem 2>/dev/null
    if [ -x /tmp/absplash ] && [ -f /tmp/autobleem.jpg ]; then
        touch /tmp/.abupdating
        LD_LIBRARY_PATH=/tmp/lib /tmp/absplash /tmp/autobleem.jpg --until-gone /tmp/.abupdating --timeout 900 > /dev/null 2>&1 &
    fi
    echo "$(date) installing the downloaded update" >> $ULOG
    cd /tmp
    LD_LIBRARY_PATH=/tmp/lib /tmp/abupdate /media >> $ULOG 2>&1
    status=$? # before the date below: a command substitution sets $? too
    echo "$(date) abupdate exit status $status" >> $ULOG
    sync
    rm -f /tmp/.abupdating /tmp/abupdate /tmp/abfetch /tmp/cacert.pem
    # the emulator copy above was the old one (autobleem.sh unpacks the new libraries itself)
    cp -f /media/Autobleem/bin/emu/pcsx-ab /tmp/pcsx 2>/dev/null && chmod +x /tmp/pcsx
}

case "$AB_SELECTION" in
"$SEL_RETROARCH")
    $RC/retroarch.sh
    exit 0
    ;;
"$SEL_UPDATE")
    update
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
