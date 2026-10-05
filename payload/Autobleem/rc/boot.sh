#!/bin/sh

# Sourced (through Autobleem/start.sh) into the shell Sony's usb_watch runs our payload with, so the cd's
# here move that shell: its cwd is the one thing of Sony's that ever holds /media, and it has to be off the
# stick while selection.sh unmounts it for the standby (see selection.sh). Everything else is reached by
# absolute path for the same reason.

RC=/media/Autobleem/rc

# Sony's UI first, at once (the stock logo held the screen until killsony.sh, far below), and again after a
# second for the late forks (killsony.sh without "now" sleeps one) - in the background, the boot goes on
$RC/killsony.sh now
$RC/killsony.sh > /dev/null 2>&1 &

# bt STEP: one line in the boot log (RAM, like the others) with the seconds since the kernel started - where the
# boot's time goes. Until ab_log.sh has said where the log directory is, the lines wait in /tmp/boot-steps.early.
BT_FILE=/tmp/boot-steps.early
bt() {
    echo "$(cut -d' ' -f1 /proc/uptime) $*" >> "$BT_FILE"
}
bt "boot.sh started, Sony's UI killed"

# OUR picture from the first moment until the launcher's window is up (the launcher removes /tmp/.abload itself,
# as after a wake or RetroArch): absplash and its pictures are copied to tmpfs - the stick is unmounted at the
# standby, a binary running from it would hold it - and the libraries it needs are unpacked to /tmp/lib (RAM) now
# instead of by autobleem.sh much later. Run with the sweep along the picture's rule (--anim) and never for long:
# the launcher's own start ends it, the timeout only the case it never comes.
sh $RC/unpack_libs.sh
bt "libraries unpacked to /tmp/lib"
cp -f /media/Autobleem/bin/autobleem/absplash /tmp/absplash && chmod +x /tmp/absplash
# the launcher's picture on tmpfs (/tmp/autobleem.jpg, what every absplash call here and in selection.sh shows):
# the 4:3 one (720x480, already stretched for the 480p output) while Weston runs the CRT 4:3 mode, else the 16:9 one
pick_splash() {
    pic=autobleem.jpg
    if [ "$(cat /tmp/weston.mode 2>/dev/null)" = 720x480 ] &&
        [ -f /media/Autobleem/bin/autobleem/splash/autobleem-4x3.jpg ]; then
        pic=autobleem-4x3.jpg
    fi
    cp -f /media/Autobleem/bin/autobleem/splash/$pic /tmp/autobleem.jpg
}
pick_splash
SPLASH_PID=/tmp/.absplash.pid
show_splash() {
    # already up (the wake's or the return's picture): nothing to start - one on the screen at a time
    if [ -s $SPLASH_PID ] && kill -0 "$(cat $SPLASH_PID)" 2>/dev/null; then
        return
    fi
    [ -x /tmp/absplash ] && [ -f /tmp/autobleem.jpg ] || return
    touch /tmp/.abload
    LD_LIBRARY_PATH=/tmp/lib /tmp/absplash /tmp/autobleem.jpg --anim sweep --until-gone /tmp/.abload --timeout 90         > /tmp/absplash.log 2>&1 &
    echo $! > $SPLASH_PID
    bt "absplash started (pid $!)"
}
show_splash

# where this run's logs go (RAM unless kept on the stick) - exported to everything below, the launcher too
. $RC/ab_log.sh
cat /tmp/boot-steps.early >> "$AB_LOG_DIR/boot.log" 2>/dev/null && rm -f /tmp/boot-steps.early
BT_FILE=$AB_LOG_DIR/boot.log
bt "log directory $AB_LOG_DIR"

# CONSOLE-15 P2: selection.sh's standby() leaves System/Logs/poweroff_reason before it unmounts the stick and
# deletes it when the standby ended in a wake. Found here, the console was reset or powered off in between: say so
# in standby.log, with Sony's power log if it is there, once.
POR=/media/System/Logs/poweroff_reason
if [ -f "$POR" ]; then
    {
        echo "$(date) boot after a power-off request: $(cat $POR) - now uptime=$(cut -d' ' -f1 /proc/uptime)"
        [ -f /tmp/power.log ] && { echo "-- /tmp/power.log"; cat /tmp/power.log; }
        echo
    } >> /media/System/Logs/standby.log
    rm -f "$POR"
