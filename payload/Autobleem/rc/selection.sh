#!/bin/sh

# What happens after autobleem-gui exits. boot.sh runs this from a copy on tmpfs, with the cwd on tmpfs,
# and starts the launcher over when it returns 0; anything else ends in a reboot, which brings AutoBleem
# back up through Sony's boot standby. AutoBleem::run() writes AB_SELECTION into autobleem_cfg.sh in the
# runtime dir (RAM, rc/ab_log.sh) on its way out (LaunchService::writeSelectionScript):
#   4  exit to RetroArch (the launcher's L2+R2 system menu): RetroArch's own menu, then the launcher again
#   6  install the update the launcher downloaded into System/Updates (a console with a network - the
#      AutoBleem kernel's WiFi): abupdate lays it over the stick (the AutoBleem package, and/or RetroArch's
#      zip over RetroArch/bin), then the new launcher starts
#   7  power off: the standby below, then the launcher again when the power button wakes the console
#   8  a new display mode (Options -> Display): straight back to boot.sh, which restarts Weston in it and
#      starts the launcher again - no reboot
#   9  no stick (AUTOBLEEM-28): not the launcher's - rc/ab_stickwatch.sh writes it when the stick was pulled while
#      the launcher, a game or this script's own work ran; the insert-stick picture until it is back (nostick below)
# After a crash, or a first boot where it never ran, the file is not there - then there is no selection
# and the reboot is what happens. The file is removed once read, so a crash after a standby is not
# taken for another power off.

SEL_RETROARCH=4
SEL_UPDATE=6
SEL_POWEROFF=7
SEL_DISPLAY=8
SEL_NOSTICK=9

RC=/media/Autobleem/rc
# the stick's own, or (no stick: selection 9) the tmpfs copy boot.sh takes before every launcher start
if [ -f $RC/ab_log.sh ]; then
    . $RC/ab_log.sh
else
    . /tmp/ab_log.sh
fi
# ab_timeout comes from ab_log.sh; a stick whose ab_log.sh is older lacks it (the Wi-Fi calls below then never ran:
# "ab_timeout: command not found", CONSOLE-15 P2 repro 3) - a bounded call here either way
if ! type ab_timeout > /dev/null 2>&1; then
    if timeout 1 true 2> /dev/null; then
        ab_timeout() { timeout 5 "$@"; }
    elif timeout -t 1 true 2> /dev/null; then
        ab_timeout() { timeout -t 5 "$@"; }
    else
        ab_timeout() { "$@"; }
    fi
fi
# a standby that worked is logged with the rest of the run (RAM unless kept); what went wrong - the stick
# busy, no standby at all - goes to the stick, where it is still there after the reboot that follows
LOG=$AB_LOG_DIR/standby.log
FAILLOG=/media/System/Logs/standby.log

# the watch log's helper (ab_watch.sh) and the stick's watch (ab_stickwatch.sh) run from tmpfs copies; this ends
# them before the stick is unmounted
ab_watch_stop() {
    [ -f /tmp/ab_watch_run.sh ] && sh /tmp/ab_watch_run.sh stop
    [ -f /tmp/ab_stickwatch_run.sh ] && sh /tmp/ab_stickwatch_run.sh stop
}

# CONSOLE-15 P2: the Power Off path writes one numbered line per step, so a reset leaves the last step done as its
# trace (the RAM log is lost with it). mark_ram: RAM ($SLOG) and the kernel log only. mark: also on the stick
# (FAILLOG, synced) - straight away while /media is mounted, else (AB_MARK_REMOUNT=1) by a quick mount, append, sync,
# umount of the same stick; a line on the stick is tagged [stick] in $SLOG so the log copy does not repeat it.
# Only the power off writes any of it, the quiet-stick rule holds.
: "${AB_MARK_REMOUNT:=0}" # 1: also a quick remount per step after the umount (the stick-side trace of a reset)
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
cp -f /media/Autobleem/bin/emu/pcsx-ab /tmp/pcsx 2> /dev/null
[ -f /tmp/pcsx ] && chmod +x /tmp/pcsx

