#!/bin/sh

# Sourced (through Autobleem/start.sh) into the shell Sony's usb_watch runs our payload with, so the cd's
# here move that shell: its cwd is the one thing of Sony's that ever holds /media, and it has to be off the
# stick while selection.sh unmounts it for the standby (see selection.sh). Everything else is reached by
# absolute path for the same reason.

RC=/media/Autobleem/rc

# What this shell loaded: the loop below runs from the shell's memory, so an update that replaces this file on the
# stick (selection.sh's abupdate, between two rounds) would leave the OLD loop running until a reboot - the old
# apply_output_mode, the old picture rules. The loop compares this with the stick's file at the top of every round
# and starts the new one in the same process (see "restart"); AB_BOOT_RESTARTED says this run is that restart.
BOOT_SUM=$(cksum < $RC/boot.sh 2> /dev/null)

# bt STEP: one line in the boot log (RAM, like the others) with the seconds since the kernel started - where the
# boot's time goes. Until ab_log.sh has said where the log directory is, the lines wait in /tmp/boot-steps.early.
BT_FILE=/tmp/boot-steps.early
bt() {
    echo "$(cut -d' ' -f1 /proc/uptime) $*" >> "$BT_FILE"
}

# the pictures on tmpfs, each with its 4:3 twin (<name>-4x3.jpg, 720x480 for the CRT mode) when the package has one:
# absplash itself shows the twin when the window it opens is 720x480, so no script has to know the mode
copy_pictures() {
    for pic in "$@"; do
        cp -f /media/Autobleem/bin/autobleem/splash/$pic.jpg /tmp/$pic.jpg
        cp -f /media/Autobleem/bin/autobleem/splash/$pic-4x3.jpg /tmp/$pic-4x3.jpg 2>/dev/null
    done
}
# AUTOBLEEM-28/29: the insert-stick and check-stick pictures in the launcher's language (config.ini's language: the
# language file's name, Polski ...; English when there is none for it) on tmpfs as insert-stick.jpg / check-stick.jpg
# with their 4:3 twins - they are shown when the stick is gone or being checked, so they must already be in RAM.
# Taken again every round: the language may have changed in Options.
copy_stick_pictures() {
    sp_lang=$(ab_config_get language)
    sp_dir=/media/Autobleem/bin/autobleem/splash
    for pic in insert-stick check-stick; do
        l=$sp_lang
        [ -n "$l" ] && [ -f "$sp_dir/$pic/$l.jpg" ] || l=English
        cp -f "$sp_dir/$pic/$l.jpg" /tmp/$pic.jpg 2>/dev/null
        cp -f "$sp_dir/$pic/$l-4x3.jpg" /tmp/$pic-4x3.jpg 2>/dev/null
    done
}
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

# Self-repair of the exec bits: this file is sourced, so it runs without +x, but everything below it is executed
# directly. A stick whose filesystem keeps the Unix mode (ext4) and was unpacked by an older updater has lost
# them, and the console would sit in a boot loop. Only files on the stick (/media) that exist and are not
# executable are touched; the console's own storage is never written.
fix_exec_bits() {
    for f in /media/Autobleem/rc/*.sh /media/Autobleem/*.sh \
        /media/Autobleem/bin/autobleem/autobleem-gui /media/Autobleem/bin/autobleem/run.sh \
        /media/Autobleem/bin/autobleem/abfetch /media/Autobleem/bin/autobleem/abupdate \
        /media/Autobleem/bin/autobleem/absplash /media/Autobleem/bin/autobleem/abdialog \
        /media/Autobleem/bin/autobleem/abfatflag /media/Autobleem/bin/emu*/pcsx-ab \
        /media/Autobleem/bin/abpad/abpadd; do
        [ -f "$f" ] && [ ! -x "$f" ] && chmod +x "$f" 2> /dev/null
    done
    return 0
}