fi

# Nothing of ours in /tmp is aged out. The console boots with its clock at 2018-09-01; on a network (the
# AutoBleem kernel's WiFi) timesyncd jumps it to today, and systemd-tmpfiles-clean.timer (15 min after boot,
# then daily) then finds everything made at boot eight years old - /usr/lib/tmpfiles.d/tmp.conf ages /tmp
# at 10 days - and deletes it: /tmp/lib's soname links went (the Apps fell back to the firmware's SDL 2.0.4),
# the libs archive, the udev rules file. /tmp is RAM, emptied by every reboot anyway. /run/tmpfiles.d is
# tmpfs too - nothing on the console's own storage is written; an x line keeps a path and all under it.
mkdir -p /run/tmpfiles.d
echo 'x /tmp/*' > /run/tmpfiles.d/autobleem.conf

# USB gamepad fix - the rules file from tmpfs: it survives the standby (the stick is unmounted then) and
# holds nothing on the stick
cp -f $RC/20-joystick.rules /tmp/20-joystick.rules
mount -o bind /tmp/20-joystick.rules /etc/udev/rules.d/20-joystick.rules
udevadm control --reload-rules
udevadm trigger
bt "udev rules reloaded and triggered"
# no mouse cursor from a pad's touchpad: its seat rule into /run, a pad already connected announced again
sh $RC/pad_seat.sh
bt "pad touchpad seat rule in place"