# The power LED while the launcher loads after a wake (the owner's report, 2026-10-02: at a first boot it blinks
# orange/green until AutoBleem is up - that blink is Sony's own boot, boot.sh writes no LED - but after a wake from
# the standby it went straight to solid green and looked finished long before the launcher was). The same pattern as
# AutoBleem 1.x's flashleds(): orange (red + green) / green, 250 ms each. A background loop, its pid in
# /tmp/.abledblink.pid; it ends by itself when the launcher removes /tmp/.abload, or after the splash's own 40 s,
# and leaves the LED solid green. Every other LED writer stops it first (led_blink_stop, by that pid only) and
# the loop also checks its pid file before each write, so a red set by the standby or poweroff_instead is never
# overwritten by a late step of it.
LEDBLINK_PID=/tmp/.abledblink.pid
led_blink_stop() {
    [ -s $LEDBLINK_PID ] && kill "$(cat $LEDBLINK_PID)" 2>/dev/null
    rm -f $LEDBLINK_PID
}
led_blink_pause() {
    usleep 250000 2> /dev/null || sleep 0.25 2> /dev/null || sleep 1
}
led_blink_start() {
    led_blink_stop
    : > $LEDBLINK_PID # the loop checks it is there (written before the fork, the pid follows)
    (
        n=0
        while [ -f /tmp/.abload ] && [ -f $LEDBLINK_PID ] && [ $n -lt 80 ]; do
            echo 1 > /sys/class/leds/red/brightness
            echo 1 > /sys/class/leds/green/brightness
            led_blink_pause
            [ -f $LEDBLINK_PID ] || exit 0
            echo 0 > /sys/class/leds/red/brightness
            led_blink_pause
            n=$((n + 1))
        done
        [ -f $LEDBLINK_PID ] || exit 0
        echo 0 > /sys/class/leds/red/brightness
        echo 1 > /sys/class/leds/green/brightness
        rm -f $LEDBLINK_PID
    ) < /dev/null > /dev/null 2>&1 &
    echo $! > $LEDBLINK_PID
}

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
    led_blink_stop
    echo 0 > /sys/class/leds/green/brightness
    echo 1 > /sys/class/leds/red/brightness
    mark_ram "poweroff_instead: shutdown -h now (the stick is unmounted)"
    shutdown -h now
    sleep 120 # never back here: the launcher must not start again while the system goes down
    systemctl reboot
}