# The one-time setup of a boot. Not done again when this run is the restart of the loop after an update: the
# console is already set up (udev rules, the pad seat, the modules, the picture on the screen).
boot_once() {
    # off the stick from the first step: everything started from here (the boot picture's absplash first) inherits
    # this cwd, and the stick check below (checkstick.sh) unmounts the stick - Sony's start.sh left us in rc/
    cd /tmp
    fix_exec_bits
    # Sony's UI first, at once (the stock logo held the screen until killsony.sh, far below), and again after a
    # second for the late forks (killsony.sh without "now" sleeps one) - in the background, the boot goes on
    $RC/killsony.sh now
    $RC/killsony.sh > /dev/null 2>&1 &
    bt "boot.sh started, Sony's UI killed"

    # OUR picture from the first moment until the launcher's window is up (the launcher removes /tmp/.abload
    # itself, as after a wake or RetroArch): absplash and its pictures are copied to tmpfs - the stick is
    # unmounted at the standby, a binary running from it would hold it - and the libraries it needs are unpacked
    # to /tmp/lib (RAM) now instead of by autobleem.sh much later. Run with the sweep along the picture's rule
    # (--anim) and never for long: the launcher's own start ends it, the timeout only the case it never comes.
    sh $RC/unpack_libs.sh
    bt "libraries unpacked to /tmp/lib"
    cp -f /media/Autobleem/bin/autobleem/absplash /tmp/absplash && chmod +x /tmp/absplash
    copy_pictures autobleem
    show_splash

    # where this run's logs go (RAM unless kept on the stick) - exported to everything below, the launcher too
    . $RC/ab_log.sh
    cat /tmp/boot-steps.early >> "$AB_LOG_DIR/boot.log" 2>/dev/null && rm -f /tmp/boot-steps.early
    BT_FILE=$AB_LOG_DIR/boot.log
    bt "log directory $AB_LOG_DIR"

    # AUTOBLEEM-31: overcommit_memory=1 and zram swap in RAM (rc/ab_memory.sh) - the N64 and Dreamcast emulators
    # reserve big address ranges up front, which the stock overcommit 0 refuses. Once per boot; idempotent.
    . $RC/ab_memory.sh
    ab_memory_setup
    bt "memory policy set (ab_memory.sh)"

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

    # a USB CD/DVD drive (the disc drive: a PS1 disc played from it) - the AutoBleem kernel's cdrom, sr_mod and sg:
    # its own (psc-kernel-payload builds them into the payload's /lib/modules since 2026-10-13: modprobe), else
    # the stick's Autobleem/lib/modules/cd/*.ko for a kernel flashed before that (insmod, cdrom first: sr_mod
    # needs it). Only into the AutoBleem kernel, whose rootfs carries the /autobleem marker - never into Sony's
    # own. udev must not read the disc's filesystem: blkid's 2048-byte reads of a PS1 disc (Mode 2) time out after
    # 30 s each and the emulator's raw reads wait behind them (minutes on the Pi). udev 229 has no flag for that,
    # so its 60-persistent-storage.rules goes to /run (tmpfs, the same name overrides /lib's) without the two sr
    # blocks - harmless with no drive.
    if [ -e /autobleem ]; then
        for name in cdrom sr_mod sg; do
            grep -qs "^$name " /proc/modules && continue
            grep -qs "/$name.ko" "$KMODDIR/modules.builtin" && continue
            if grep -qs "/$name.ko" "$KMODDIR/modules.dep"; then
                modprobe "$name" && continue
                echo "boot: modprobe $name failed"
            fi
            kmod=/media/Autobleem/lib/modules/cd/$name.ko
            if [ -f "$kmod" ]; then
                insmod "$kmod" || echo "boot: $name.ko refused"
            else
                echo "boot: no $name for this kernel"
            fi
        done
        if [ ! -f /run/udev/rules.d/60-persistent-storage.rules ] && [ -f /lib/udev/rules.d/60-persistent-storage.rules ]; then
            mkdir -p /run/udev/rules.d
            sed '/^KERNEL=="sr\*", ENV{DISK_EJECT_REQUEST}!=/,/IMPORT{builtin}="blkid/d' \
                /lib/udev/rules.d/60-persistent-storage.rules > /run/udev/rules.d/60-persistent-storage.rules
            udevadm control --reload-rules
        fi
        bt "CD drive modules done"
    fi

    # a third time, the udev work and the modules gave Sony's late starters time to come up (no sleep this time)
    $RC/killsony.sh now
    $RC/backup.sh
    bt "backup.sh done"
    # AUTOBLEEM-29: the filesystem check (rc/ab_fsck.sh) and its tools on tmpfs, with the pictures - checkstick.sh
    # unmounts the stick for a check, and selection.sh checks a stick that comes back after a pull
    cp -f $RC/ab_fsck.sh /tmp/ab_fsck.sh && (. /tmp/ab_fsck.sh && ab_fsck_copy)
    copy_stick_pictures
    # the stick's dirty flag (and its check), before anything of ours writes to it - from a tmpfs copy, with this
    # shell's cwd on tmpfs (the top of boot_once): a script run from the stick holds it open, and a check needs the
    # stick unmounted
    cp -f $RC/checkstick.sh /tmp/checkstick.sh
    sh /tmp/checkstick.sh
    bt "checkstick.sh done"
    # the AutoBleem kernel's SSH key from the stick, if any (C10) - a no-op on the stock kernel
    $RC/ssh_keys.sh
    bt "ssh_keys.sh done"
}
if [ -z "$AB_BOOT_RESTARTED" ]; then
    boot_once
else
    . $RC/ab_log.sh
    BT_FILE=$AB_LOG_DIR/boot.log
    bt "boot.sh restarted by the loop (run $AB_BOOT_RESTARTS) - the console is set up already"
fi

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
    [ -n "$want" ] || want=$(ab_config_get outputmode)
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
    show_splash
    bt "Weston back in ${want}"
}

