#!/bin/sh

# What happens after autobleem-gui exits. boot.sh runs this from a copy on tmpfs, with the cwd on tmpfs,
# and starts the launcher over when it returns 0; anything else ends in a reboot, which brings AutoBleem
# back up through Sony's boot standby. AutoBleem::run() writes AB_SELECTION into autobleem_cfg.sh in the
# runtime dir (RAM, rc/ab_log.sh) on its way out (LaunchService::writeSelectionScript):
#   4  exit to RetroArch (the launcher's L2+R2 system menu): RetroArch's own menu, then the launcher again
#   6  install the update the launcher downloaded into System/Updates (a console with a network - the
#      AutoBleem kernel's WiFi): abupdate lays it over the stick, then the new launcher starts
#   7  power off: the standby below, then the launcher again when the power button wakes the console
#   8  a new display mode (Options -> Display): straight back to boot.sh, which restarts Weston in it and
#      starts the launcher again - no reboot
# After a crash, or a first boot where it never ran, the file is not there - then there is no selection
# and the reboot is what happens. The file is removed once read, so a crash after a standby is not
# taken for another power off.

SEL_RETROARCH=4
SEL_UPDATE=6
SEL_POWEROFF=7
SEL_DISPLAY=8

RC=/media/Autobleem/rc
. $RC/ab_log.sh
# a standby that worked is logged with the rest of the run (RAM unless kept); what went wrong - the stick
# busy, no standby at all - goes to the stick, where it is still there after the reboot that follows
LOG=$AB_LOG_DIR/standby.log
FAILLOG=/media/System/Logs/standby.log

# the watch log's helper (ab_watch.sh) runs from a tmpfs copy; this ends it before the stick is unmounted
ab_watch_stop() {
    [ -f /tmp/ab_watch_run.sh ] && sh /tmp/ab_watch_run.sh stop
}

# CONSOLE-15 P2: the Power Off path writes one numbered line per step, so a reset leaves the last step done as its
# trace (the RAM log is lost with it). mark_ram: RAM ($SLOG) and the kernel log only. mark: also on the stick
# (FAILLOG, synced) - straight away while /media is mounted, else (AB_MARK_REMOUNT=1) by a quick mount, append, sync,
# umount of the same stick; a line on the stick is tagged [stick] in $SLOG so the log copy does not repeat it.
# Only the power off writes any of it, the quiet-stick rule holds.
AB_MARK_REMOUNT=1
STEP=0
mark_line() {
    STEP=$((STEP + 1))
    MARK_LINE="$(date) step $STEP: $* up=$(cut -d' ' -f1 /proc/uptime)"
}
mark_ram() {
    mark_line "$@"
    echo "$MARK_LINE" >> $SLOG
    echo "ab_standby: $MARK_LINE" > /dev/kmsg 2>/dev/null
}
mark() {
    mark_line "$@"
    echo "ab_standby: $MARK_LINE" > /dev/kmsg 2>/dev/null
    if grep -q ' /media ' /proc/mounts; then
        echo "$MARK_LINE" >> $FAILLOG
        sync
        echo "$MARK_LINE [stick]" >> $SLOG
    elif [ "$AB_MARK_REMOUNT" = 1 ] && [ -n "$DEV" ] && mount "$DEV" /media 2> /dev/null; then
        echo "$MARK_LINE" >> $FAILLOG
        sync
        m=0
        until umount /media 2> /dev/null || [ $m -ge 5 ]; do
            m=$((m + 1))
            sleep 1
        done
        if [ -f /tmp/ab_stick_owned ] && [ -x /tmp/abfatflag ]; then
            /tmp/abfatflag "$DEV" clean
        fi
        sync
        echo "$MARK_LINE [stick]" >> $SLOG
    else
        echo "$MARK_LINE (not on the stick)" >> $SLOG
    fi
}