# CONSOLE-15 P2: suspend with a hub and a Wi-Fi dongle on the rear (OTG) port. musb refuses suspend-to-RAM
# ("trying to suspend as a_host while active", -16) unless its root port is itself suspended (otg state A_SUSPEND), and
# a hub whose child is active - the Wi-Fi dongle is power/control=on, wlan0 up - never suspends. So before the
# suspend the rear devices are made idle: wlan0 down, then the device(s) on the root port (the hub) deauthorized -
# unconfigured, its children gone, nothing left to keep it awake - and the bus is waited for until it reports
# runtime 'suspended'. After the wake they are authorized again, the hub enumerates its children afresh (the dongle
# is back at its own defaults) and wlan0 is brought up. The port's own host restart (swmode, below) stays the recovery
# for a bus the resume left dead. Everything is marked; nothing is written to the console's own storage.
rear_devs() { # the devices directly on the rear root port (1-1, not 1-1.4 or an interface)
    for d in /sys/bus/usb/devices/$REAR_NUM-*; do
        case "${d##*/}" in *.* | *:*) continue ;; esac
        [ -e "$d" ] && echo "$d"
    done
}
rear_kids() { # how many devices are below those (the hub's ports)
    k=0
    for d in /sys/bus/usb/devices/$REAR_NUM-*.*; do
        case "${d##*/}" in *:*) continue ;; esac
        [ -e "$d" ] && k=$((k + 1))
    done
    echo $k
}
rear_state() {
    s="bus=$(cat $REAR_BUS/power/runtime_status 2>/dev/null)"
    for d in /sys/bus/usb/devices/$REAR_NUM-*; do
        case "${d##*/}" in *:*) continue ;; esac
        [ -e "$d" ] && s="$s ${d##*/}=$(cat $d/power/runtime_status 2>/dev/null)/auth$(cat $d/authorized 2>/dev/null)/$(cat $d/power/control 2>/dev/null)"
    done
    echo "$s"
}
rear_quiesce() {
    ab_timeout ip link set wlan0 down > /dev/null 2>&1
    for d in $(rear_devs); do
        echo auto > $d/power/control 2> /dev/null
        echo 0 > $d/authorized 2> /dev/null
    done
    q=0
    while [ "$(cat $REAR_BUS/power/runtime_status 2>/dev/null)" != suspended ] && [ $q -lt 6 ]; do
        q=$((q + 1))
        sleep 1
    done
    mark "rear port quiesced after $q s: $(rear_state)"
}
rear_hostmode() { # the glue's own switch redoes the host bring-up (see the wake below)
    echo idle > $REAR_MODE # the first time a no-op (the switch only tracks its own writes)
    sleep 1
    echo host > $REAR_MODE
}
rear_wait_hub() { # $1 = seconds
    w=0
    while [ -z "$(rear_devs)" ] && [ $w -lt $1 ]; do
        w=$((w + 1))
        sleep 1
    done
}
rear_restore() {
    for d in $(rear_devs); do
        [ "$(cat $d/authorized 2>/dev/null)" = 0 ] && echo 1 > $d/authorized 2> /dev/null
    done
    w=0
    while [ "$(rear_kids)" -lt "$REAR_KIDS" ] && [ $w -lt 8 ]; do
        w=$((w + 1))
        sleep 1
    done
    mark_ram "rear port restored after $w s (devices below the hub: $(rear_kids) of $REAR_KIDS): $(rear_state)"
}
# wlan0 back up after the rear port came back; the services are touched only when it is not associated 5 s later
wifi_restore() {
    w=0
    while [ ! -e /sys/class/net/wlan0 ] && [ $w -lt 8 ]; do
        w=$((w + 1))
        sleep 1
    done
    ab_timeout ip link set wlan0 up > /dev/null 2>&1
    sleep 5
    if ab_timeout iw dev wlan0 link 2>&1 | grep -q 'Not connected'; then
        mark_ram "wlan0 not connected after the wake - restarting the Wi-Fi services: $(systemctl list-units --no-legend 'wpa_supplicant*' 'dhcpcd*' 'dhclient*' 2>/dev/null | awk '{print $1}' | tr '\n' ' ')"
        for u in $(systemctl list-units --no-legend 'wpa_supplicant*' 'dhcpcd*' 'dhclient*' 2>/dev/null | awk '{print $1}'); do
            ( systemctl restart "$u" > /dev/null 2>&1 & )
        done
    else
        mark_ram "wlan0 up: $(ab_timeout iw dev wlan0 link 2>&1 | head -1)"
    fi
}
# The rear port, Bluetooth and Wi-Fi after the wake, in the background while the launcher starts (the owner,
# 2026-10-01: the wake picture stayed ~15 s, 13 of them this). The stick is on the front bus and the launcher takes
# a pad whenever it shows up. Bluetooth is started again only once its adapter is back: bluetoothd kept running
# across the hub's deauthorization saw the adapter come back as a new device, and the first connection of a paired
# DualSense hung in its authentication for ~40 s ("paired" on the pad, nothing in the launcher) - a cold boot, where
# bluetoothd starts with the adapter there, never did. /tmp/.abrearwake is there while this runs; a standby waits for it.
rear_wake() {
    RW_FROM=$(wc -l < $SLOG)
    if [ "$REAR_HAD" = 1 ]; then
        sleep 3 # the USB bus re-enumerates after the resume
        mark_ram "rear port after the wake: $(rear_state)"
        rear_wait_hub 2
        if [ -z "$(rear_devs)" ] && [ -w $REAR_MODE ]; then
            mark_ram "rear USB: the hub did not come back - restarting the port's host mode"
            rear_hostmode
            rear_wait_hub 6
        fi
        rear_restore
        if [ "$(rear_kids)" -lt "$REAR_KIDS" ] && [ -w $REAR_MODE ]; then
            mark_ram "rear USB: only $(rear_kids) of $REAR_KIDS devices - restarting the port's host mode"
            rear_hostmode
            rear_wait_hub 6
            rear_restore
        fi
    fi
    rm -f /tmp/.abrearusb # the rear USB devices are back (or not coming): a stick on the hub is there by now
    if [ -n "$BT_UNITS" ]; then
        w=0
        while [ ! -e /sys/class/bluetooth/hci0 ] && [ $w -lt 8 ]; do
            w=$((w + 1))
            sleep 1
        done
        ab_timeout systemctl start $BT_UNITS > /dev/null 2>&1
        mark_ram "Bluetooth started again ($BT_UNITS) - adapter after $w s: $(ls /sys/class/bluetooth 2>/dev/null | tr '\n' ' ')"
    fi
    [ "$REAR_HAD" = 1 ] && wifi_restore
    # CONSOLE-15 evidence: the network after the wake, each call bounded
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
    tail -n +$((RW_FROM + 1)) $SLOG >> $LOG 2>/dev/null
    rm -f /tmp/.abrearwake
}