# AUTOBLEEM-33: the stick boot (the new /init: the root's upper layer on tmpfs below /abrw) leaves the console's /data
# read-write, as Sony's own boot has it. It is read-only only while the AutoBleem launcher runs, and read-write again
# when the launcher has left (standby, RetroArch, power off: Sony's power manager and saves write there). The legacy
# boot keeps its overlay on /data - never touched there. ext4 with its journal: the remount to ro flushes it.
data_mode() {
    grep -qs ' / overlay .*upperdir=/abrw/' /proc/mounts || return 0
    grep -qs ' /data ext4 ' /proc/mounts || return 0
    sync
    if mount -o remount,"$1" /data 2> /dev/null; then
        bt "/data $1"
    else
        bt "/data $1 FAILED (still $(awk '$2 == "/data" { split($4, o, ","); print o[1] }' /proc/mounts))"
    fi
}

# The launcher, and after it whatever it asked for: selection.sh comes back (exit 0) after a standby or a
# RetroArch session and the launcher is started over; a reboot never returns. It runs from a copy on
# tmpfs with the cwd there, so nothing of ours is on the stick while it unmounts it - the copy is taken
# every time round, so a stick updated during a standby is what runs at the next power off; the wake-up
# picture (absplash, shown from the resume until the launcher's window is up) goes to tmpfs with it, the
# stick not being mounted at the wake.
#
# The loop itself lives in the shell's memory, so it is also what an update must not leave behind: at the top of
# every round the stick's boot.sh is compared with the one this shell loaded (BOOT_SUM), and when it differs (an
# update, selection.sh's abupdate) a tmpfs copy of the new file replaces this shell (exec: the same process, the
# same cwd and environment - no second loop, nothing left from the old one) with AB_BOOT_RESTARTED set, which
# skips boot_once. A file that does not parse (sh -n) or a restart that keeps happening (5) is left alone.
restart_if_updated() {
    [ -s $RC/boot.sh ] || return
    [ "$(cksum < $RC/boot.sh 2> /dev/null)" = "$BOOT_SUM" ] && return
    ab_n=${AB_BOOT_RESTARTS:-0}
    if [ "$ab_n" -ge 5 ]; then
        bt "boot.sh on the stick changed again - not restarting (limit)"
        return
    fi
    grep -q "$(printf '\r')" $RC/boot.sh && sed -i 's/\r//g' $RC/boot.sh # as Autobleem/start.sh does
    cp -f $RC/boot.sh /tmp/boot.restart.sh # a copy on tmpfs: a shell running the stick's file would hold the stick
    if ! sh -n /tmp/boot.restart.sh 2> /dev/null; then
        bt "boot.sh on the stick changed but does not parse - keeping the running one"
        return
    fi
    bt "boot.sh on the stick changed (an update) - running the new one"
    AB_BOOT_RESTARTED=1
    AB_BOOT_RESTARTS=$((ab_n + 1))
    export AB_BOOT_RESTARTED AB_BOOT_RESTARTS
    exec sh /tmp/boot.restart.sh
}

while true; do
    cd $RC
    restart_if_updated
    bt "loop: launcher round starts"
    show_splash   # not up (the launcher left for a new display mode, the first try failed): up now
    apply_output_mode
    # CONSOLE-15 evidence: the watch log (RAM, one line per 30 s - see ab_watch.sh); it runs from a tmpfs copy
    # and is stopped by selection.sh before the stick is unmounted, so it is started again every time round
    sh $RC/ab_watch.sh start
    # AUTOBLEEM-28: what the round needs when the stick is pulled in the middle of it, on tmpfs before it starts -
    # selection.sh and its ab_log.sh (taken again after the launcher, below, when the stick is still there), the
    # pictures - and the stick's watch, which stops and kills everything below this shell when the stick goes
    cp -f $RC/selection.sh /tmp/selection.sh
    cp -f $RC/ab_log.sh /tmp/ab_log.sh
    copy_stick_pictures
    sh $RC/ab_stickwatch.sh start $$
    rm -f "$AB_RUNTIME_DIR/autobleem_exit"
    bt "starting autobleem.sh"
    fix_exec_bits # an update during the last round may have brought files without the bit
    data_mode ro
    ./autobleem.sh
    ab_sh_rc=$?
    bt "autobleem.sh ended (status $ab_sh_rc)"
    data_mode rw
    # autobleem.sh writes the launcher's own status; if it was killed itself, its status is the next best
    [ -f "$AB_RUNTIME_DIR/autobleem_exit" ] || echo $ab_sh_rc > "$AB_RUNTIME_DIR/autobleem_exit"
    # (the stick pulled: these copies fail and the ones taken before the round stay)
    cp -f $RC/selection.sh /tmp/selection.sh 2>/dev/null
    cp -f /media/Autobleem/bin/autobleem/absplash /tmp/absplash 2>/dev/null && chmod +x /tmp/absplash
    copy_pictures autobleem updating poweroff 2>/dev/null
    cd /tmp
    sh /tmp/selection.sh && continue
    # AUTOBLEEM-28: the stick was pulled while selection.sh's own work ran (RetroArch, an update) - the watch killed
    # it and left the no-stick selection (9): selection.sh again, from its tmpfs copy, waits for the stick
    [ -f /tmp/.abnostick ] && sh /tmp/selection.sh && continue
    break
done
sync
systemctl reboot   # systemd's: a busybox reboot over it only signals init and returns
