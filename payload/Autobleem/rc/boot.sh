#!/bin/sh

#USB gamepad fix
mount -o bind /media/Autobleem/rc/20-joystick.rules /etc/udev/rules.d/20-joystick.rules
udevadm control --reload-rules
udevadm trigger

# kernel modules the stick carries beyond the firmware's (xpad.ko for Xbox pads - the site's libs pack,
# unpacked by the installer into Autobleem/lib/modules)
for kmod in /media/Autobleem/lib/modules/*.ko; do
    [ -f "$kmod" ] && insmod "$kmod"
done

./killsony.sh
./backup.sh
./autobleem.sh
./selection.sh