# whether anything is plugged in at the rear (micro-USB, OTG) port now: only the AutoBleem kernel runs it as a
# host, and a device there may not come back after the resume (see below)
# (the bus is found by its controller, musb-hdrc.0.auto - the front ports are musbfsh - not by its number)
rear_detect() {
    REAR_HAD=0
    REAR_DEV=""
    for u in /sys/bus/usb/devices/usb*; do
        case "$(readlink -f $u)" in
        */musb-hdrc.0.auto/usb*)
            REAR_NUM=${u##*/usb}
            REAR_BUS=$u
            REAR_DEV=/sys/bus/usb/devices/$REAR_NUM-1
            ;;
        esac
    done
    [ -n "$REAR_DEV" ] && [ -e "$REAR_DEV" ] && REAR_HAD=1
    REAR_MODE=/sys/devices/platform/mt_usb/swmode
    REAR_KIDS=0
    [ "$REAR_HAD" = 1 ] && REAR_KIDS=$(rear_kids)
}

# CONSOLE-15 P2: a hub with a Wi-Fi dongle on the rear port keeps its root port from suspending, and the kernel
# then refuses the suspend (see rear_quiesce above): the rear devices are made idle first - after the stick's umount, which
# may be on that hub. A real suspend is tried either way; poweroff_instead only after it failed. Bluetooth stopped for the suspend and started again by rear_wake once its adapter is back (see
# there). The kernel overlay's start script binds its key store over /var/lib/bluetooth again on every start - the
# same directory, so the extra layer changes nothing and is left alone (the pairings live there)
quiesce_for_suspend() {
    BT_UNITS=""
    for u in bluetooth abbtagent; do
        systemctl -q is-active $u.service 2>/dev/null && BT_UNITS="$BT_UNITS $u.service"
    done
    if [ -n "$BT_UNITS" ]; then
        ab_timeout systemctl stop $BT_UNITS > /dev/null 2>&1
        mark "Bluetooth stopped for the suspend:$BT_UNITS"
    fi
    if [ "$REAR_HAD" = 1 ]; then
        mark "rear USB: $REAR_KIDS device(s) below the hub: $(rear_state) - quiescing"
        rear_quiesce
    fi
}

# the AutoBleem picture from the wake until the launcher's window is up (it removes /tmp/.abload itself, as after
# RetroArch) - the seconds of the bus, the mount and the launcher's start were black - and the LED blinking as at a
# first boot meanwhile (see led_blink_start; only with the picture up - without absplash nothing holds .abload)
wake_picture() {
    if [ -x /tmp/absplash ] && [ -f /tmp/autobleem.jpg ]; then
        touch /tmp/.abload
        LD_LIBRARY_PATH=/tmp/lib /tmp/absplash /tmp/autobleem.jpg --anim sweep --until-gone /tmp/.abload --timeout 40 > /tmp/absplash.log 2>&1 &
        echo $! > /tmp/.absplash.pid # boot.sh shows no second picture over this one
    else
        echo "no absplash on tmpfs" >> $SLOG
    fi
    [ -f /tmp/.abload ] && led_blink_start
}

# the "please wait - powering off" picture over Weston's bare desktop until the suspend blanks the screen (the owner,
# 2026-10-01: not black); removed by the wake (suspend_wake). RAM only.
down_picture() {
    if [ -x /tmp/absplash ]; then
        touch /tmp/.abdown
        DOWNPIC=/tmp/poweroff.jpg; [ -f $DOWNPIC ] || DOWNPIC=/tmp/autobleem.jpg
        LD_LIBRARY_PATH=/tmp/lib /tmp/absplash $DOWNPIC --until-gone /tmp/.abdown --timeout 300 > /dev/null 2>&1 &
    fi
}

