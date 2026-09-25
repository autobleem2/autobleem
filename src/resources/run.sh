#!/bin/bash

# the launcher's stdout/stderr go where this run's logs go - RAM (/tmp/autobleem/logs) unless the logs are
# kept on the stick (rc/ab_log.sh, docs/quiet-stick-plan.md)
. /media/Autobleem/rc/ab_log.sh
# A kernel with the backported pad drivers as modules (psc-kernel-payload 2026-09-25 on: hid-sony,
# hid-playstation, hid-nintendo, xpad) drives the pads itself - SDL's own HIDAPI drivers then only get in the
# way: SDL 2.0.14's PS4 driver probes a Bluetooth DualShock 4 with a GET_REPORT for feature 0x12, which the
# pad answers by hanging up (btmon, 2026-09-25). Off, so SDL reads the pads through the kernel's evdev
# devices; exported, so the emulators the launcher starts see the same. Older kernels keep SDL's drivers.
if [ -f "/lib/modules/$(uname -r)/kernel/drivers/hid/hid-sony.ko" ]; then
    export SDL_JOYSTICK_HIDAPI=0
fi
LD_LIBRARY_PATH=/tmp/lib ./autobleem-gui /media > "$AB_LOG_DIR/AB_out.txt" 2> "$AB_LOG_DIR/AB_err.txt"
