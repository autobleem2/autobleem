# Experiment (2026-09-22): a soft power-off the way Sony's power_manage does it, but with the stick
# unmounted - umount /media, red LED, echo mem > /sys/power/state, and after the power button wakes the
# console: wait for the stick to come back, mount it, carry on into the launcher. SOURCED from boot.sh
# (". ./psc_sleep_test.sh") so the cd moves the shell that holds /media as its cwd. Nothing is written
# to the console's own storage; the log is built in /tmp and copied to System/Logs at the end.

OUT=/tmp/sleep_test.txt
RC_DIR=/media/Autobleem/rc
_t0=$(date +%s)
_log() { echo "[+$(( $(date +%s) - _t0 ))s] $*" >> $OUT; }

: > $OUT
_log "start: cwd=$(pwd) pid=$$"
_log "leds: green=$(cat /sys/class/leds/green/brightness) red=$(cat /sys/class/leds/red/brightness)"
_log "wake_lock: '$(cat /sys/power/wake_lock 2>&1)'  power/disable=$(cat /data/power/disable) resume_count=$(cat /data/power/resume_count) usbreset_count=$(cat /data/power/usbreset_count)"
_log "input devices before: $(grep '^N:' /proc/bus/input/devices | tr '\n' ' ')"
_log "dev before: $(ls /dev/sd* 2>/dev/null | tr '\n' ' ')"

cd /tmp
umount /etc/udev/rules.d/20-joystick.rules 2>>$OUT && _log "bind umount ok" || _log "bind umount FAILED"
sync
if umount /media 2>>$OUT; then
    _log "umount /media ok; byte 0x41 = $(dd if=/dev/sda1 bs=1 skip=65 count=1 2>/dev/null | od -A n -t x1)"
    _dmesg_mark=$(dmesg | wc -l)

    echo 0 > /sys/class/leds/green/brightness
    echo 1 > /sys/class/leds/red/brightness
    sync
    _log "suspending"
    echo mem > /sys/power/state 2>>$OUT && _log "resumed: echo mem returned 0" || _log "echo mem FAILED (rc=$?)"
    echo 1 > /sys/class/leds/green/brightness
    echo 0 > /sys/class/leds/red/brightness

    _log "after resume: resume_count=$(cat /data/power/resume_count) usbreset_count=$(cat /data/power/usbreset_count)"
    # watch the USB bus settle for a few seconds before touching the stick (power_manage may send a usb reset)
    for i in 1 2 3 4 5 6 7 8; do
        sleep 1
        _log "t+${i}s dev: $(ls /dev/sd* 2>/dev/null | tr '\n' ' ') usbreset_count=$(cat /data/power/usbreset_count)"
    done
    _log "dmesg since suspend:"; dmesg | tail -n +$_dmesg_mark | grep -i "PM:\|suspend\|resume\|usb\|sd[a-z]\|fat\|reset" >> $OUT
    _log "input devices after: $(grep '^N:' /proc/bus/input/devices | tr '\n' ' ')"

    _dev=""
    for i in $(seq 1 30); do
        _dev="$(blkid | grep "^/dev/sd\(a\|b\)1:" | grep -E "LABEL=\"SONY.{0,4}\"" | awk -F: '{print $1}' | head -1)"
        [ -n "$_dev" ] && break
        sleep 1
    done
    _log "stick device: '$_dev' after $i s"
    if [ -n "$_dev" ]; then
        _log "byte 0x41 before mount = $(dd if=$_dev bs=1 skip=65 count=1 2>/dev/null | od -A n -t x1)"
        mount "$_dev" /media 2>>$OUT && _log "mounted: $(grep ' /media ' /proc/mounts)" || _log "mount FAILED"
    else
        _log "no stick after resume - giving up, the boot will fail below"
    fi
else
    _log "umount /media FAILED - holders:"
    for d in /proc/[0-9]*; do
        p=${d#/proc/}
        case "$(readlink $d/cwd 2>/dev/null)" in /media*) echo "  $p cwd $(cat $d/comm) $(readlink $d/cwd)" >> $OUT;; esac
        ls -l $d/fd 2>/dev/null | grep /media | sed "s|^|  $p fd $(cat $d/comm 2>/dev/null) |" >> $OUT
    done
fi

cd $RC_DIR
mount -o bind $RC_DIR/20-joystick.rules /etc/udev/rules.d/20-joystick.rules 2>>$OUT && _log "bind mount restored"
_log "end: cwd=$(pwd)"
mkdir -p /media/System/Logs && cp $OUT /media/System/Logs/sleep_test.txt; sync