# the stick: any sd?1 with the SONY label, not only sda1/sdb1 as usb_watch looks - a stick pulled during the standby
# and plugged back can come back under a new name while the kernel still holds the old one, and the wake then found
# nothing and rebooted (2026-09-25)
find_stick() {
    blkid | grep "^/dev/sd[a-z]1:" | grep -E "LABEL=\"SONY.{0,4}\"" | awk -F: '{print $1}' | head -1
}

# AUTOBLEEM-28: no stick - the insert-stick picture (the UI language's, boot.sh's tmpfs copy) until a stick is in
# any port, polled every second, for at most NOSTICK_WAIT s; POWER on the picture (absplash --power-file) ends the
# wait at once. $1 = check: the stick was pulled while mounted, so it is checked before the mount (rc/ab_fsck.sh -
# the check-stick picture with its bar goes over this one). Returns 0 with the stick mounted on /media, 1 when the
# console should sleep (the time is up, or POWER).
NOSTICK_WAIT=300

# AUTOBLEEM-29: a stick about to be mounted after a wake or a wait is checked (rc/ab_fsck.sh, the check-stick picture
# with its bar) when it needs it: a FAT whose flag says it was left mounted - pulled, or dirty from elsewhere: the
# kernel never clears a flag it finds set at mount, so it would stay dirty for good - and, with $2 = check (pulled
# while mounted), any exFAT/ext too. STICK_CHECKED=1 when it ran (stick_mounted keeps its log).
STICK_CHECKED=0
stick_check() {
    [ -f /tmp/ab_fsck.sh ] || return 0
    . /tmp/ab_fsck.sh
    case "$(ab_fsck_type "$1")" in
    vfat | msdos | fat)
        [ -x /tmp/abfatflag ] && /tmp/abfatflag "$1" 2> /dev/null | grep -q '^dirty' || return 0
        ;;
    *)
        [ "$2" = check ] || return 0
        ;;
    esac
    rm -f $ABFSCK_LOG
    ab_fsck "$1"
    STICK_CHECKED=1
}

wait_for_stick() {
    want_check=$1
    rm -f /tmp/.abpower
    touch /tmp/.abnostick
    if [ -x /tmp/absplash ] && [ -f /tmp/insert-stick.jpg ]; then
        LD_LIBRARY_PATH=/tmp/lib /tmp/absplash /tmp/insert-stick.jpg --until-gone /tmp/.abnostick \
            --timeout $((NOSTICK_WAIT + 60)) --power-file /tmp/.abpower > /tmp/absplash-nostick.log 2>&1 &
    fi
    echo "$(date) no stick - the insert-stick picture, waiting up to $NOSTICK_WAIT s (check: ${want_check:-no})" >> $SLOG
    w=0
    seen=""    # the device found last time round (a new one gets a second to settle)
    checked="" # the device checked already (a mount that fails is tried again every second, its check once)
    while [ $w -lt $NOSTICK_WAIT ] && [ ! -f /tmp/.abpower ]; do
        DEV=$(find_stick)
        [ -z "$DEV" ] && { seen=""; checked=""; }
        if [ -n "$DEV" ]; then
            if [ "$DEV" != "$seen" ]; then
                seen=$DEV
                sleep 1
            fi
            if [ "$DEV" != "$checked" ]; then
                checked=$DEV
                stick_check "$DEV" "$want_check"
            fi
            if mount "$DEV" /media; then
                rm -f /tmp/.abnostick
                echo "$(date) stick $DEV mounted after $w s" >> $SLOG
                return 0
            fi
            [ "$DEV" = "$unmountable" ] || echo "$(date) $DEV did not mount - still waiting" >> $SLOG
            unmountable=$DEV
        fi
        w=$((w + 1))
        sleep 1
    done
    rm -f /tmp/.abnostick
    if [ -f /tmp/.abpower ]; then
        echo "$(date) POWER on the insert-stick picture after $w s - the standby" >> $SLOG
    else
        echo "$(date) no stick within $NOSTICK_WAIT s - the standby" >> $SLOG
    fi
    rm -f /tmp/.abpower
    return 1
}