# CONSOLE-15 P2: what the console looks like before the first step (read-only on /sys and /proc), into FAILLOG
# while the stick is mounted: the watchdog (and who holds /dev/watchdog), every USB device with its name (the rear
# port: musb-hdrc.0.auto), the power sources, the OTG glue's modes, the boot reason on the kernel's command line.
state_dump() {
    echo "-- state before the first step: $(uname -r)"
    echo "cmdline: $(cat /proc/cmdline 2>/dev/null)"
    for w in /sys/class/watchdog/watchdog*; do
        [ -d "$w" ] || continue
        echo "watchdog ${w##*/}: id=$(cat $w/identity 2>/dev/null) timeout=$(cat $w/timeout 2>/dev/null) timeleft=$(cat $w/timeleft 2>/dev/null) nowayout=$(cat $w/nowayout 2>/dev/null) state=$(cat $w/state 2>/dev/null)"
    done
    for d in /proc/[0-9]*; do
        ls -l $d/fd 2>/dev/null | grep -q /dev/watchdog && echo "watchdog held by ${d#/proc/} $(cat $d/comm 2>/dev/null)"
    done
    for u in /sys/bus/usb/devices/*; do
        [ -f "$u/idVendor" ] || continue
        echo "usb ${u##*/}: $(cat $u/idVendor):$(cat $u/idProduct) \"$(cat $u/manufacturer 2>/dev/null)\" \"$(cat $u/product 2>/dev/null)\" ctrl=$(readlink -f $u | sed -n 's|.*/\([^/]*\)/usb[0-9]*.*|\1|p') power=$(cat $u/power/control 2>/dev/null)/$(cat $u/power/runtime_status 2>/dev/null)"
    done
    for p in /sys/class/power_supply/*; do
        [ -d "$p" ] && echo "power_supply ${p##*/}: type=$(cat $p/type 2>/dev/null) online=$(cat $p/online 2>/dev/null) status=$(cat $p/status 2>/dev/null)"
    done
    echo "musb: charger_info=$(cat /sys/module/musb_hdrc/parameters/charger_info 2>/dev/null) swmode=$(cat /sys/devices/platform/mt_usb/swmode 2>/dev/null) mode=$(cat /sys/devices/platform/mt_usb/mode 2>/dev/null)"
    echo "gadget=$(cat /sys/class/android_usb/android0/enable 2>/dev/null) pm: state=$(cat /sys/power/state 2>/dev/null) autosleep=$(cat /sys/power/autosleep 2>/dev/null)"
    echo "mounts: $(grep -E ' /media | /data ' /proc/mounts | tr '\n' ';')"
    echo
}

AB_SELECTION=0
[ -f "$AB_RUNTIME_DIR/autobleem_cfg.sh" ] && . "$AB_RUNTIME_DIR/autobleem_cfg.sh"
rm -f "$AB_RUNTIME_DIR/autobleem_cfg.sh"
rm -f $RC/autobleem_cfg.sh # where a launcher before the quiet-stick plan wrote it
echo Selection: $AB_SELECTION

# the emulator the launch script execs; a fresh copy on tmpfs every boot
echo Custom PCSX
cp -f /media/Autobleem/bin/emu/pcsx-ab /tmp/pcsx
[ -f /tmp/pcsx ] && chmod +x /tmp/pcsx

# When the kernel will not suspend at all: a real power off, as AutoBleem 1.x did it. A USB host on the
# micro-USB (power) port - the AutoBleem kernel's OTG, a hub with the stick on it - refuses suspend-to-RAM
# while it serves a device ("trying to suspend as a_host while active", usb1 error -16); shutdown only
# runs the drivers' shutdown hooks, so nothing can refuse it. The POWER button is then a cold boot, not a
# quick wake. The stick is still attached (no suspend happened): mounted again for the log, then unmounted
# here and its flag cleared as standby() does - not left to systemd's unmount on the way down: a stick that
# came out of that power off dirty was reported (2026-09-25, the stick on the rear port, Windows' "scan and
# fix" after the red LED).
poweroff_instead() {
    echo "$(date) no standby on this console - powering off instead" >> $SLOG
    mark_ram "poweroff_instead entered (dev=$DEV)"
    # CONSOLE-15 P2: still mounted when the standby was skipped (rear host); else mounted again for the log - and when
    # the old name is gone (the stick re-enumerated under another one in a half-done suspend) the same search as the
    # wake's
    if grep -q ' /media ' /proc/mounts || mount "$DEV" /media 2>> $SLOG ||
        { DEV="$(blkid | grep "^/dev/sd[a-z]1:" | grep -E "LABEL=\"SONY.{0,4}\"" | awk -F: '{print $1}' | head -1)"; [ -n "$DEV" ] && mount "$DEV" /media 2>> $SLOG; }; then
        mark "poweroff_instead: stick mounted ($DEV), log goes to it"
        { grep -v ' \[stick\]$' $SLOG; echo; } >> $FAILLOG # the [stick] lines are on the stick already
        sync
        mark "poweroff_instead: LED red, umount and shutdown -h now are next"
        n=0
        until umount /media 2>/dev/null || [ $n -ge 5 ]; do
            n=$((n + 1))
            sleep 1
        done
        if [ -f /tmp/ab_stick_owned ] && [ -x /tmp/abfatflag ]; then
            /tmp/abfatflag "$DEV" clean
        fi
        sync
    else
        mark_ram "poweroff_instead: the stick could not be mounted for the log (dev=$DEV)"
    fi
    echo 0 > /sys/class/leds/green/brightness
    echo 1 > /sys/class/leds/red/brightness
    mark_ram "poweroff_instead: shutdown -h now (the stick is unmounted)"
    shutdown -h now
    sleep 120 # never back here: the launcher must not start again while the system goes down
    systemctl reboot
}

