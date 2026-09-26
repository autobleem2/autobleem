#!/bin/sh

# Sourced (through Autobleem/start.sh) into the shell Sony's usb_watch runs our payload with, so the cd's
# here move that shell: its cwd is the one thing of Sony's that ever holds /media, and it has to be off the
# stick while selection.sh unmounts it for the standby (see selection.sh). Everything else is reached by
# absolute path for the same reason.

RC=/media/Autobleem/rc

# where this run's logs go (RAM unless kept on the stick) - exported to everything below, the launcher too
. $RC/ab_log.sh

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

$RC/killsony.sh
$RC/backup.sh
# the stick's dirty flag, before anything of ours writes to it
$RC/checkstick.sh

# The launcher, and after it whatever it asked for: selection.sh comes back (exit 0) after a standby or a
# RetroArch session and the launcher is started over; a reboot never returns. It runs from a copy on
# tmpfs with the cwd there, so nothing of ours is on the stick while it unmounts it - the copy is taken
# every time round, so a stick updated during a standby is what runs at the next power off; the wake-up
# picture (absplash, shown from the resume until the launcher's window is up) goes to tmpfs with it, the
# stick not being mounted at the wake.
while true; do
    cd $RC
    ./autobleem.sh
    cp -f $RC/selection.sh /tmp/selection.sh
    cp -f /media/Autobleem/bin/autobleem/absplash /tmp/absplash && chmod +x /tmp/absplash
    cp -f /media/Autobleem/bin/autobleem/splash/autobleem.jpg /tmp/autobleem.jpg
    cd /tmp
    sh /tmp/selection.sh || break
done
sync
systemctl reboot   # systemd's: a busybox reboot over it only signals init and returns