# the stick is mounted again after a wake or a wait: the kernel owns its flag (clean at mount, or not ours), the
# session marker is back, and a later boot is not 'after a power-off request'
stick_mounted() {
    rm -f /tmp/ab_stick_owned
    if [ "$STICK_CHECKED" = 1 ] && [ -f /tmp/abfsck.log ]; then
        mkdir -p /media/System/Logs
        cat /tmp/abfsck.log >> /media/System/Logs/fsck.log
    fi
    touch /media/System/.session
    rm -f /media/System/Logs/poweroff_reason
    { cat $SLOG; [ -f /tmp/absplash.log ] && sed 's/^/  absplash: /' /tmp/absplash.log; } >> $LOG
}

# the suspend once more with no stick in (AUTOBLEEM-28: nobody put one in, or POWER on the insert-stick picture):
# the last wake's background part ends first, then the rear port and Bluetooth go idle as for the first suspend
sleep_again() {
    w=0
    while [ -f /tmp/.abrearwake ] && [ $w -lt 25 ]; do
        w=$((w + 1))
        sleep 1
    done
    rm -f /tmp/.abrearwake
    led_blink_stop
    rm -f /tmp/.abload
    down_picture
    rear_detect
    STICK_REAR=0 # no stick in: the insert-stick picture after 5 s
    mark "the standby again with no stick (rear_had=$REAR_HAD)"
    quiesce_for_suspend
}

# after a wake: the stick within 5 s (the wake picture up meanwhile) - or, when it was on the rear hub before the
# standby (STICK_REAR), until the rear port's USB devices are back (rear_wake's first part, .abrearusb - not its
# Wi-Fi and Bluetooth; at most 15 s: the stick returns only with the port) - else the insert-stick wait. 0 = mounted.
STICK_REAR=0
stick_after_wake() {
    i=0
    woke_checked=""
    while [ $i -lt 5 ] || { [ "$STICK_REAR" = 1 ] && [ -f /tmp/.abrearusb ] && [ $i -lt 15 ]; }; do
        DEV=$(find_stick)
        [ $i = 0 ] && { echo "$(date) after the wake:"; blkid | grep '^/dev/sd' | sed 's/^/  /'; } >> $SLOG
        if [ -n "$DEV" ] && [ "$DEV" != "$woke_checked" ]; then
            woke_checked=$DEV
            stick_check "$DEV"
        fi
        if [ -n "$DEV" ] && mount "$DEV" /media; then
            echo "$(date) mounted $DEV after $i s" >> $SLOG
            return 0
        fi
        DEV=""
        i=$((i + 1))
        sleep 1
    done
    # not back: the wake picture and its blink end, the insert-stick picture takes over
    led_blink_stop
    echo 1 > /sys/class/leds/green/brightness
    echo 0 > /sys/class/leds/red/brightness
    rm -f /tmp/.abload
    wait_for_stick || return 1
    wake_picture
    return 0
}