# The console's "power off", the way Sony's own power_manage does it - suspend to RAM, the power button
# wakes it - but with the stick unmounted first, so it can be pulled while the console is "off" without
# coming back dirty (Windows' "scan and fix"). The red LED alone is the AutoBleem standby; the USB bus is
# reset by the resume and the stick comes back a few seconds later (the same name as before, or the
# other one), and is mounted again exactly as usb_watch mounted it. Nothing here touches the console's
# own storage. Returns 0 with the stick mounted again, 1 when it is not back within 60 s (30 s, then 30 s more).
standby() {
    ab_watch_stop # the watch log (CONSOLE-15) must not touch /media or run through the suspend
    DEV=$(awk '$2 == "/media" { print $1 }' /proc/mounts | head -1)
    SLOG=/tmp/standby.log
    echo "$(date) standby: dev=$DEV" > $SLOG
    rm -f /media/System/.session # the session ended cleanly - checkstick.sh may clear the flag next boot
    sync
    # CONSOLE-15 P2: the state BEFORE the dangerous step, on the stick (the umount below is the last time it can be
    # written; a reset in the suspend leaves no other trace). Read-only on /sys and /proc; the marker is read and
    # deleted by boot.sh at the next start. Only a power off writes this - the quiet-stick rule holds.
    # whether anything is plugged in at the rear (micro-USB, OTG) port now: only the AutoBleem kernel runs it as a
    # host, and a device there may not come back after the resume (see below)
    # (the bus is found by its controller, musb-hdrc.0.auto - the front ports are musbfsh - not by its number)
    REAR_HAD=0
    REAR_DEV=""
    for u in /sys/bus/usb/devices/usb*; do
        case "$(readlink -f $u)" in
        */musb-hdrc.0.auto/usb*) REAR_DEV=/sys/bus/usb/devices/${u##*/usb}-1 ;;
        esac
    done
    [ -n "$REAR_DEV" ] && [ -e "$REAR_DEV" ] && REAR_HAD=1
    mkdir -p /media/System/Logs
    {
        echo "$(date) power-off requested: uptime=$(cut -d' ' -f1 /proc/uptime) rtc=$(cat /sys/class/rtc/rtc0/since_epoch 2>/dev/null) rear_dev=${REAR_DEV:-none} rear_had=$REAR_HAD wakelocks=[$(cat /sys/power/wake_lock 2>/dev/null)]"
        echo "-- dmesg | tail -20"
        dmesg 2>&1 | tail -20
        echo
    } >> $FAILLOG
    echo "uptime=$(cut -d' ' -f1 /proc/uptime) standby requested" > /media/System/Logs/poweroff_reason
    sync
    mark "power-off marker and poweroff_reason written, the watch log is stopped"
    state_dump >> $FAILLOG
    sync
    mark "state before the first step saved (watchdog, usb, power supplies, boot reason)"
    # CONSOLE-15 P2 (the fix under test): a USB host on the rear (OTG) port refuses suspend-to-RAM every time (see
    # poweroff_instead), and the steps on the way to that refusal - the gadget switched off on the same port, the
    # LED, three refused 'echo mem' - are where a reset in the first seconds could hide. So with a rear device the
    # power off goes straight to poweroff_instead, the stick still mounted. AB_REAR_SKIP_SUSPEND=0 in the environment
    # (or here) gives the old path back.
    : "${AB_REAR_SKIP_SUSPEND:=1}"
    if [ "$REAR_HAD" = 1 ] && [ "$AB_REAR_SKIP_SUSPEND" = 1 ]; then
        mark "rear USB device present ($REAR_DEV): no suspend attempt, straight to the power off"
        poweroff_instead
    fi
    mark "umount /media next"
    n=0
    until umount /media; do
        n=$((n + 1))
        if [ $n -ge 5 ]; then
            echo "$(date) standby: /media is busy - holders:" >> $FAILLOG
            for d in /proc/[0-9]*; do
                p=${d#/proc/}
                case "$(readlink $d/cwd 2>/dev/null)" in /media*) echo "  $p cwd $(cat $d/comm)" >> $FAILLOG ;; esac
                ls -l $d/fd 2>/dev/null | grep -q /media && echo "  $p fd $(cat $d/comm)" >> $FAILLOG
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
    mark "/media unmounted, fat flag cleared"

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
    mark "USB gadget handled (was on: $GADGET_ON)"

    echo 0 > /sys/class/leds/green/brightness
    echo 1 > /sys/class/leds/red/brightness
    sync
    mark "LED red set, echo mem next"
    # a refused suspend fails the write (EBUSY, a wakeup source held); a real one returns after the wake.
    # Refused: what held it goes to the log, and two more tries for a passing wakelock.
    try=1
    until mark "echo mem try $try starts" && echo mem > /sys/power/state 2>> $SLOG; do
        echo "$(date) the kernel refused to suspend (try $try) - wakelocks: $(cat /sys/power/wake_lock 2>/dev/null)" >> $SLOG
        dmesg | tail -60 | grep -iE 'failed to suspend|while active|early wake|abort|wakeup pending' >> $SLOG
        if [ $try -ge 3 ]; then
            poweroff_instead
        fi
        try=$((try + 1))
        sleep 2
    done
    echo 1 > /sys/class/leds/green/brightness
    echo 0 > /sys/class/leds/red/brightness
    if [ $GADGET_ON = 1 ]; then
        # the overlay's own script brings it back as it came up at boot (the gadget, rndis0's address, ssh)
        if [ -x /etc/autobleem/rndis ] || [ -f /etc/autobleem/rndis ]; then
            # in the background: the overlay's start() ends in tcpsvd (its FTP server), which stays in the
            # foreground and never returns - waited for, it hung the wake with the green LED and a black screen
            ( bash /etc/autobleem/rndis restart > /dev/null 2>&1 & )
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

    # The rear (OTG) port on the AutoBleem kernel: MediaTek's musb driver does not restart its host session after
    # a resume, so a hub or dongle there stays gone - WiFi and Bluetooth dead after every wake (2026-09-26: the
    # front bus came back in 2 s, the rear hub never did). Its glue's own switch redoes the host bring-up
    # (musb_id_pin_sw_work: VBUS, session, PHY) and the hub re-enumerates within 2 s; unbinding the driver instead
    # leaves the port dead (its probe cannot run twice). Only when something was there before the standby and is
    # still missing - the stock kernel, an empty rear port or a device that came back: nothing, no wait.
    REAR_MODE=/sys/devices/platform/mt_usb/swmode
    if [ "$REAR_HAD" = 1 ] && [ -w $REAR_MODE ]; then
        j=0
        while [ ! -e "$REAR_DEV" ] && [ $j -lt 2 ]; do
            j=$((j + 1))
            sleep 1
        done
        if [ ! -e "$REAR_DEV" ]; then
            echo "$(date) rear USB: its device did not come back - restarting the port's host mode" >> $SLOG
            echo idle > $REAR_MODE # the first time a no-op (the switch only tracks its own writes)
            sleep 1
            echo host > $REAR_MODE
        fi
    fi

    # CONSOLE-15 evidence: the network after the wake (the 3 s + the rear port's wait above), each call bounded
    {
        echo "$(date) network after the wake:"
        echo "== iw dev wlan0 link"
        ab_timeout iw dev wlan0 link
        echo "== ip addr show wlan0"
        ab_timeout ip addr show wlan0
        echo "== wpa_cli -i wlan0 status"
        ab_timeout wpa_cli -i wlan0 status
        echo "== dmesg | tail -60"
        dmesg 2>&1 | tail -60
    } >> $SLOG 2>&1

    i=0
    while [ $i -lt 60 ]; do
        [ $i = 30 ] && echo "$(date) no stick after 30 s - retrying for another 30 s" >> $SLOG
        # any sd?1, not only sda1/sdb1 as usb_watch looks: a stick pulled during the standby and plugged back
        # can come back under a new name while the kernel still holds the old one, and the wake then found
        # nothing and rebooted (2026-09-25)
        DEV="$(blkid | grep "^/dev/sd[a-z]1:" | grep -E "LABEL=\"SONY.{0,4}\"" | awk -F: '{print $1}' | head -1)"
        [ $i = 0 ] && { echo "$(date) after the wake:"; blkid | grep '^/dev/sd' | sed 's/^/  /'; } >> $SLOG
        [ -n "$DEV" ] && mount "$DEV" /media && break
        DEV=""
        i=$((i + 1))
        sleep 1
    done
    if [ -z "$DEV" ]; then
        rm -f /tmp/.abload
        echo "$(date) no stick within 60 s after the wake - rebooting" >> $SLOG
        blkid | sed 's/^/  blkid: /' >> $SLOG
        # the stick is the one place this log belongs and it is missing: to the kernel log and the journal, where
        # it is still readable after the reboot if the system keeps them
        while IFS= read -r l; do
            echo "ab_standby: $l" > /dev/kmsg 2>/dev/null
            command -v logger > /dev/null 2>&1 && logger -t ab_standby -- "$l"
        done < $SLOG
        return 1
    fi
    rm -f /tmp/ab_stick_owned # a fresh mount: the kernel owns the flag again (clean at mount, or not ours)
    touch /media/System/.session
    rm -f /media/System/Logs/poweroff_reason # the power-off worked: a later boot is not 'after a power-off request'
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
    if [ -x /tmp/absplash ] && [ -f /tmp/autobleem.jpg ]; then
        touch /tmp/.abupdating
        LD_LIBRARY_PATH=/tmp/lib /tmp/absplash /tmp/autobleem.jpg --until-gone /tmp/.abupdating --timeout 900 > /dev/null 2>&1 &
    fi
    echo "$(date) installing the downloaded update" >> $ULOG
    cd /tmp
    LD_LIBRARY_PATH=/tmp/lib /tmp/abupdate /media >> $ULOG 2>&1
    echo "$(date) abupdate exit status $?" >> $ULOG
    sync
    rm -f /tmp/.abupdating /tmp/abupdate
    # the emulator copy above was the old one (autobleem.sh unpacks the new libraries itself)
    cp -f /media/Autobleem/bin/emu/pcsx-ab /tmp/pcsx 2>/dev/null && chmod +x /tmp/pcsx
}