# kernel modules the stick carries beyond the firmware's (xpad.ko for Xbox pads - the site's libs pack,
# unpacked by the installer into Autobleem/lib/modules) - only for a kernel without its own: the AutoBleem
# kernel from psc-kernel-payload 2026-09-25 on builds a newer xpad (and more) as modules, udev loads them,
# and the stick's 2020 build would take the pad from it - built for another kernel, at best refused
KMODDIR=/lib/modules/$(uname -r)
for kmod in /media/Autobleem/lib/modules/*.ko; do
    [ -f "$kmod" ] || continue
    name=$(basename "$kmod")
    if grep -qs "/$name" "$KMODDIR/modules.dep" "$KMODDIR/modules.builtin"; then
        echo "boot: $name - the kernel has its own"
        continue
    fi
    insmod "$kmod"
done
bt "kernel modules done"

# a third time, the udev work and the modules gave Sony's late starters time to come up (no sleep this time)
$RC/killsony.sh now
$RC/backup.sh
bt "backup.sh done"
# the stick's dirty flag, before anything of ours writes to it
$RC/checkstick.sh
bt "checkstick.sh done"
# the AutoBleem kernel's SSH key from the stick, if any (C10) - a no-op on the stock kernel
$RC/ssh_keys.sh
bt "ssh_keys.sh done"

# Options -> Display: Weston's output mode, 720p (the firmware's), 1080p or 720x480 ("CRT 4:3": 480p, for a CRT
# behind an HDMI converter). The console's HDMI driver reads no EDID, so a client cannot switch modes: Weston is
# restarted with a CEA modeline (1080p60, 480p60) from a RAM copy of its
# ini, bind-mounted over it - nothing on the console's own storage is written, and a reboot is the firmware's
# 720p again. Run before every start of the launcher (the boot, after a standby, after the launcher left for a
# new mode - AB_SELECTION 8), and only when the wanted mode differs from the running one (~3 s of black). The
# wanted mode: the launcher's pending one (outputmode.pending in the runtime dir - being tried, the launcher asks
# to keep it), else config.ini's outputmode; anything but 1080 and 720x480 is 720. The copy is rewritten in place (cat >),
# never replaced: the bind mount holds its inode.
WESTON_INI=/etc/xdg/weston/weston.ini
apply_output_mode() {
    want=$(cat "$AB_RUNTIME_DIR/outputmode.pending" 2>/dev/null | tr -d '\r' | head -1)
    [ -n "$want" ] || want=$(sed -n 's/^outputmode=//p' /media/System/config.ini 2>/dev/null | tr -d '\r' | tail -1)
    case "$want" in 1080 | 720x480) ;; *) want=720 ;; esac
    have=$(cat /tmp/weston.mode 2>/dev/null)
    [ -n "$have" ] || have=720
    [ "$want" = "$have" ] && return
    if ! grep -qs " $WESTON_INI " /proc/mounts; then
        cp -f $WESTON_INI /tmp/weston.orig.ini
        cp -f /tmp/weston.orig.ini /tmp/weston.ini
        mount -o bind /tmp/weston.ini $WESTON_INI
    fi
    case "$want" in
        1080) modeline='148.50 1920 2008 2052 2200 1080 1084 1089 1125 +hsync +vsync' ;;
        720x480) modeline='27.00 720 736 798 858 480 489 495 525 -hsync -vsync' ;; # CEA 480p60, VIC 2/3
        *) modeline= ;;
    esac
    if [ -n "$modeline" ]; then
        sed "s/^mode=1280x720\$/mode=$modeline/" /tmp/weston.orig.ini > /tmp/weston.ini
    else
        cat /tmp/weston.orig.ini > /tmp/weston.ini
    fi
    echo "$(date) boot: Weston restarted in ${want} (was ${have})" >> "$AB_LOG_DIR/display.log"
    # the picture's window dies with the compositor: stopped (our own pid), and shown again once Weston is back
    [ -s $SPLASH_PID ] && kill "$(cat $SPLASH_PID)" 2>/dev/null
    rm -f $SPLASH_PID
    bt "Weston restart to ${want}"
    systemctl restart weston
    sleep 3
    echo "$want" > /tmp/weston.mode
    pick_splash # the picture of the new mode
    show_splash
    bt "Weston back in ${want}"
}

# The launcher, and after it whatever it asked for: selection.sh comes back (exit 0) after a standby or a
# RetroArch session and the launcher is started over; a reboot never returns. It runs from a copy on
# tmpfs with the cwd there, so nothing of ours is on the stick while it unmounts it - the copy is taken
# every time round, so a stick updated during a standby is what runs at the next power off; the wake-up
# picture (absplash, shown from the resume until the launcher's window is up) goes to tmpfs with it, the
# stick not being mounted at the wake.
while true; do
    cd $RC
    bt "loop: launcher round starts"
    show_splash   # not up (the launcher left for a new display mode, the first try failed): up now
    apply_output_mode
    # CONSOLE-15 evidence: the watch log (RAM, one line per 30 s - see ab_watch.sh); it runs from a tmpfs copy
    # and is stopped by selection.sh before the stick is unmounted, so it is started again every time round
    sh $RC/ab_watch.sh start
    rm -f "$AB_RUNTIME_DIR/autobleem_exit"
    bt "starting autobleem.sh"
    ./autobleem.sh
    ab_sh_rc=$?
    bt "autobleem.sh ended (status $ab_sh_rc)"
    # autobleem.sh writes the launcher's own status; if it was killed itself, its status is the next best
    [ -f "$AB_RUNTIME_DIR/autobleem_exit" ] || echo $ab_sh_rc > "$AB_RUNTIME_DIR/autobleem_exit"
    cp -f $RC/selection.sh /tmp/selection.sh
    cp -f /media/Autobleem/bin/autobleem/absplash /tmp/absplash && chmod +x /tmp/absplash
    pick_splash
    cp -f /media/Autobleem/bin/autobleem/splash/updating.jpg /tmp/updating.jpg
    cp -f /media/Autobleem/bin/autobleem/splash/poweroff.jpg /tmp/poweroff.jpg
    cd /tmp
    sh /tmp/selection.sh || break
done
sync
systemctl reboot   # systemd's: a busybox reboot over it only signals init and returns