# The console's "power off", the way Sony's own power_manage does it - suspend to RAM, the power button
# wakes it - but with the stick unmounted first, so it can be pulled while the console is "off" without
# coming back dirty (Windows' "scan and fix"). The orange LED (red + green) is the AutoBleem standby; the USB bus is
# reset by the resume and the stick comes back a few seconds later (the same name as before, or the
# other one), and is mounted again exactly as usb_watch mounted it. Nothing here touches the console's
# own storage. A wake with no stick (AUTOBLEEM-28) shows the insert-stick picture and sleeps again after 5 minutes,
# or at once on POWER (stick_after_wake, sleep_again). Returns 0 with the stick mounted again, 1 only when /media
# could not be unmounted (the reboot follows).
standby() {
    ab_watch_stop # the watch log (CONSOLE-15) must not touch /media or run through the suspend
    # the last wake's background part (rear_wake) still running - a power off right after a wake - ends first
    w=0
    while [ -f /tmp/.abrearwake ] && [ $w -lt 25 ]; do
        w=$((w + 1))
        sleep 1
    done
    rm -f /tmp/.abrearwake
    DEV=$(awk '$2 == "/media" { print $1 }' /proc/mounts | head -1)
    SLOG=/tmp/standby.log
    echo "$(date) standby: dev=$DEV" > $SLOG
    # the launcher has left and Weston shows its bare desktop - black with the mouse cursor - until the suspend
    # blanks the screen. A full-screen absplash with the power-off picture covers it (the owner, 2026-10-01: "please
    # wait - powering off", not black; the wake picture if it is missing, black if both are), from here until the
    # wake; the wake picture goes over it and the flag is removed there. RAM only, the stick is not touched.
    down_picture
    rm -f /media/System/.session # the session ended cleanly - checkstick.sh may clear the flag next boot
    sync
    # CONSOLE-15 P2: the state BEFORE the dangerous step, on the stick (the umount below is the last time it can be
    # written; a reset in the suspend leaves no other trace). Read-only on /sys and /proc; the marker is read and
    # deleted by boot.sh at the next start. Only a power off writes this - the quiet-stick rule holds.
    rear_detect
    # the stick itself on the rear (OTG) port's hub: after the wake it comes back only with that port (stick_after_wake)
    case "$(readlink -f /sys/class/block/${DEV##*/} 2>/dev/null)" in */musb-hdrc.0.auto/*) STICK_REAR=1 ;; esac
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
    # the rear port idle only now: a stick on the rear hub goes with it, and quiesced before the umount it was gone
    # when the kernel came to mark the volume clean ("unable to read boot sector") - every such stick came back
    # "not properly unmounted" (2026-10-13)
    quiesce_for_suspend

    # AUTOBLEEM-28: asleep until the stick is back - a wake with no stick in shows the insert-stick picture (5 minutes;
    # POWER on it = at once) and sleeps again, where it used to reboot after 60 s
    until suspend_wake && stick_after_wake; do
        sleep_again
    done
    stick_mounted
    return 0
}

# One suspend to RAM and the wake from it (the stick is not mounted: standby() unmounted it, or it is not there),
# up to the moment the stick may be looked for. Always returns 0 - a refused suspend ends in poweroff_instead.
suspend_wake() {
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

    led_blink_stop
    echo 1 > /sys/class/leds/green/brightness # orange (red + green) = the standby, as the wake blink's orange
    echo 1 > /sys/class/leds/red/brightness
    sync
    mark "LED orange set, echo mem next"
    # a refused suspend fails the write (EBUSY, a wakeup source held); a real one returns after the wake.
    # Refused: what held it goes to the log, and two more tries for a passing wakelock.
    try=1
    until mark "echo mem try $try starts" && echo mem > /sys/power/state 2>> $SLOG; do
        echo "$(date) the kernel refused to suspend (try $try) - wakelocks: $(cat /sys/power/wake_lock 2>/dev/null)" >> $SLOG
        dmesg | tail -60 | grep -iE 'failed to suspend|while active|early wake|abort|wakeup pending' >> $SLOG
        if [ $try -ge 3 ]; then
            mark_ram "echo mem refused $try times - no real suspend possible, falling back to the power off; rear: $(rear_state 2>/dev/null) otg: $(dmesg | tail -80 | grep -iE 'musb|a_host|otg' | tail -3 | tr '\n' '|')"
            poweroff_instead
        fi
        try=$((try + 1))
        sleep 2
    done
    echo 1 > /sys/class/leds/green/brightness
    echo 0 > /sys/class/leds/red/brightness
    mark_ram "echo mem returned (try $try) - the console woke"
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

    echo "$(date) resumed" >> $SLOG
    wake_picture
    rm -f /tmp/.abdown # the black cover of the power down is done (the wake picture is on top of it)

    # The rear (OTG) port on the AutoBleem kernel: MediaTek's musb driver does not restart its host session after
    # a resume, so a hub or dongle there stays gone - WiFi and Bluetooth dead after every wake (2026-09-26: the
    # front bus came back in 2 s, the rear hub never did). Its glue's own switch redoes the host bring-up
    # (musb_id_pin_sw_work: VBUS, session, PHY) and the hub re-enumerates within 2 s; unbinding the driver instead
    # leaves the port dead (its probe cannot run twice). Only when something was there before the standby and is
    # still missing - the stock kernel, an empty rear port or a device that came back: nothing, no wait.
    # CONSOLE-15 P2: the rear devices were deauthorized for the suspend (rear_quiesce): when the hub is there after the
    # wake it is authorized again and its children enumerate afresh; when the bus is dead the host restart comes first.
    # All of it, Bluetooth and Wi-Fi with it, in the background (rear_wake) - the stick and the launcher do not wait.
    if [ "$REAR_HAD" = 1 ] || [ -n "$BT_UNITS" ]; then
        touch /tmp/.abrearwake /tmp/.abrearusb
        ( rear_wake ) < /dev/null > /dev/null 2>&1 &
    fi

    sleep 3 # the USB bus re-enumerates after the resume
    return 0
}

# AUTOBLEEM-28: the stick was pulled while the launcher, a game or this script's own work ran - ab_stickwatch.sh
# stopped and killed all of it, let the dead mount go and left selection 9. The insert-stick picture until the stick
# is back in any port; it is checked (pulled while mounted), mounted, and boot.sh's loop starts the launcher again.
# Nobody within 5 minutes, or POWER: the standby, the same wait after every wake.
nostick() {
    SLOG=/tmp/standby.log
    echo "$(date) no stick: pulled while mounted (stickwatch: $(tail -1 $AB_RUNTIME_DIR/stickwatch.log 2>/dev/null))" > $SLOG
    DEV=""
    led_blink_stop
    if ! wait_for_stick check; then
        sleep_again
        until suspend_wake && stick_after_wake; do
            sleep_again
        done
    fi
    stick_mounted
    [ -f /tmp/.abload ] || wake_picture
    return 0
}

# The update the launcher downloaded: abupdate (the PC installer's own code, autobleem-core's InstallerJob)
# replaces what the package ships - Autobleem/bin, the rc scripts, the console tools, the shipped themes -
# and keeps everything of the user's. It runs from tmpfs because Autobleem/bin/autobleem is what it
# replaces; the AutoBleem picture is on the screen meanwhile. Either way the launcher comes back: the new
# one, or - on a failure, which update.log explains - the old one with System/Updates kept.
# (RetroArch's own install/remove is not here: the launcher runs `abupdate ROOT --retroarch install|remove ...`
# itself, from its own folder - see src/tools/abupdate.cpp.)
update() {
    ULOG=/media/System/Logs/update.log
    cp -f /media/Autobleem/bin/autobleem/abupdate /tmp/abupdate 2>/dev/null && chmod +x /tmp/abupdate
    # abupdate downloads through abfetch beside it (ownDir()), with abfetch's cacert.pem beside that: the same
    # copy, or UpdateRoms is skipped with "/tmp/abfetch: No such file"
    cp -f /media/Autobleem/bin/autobleem/abfetch /tmp/abfetch 2>/dev/null && chmod +x /tmp/abfetch
    cp -f /media/Autobleem/bin/autobleem/cacert.pem /tmp/cacert.pem 2>/dev/null
    if [ ! -x /tmp/abupdate ]; then
        echo "$(date) no abupdate on the stick - the update is not installed" >> $ULOG
        return
    fi
    if [ -x /tmp/absplash ] && [ -f /tmp/autobleem.jpg ]; then
        touch /tmp/.abupdating
        # the "updating, do not switch off" picture (UIREV-47), the plain one when the stick has none
        UPIC=/tmp/updating.jpg
        [ -f $UPIC ] || UPIC=/tmp/autobleem.jpg
        LD_LIBRARY_PATH=/tmp/lib /tmp/absplash $UPIC --until-gone /tmp/.abupdating --timeout 900 > /dev/null 2>&1 &
    fi
    echo "$(date) installing the downloaded update" >> $ULOG
    cd /tmp
    LD_LIBRARY_PATH=/tmp/lib /tmp/abupdate /media >> $ULOG 2>&1
    st=$? # before $(date) below: that sets $? itself
    echo "$(date) abupdate exit status $st" >> $ULOG
    sync
    rm -f /tmp/.abupdating /tmp/abupdate /tmp/abfetch /tmp/cacert.pem
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
    # /media busy: the reboot brings whatever is plugged in back up
    ;;
"$SEL_NOSTICK")
    nostick
    exit 0
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