case "$AB_SELECTION" in
"$SEL_DISPLAY")
    exit 0
    ;;
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
*)
    # no selection (or none the launcher leaves with): it did not leave the way it does - a crash, killed.
    # Its logs are in RAM, which a reboot would empty, so they go to the stick first
    ab_watch_stop
    ab_persist_logs "autobleem-gui ended without a selection (AB_SELECTION=$AB_SELECTION) - a crash?"
    # The reboot that used to follow is a FULL RESET: Sony's boot (start_pman's "echo mem") then puts the console
    # into standby until POWER is pressed. So a crash starts the launcher over instead (boot.sh's loop, exit 0),
    # unless it keeps crashing: 3 crashes within 10 minutes (uptime seconds, one per line in the runtime dir -
    # the clock is not set at boot, so not date) fall back to the reboot, and say why.
    CRASHES=$AB_RUNTIME_DIR/crash_times
    NOW=$(cut -d. -f1 /proc/uptime)
    { cat "$CRASHES" 2>/dev/null; echo "$NOW"; } | tail -n 3 > "$CRASHES.new" && mv -f "$CRASHES.new" "$CRASHES"
    RECENT=$(awk -v now="$NOW" '$1 >= now - 600 { n++ } END { print n + 0 }' "$CRASHES")
    if [ "$RECENT" -ge 3 ]; then
        echo "$(date) crash loop: $RECENT launcher crashes within 10 minutes (uptime $NOW s) - rebooting" >> $FAILLOG
    else
        echo "$(date) launcher crash $RECENT of 3 within 10 minutes - starting it again, no reboot" >> $FAILLOG
        # what the crashed launcher can leave that would confuse a fresh start: its wake-up/update picture flags
        # (an absplash waits on them), the hand-over files (autobleem_cfg.sh is gone already, read above).
        # Kept on purpose: extensions.active (the extensions' crash guard - the next launcher reads it from here, as it
        # would from the stick after a reboot) and outputmode.pending
        # (boot.sh's apply_output_mode redoes Weston before the start, whatever mode the crash left).
        rm -f /tmp/.abload /tmp/.abupdating "$AB_RUNTIME_DIR/autobleem_cfg.sh"
        sync
        exit 0
    fi
    ;;
esac

sync
umount /media 2>/dev/null
sync
systemctl reboot
