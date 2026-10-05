#!/bin/sh
# pad_seat.sh - no mouse cursor from a pad's touchpad (rc/99-autobleem-pad-seat.rules says why). Run by boot.sh:
#   1. the rule into /run/udev/rules.d (tmpfs; udev reads it next to /etc/udev/rules.d), and udev told to reload;
#   2. a pad touchpad or motion-sensors node that is already there (a pad paired before this ran) is announced to udev
#      again - "remove", then "add" - so the rule applies and Weston's libinput drops it as a pointer. A pad that
#      connects later meets the rule on its own "add".
# AB_PAD_SEAT_RULES_DIR / AB_PAD_SEAT_SYSFS / AB_PAD_SEAT_UDEVADM: other places, for the test (tests/rc/test_pe_run.cpp).

RC_DIR=$(cd "$(dirname "$0")" && pwd)
RULES_DIR=${AB_PAD_SEAT_RULES_DIR:-/run/udev/rules.d}
SYSFS=${AB_PAD_SEAT_SYSFS:-/sys/class/input}
UDEVADM=${AB_PAD_SEAT_UDEVADM:-udevadm}

mkdir -p "$RULES_DIR"
cp -f "$RC_DIR/99-autobleem-pad-seat.rules" "$RULES_DIR/99-autobleem-pad-seat.rules"
$UDEVADM control --reload-rules 2> /dev/null

for node in "$SYSFS"/event*; do
    [ -f "$node/device/name" ] && [ -f "$node/uevent" ] || continue
    name=$(cat "$node/device/name")
    case "$name" in
        *"Wireless Controller Touchpad" | *DualSense*Touchpad | *DualShock*Touchpad | \
            *"Wireless Controller Motion Sensors" | *DualSense*"Motion Sensors" | *DualShock*"Motion Sensors")
            echo remove > "$node/uevent"
            echo add > "$node/uevent"
            echo "pad_seat: $(basename "$node") ($name) announced again"
            ;;
    esac
done
