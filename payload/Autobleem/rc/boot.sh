#!/bin/sh

# Sourced (through Autobleem/start.sh) into the shell Sony's usb_watch runs our payload with, so the cd's
# here move that shell: its cwd is the one thing of Sony's that ever holds /media, and it has to be off the
# stick while selection.sh unmounts it for the standby (see selection.sh). Everything else is reached by
# absolute path for the same reason.

RC=/media/Autobleem/rc

# USB gamepad fix - the rules file from tmpfs: it survives the standby (the stick is unmounted then) and
# holds nothing on the stick
cp -f $RC/20-joystick.rules /tmp/20-joystick.rules
mount -o bind /tmp/20-joystick.rules /etc/udev/rules.d/20-joystick.rules
udevadm control --reload-rules
udevadm trigger

# kernel modules the stick carries beyond the firmware's (xpad.ko for Xbox pads - the site's libs pack,
# unpacked by the installer into Autobleem/lib/modules)
for kmod in /media/Autobleem/lib/modules/*.ko; do
    [ -f "$kmod" ] && insmod "$kmod"
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
