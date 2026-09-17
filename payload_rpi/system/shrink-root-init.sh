#!/bin/sh
#
# Shrink the root filesystem and put an exFAT data partition in the space that frees up.
#
# This runs as init= (i.e. as PID 1, before the root filesystem is mounted read-write), because ext4 cannot be
# shrunk while it is mounted. It is the same trick Raspberry Pi OS uses for its own first-boot expansion
# (/usr/lib/raspberrypi-sys-mods/init_resize.sh), and it is installed, armed and disarmed by install.sh
# --shrink-root; nothing else should ever run it.
#
# The very first thing it does is put the original cmdline.txt back, so that however badly the rest goes the
# Pi boots normally next time instead of running this again.
set -u

PATH=/usr/sbin:/usr/bin:/sbin:/bin
export PATH

LOG=/dev/kmsg

say() {
    echo "autobleem-shrink: $*" > "$LOG" 2>/dev/null
    echo "autobleem-shrink: $*"
}

finish() {
    say "$1"
    say "rebooting"
    sync
    sleep 2
    reboot -f
    # reboot -f should not return; if it somehow does, do not fall through to a half-configured system
    while true; do sleep 60; done
}

mount -t proc proc /proc 2>/dev/null
mount -t sysfs sys /sys 2>/dev/null
mount -t tmpfs tmp /run 2>/dev/null

say "started"

#*******************************
# what to do, off the kernel command line
#*******************************
GIB=""
LABEL="AUTOBLEEM"
for arg in $(cat /proc/cmdline); do
    case "$arg" in
        ab_shrink_gib=*)   GIB="${arg#ab_shrink_gib=}" ;;
        ab_shrink_label=*) LABEL="${arg#ab_shrink_label=}" ;;
    esac
done

#*******************************
# which devices
#*******************************
ROOT_PART_DEV=$(findmnt / -o source -n)
ROOT_PART_NAME=$(echo "$ROOT_PART_DEV" | cut -d "/" -f 3)
ROOT_DEV_NAME=$(echo /sys/block/*/"${ROOT_PART_NAME}" | cut -d "/" -f 4)
ROOT_DEV="/dev/${ROOT_DEV_NAME}"
ROOT_PART_NUM=$(cat "/sys/block/${ROOT_DEV_NAME}/${ROOT_PART_NAME}/partition")

say "root=$ROOT_PART_DEV disk=$ROOT_DEV part=$ROOT_PART_NUM target=${GIB}GiB label=$LABEL"

#*******************************
# disarm before doing anything destructive
#*******************************
# The boot partition is the first one on the same disk. Putting the saved cmdline.txt back now means a crash,
# a power cut or a bug below costs at most a half-resized filesystem - never a card that boots into this
# script for ever.
case "$ROOT_DEV" in
    *[0-9]) BOOT_PART_DEV="${ROOT_DEV}p1" ;;
    *)      BOOT_PART_DEV="${ROOT_DEV}1" ;;
esac

mkdir -p /run/abboot
if mount -t vfat -o rw "$BOOT_PART_DEV" /run/abboot 2>/dev/null; then
    if [ -f /run/abboot/cmdline.txt.autobleem-backup ]; then
        cp -f /run/abboot/cmdline.txt.autobleem-backup /run/abboot/cmdline.txt
        say "restored cmdline.txt"
    else
        say "WARNING: no cmdline.txt.autobleem-backup to restore"
    fi
    sync
    umount /run/abboot
else
    finish "cannot mount the boot partition ($BOOT_PART_DEV) to disarm - refusing to touch the card"
fi

case "$GIB" in
    ''|*[!0-9]*) finish "ab_shrink_gib=$GIB is not a whole number of GiB - nothing done" ;;
esac

#*******************************
# shrink
#*******************************
mount / -o remount,ro || finish "cannot remount the root filesystem read-only - nothing done"

say "checking $ROOT_PART_DEV"
e2fsck -fy "$ROOT_PART_DEV" || say "e2fsck returned $? (1 and 2 mean it fixed things, higher is trouble)"

# leave the filesystem a little smaller than the partition will be, so the partition never ends up the
# smaller of the two - that is the one ordering mistake here that loses data
FS_MIB=$(( GIB * 1024 - 16 ))
say "resizing the filesystem to ${FS_MIB}MiB"
resize2fs "$ROOT_PART_DEV" "${FS_MIB}M" || finish "resize2fs failed - the filesystem is unchanged"

PART_START_MIB=$(parted -ms "$ROOT_DEV" unit MiB print | awk -F: -v p="$ROOT_PART_NUM" \
    '$1 == p { sub("MiB", "", $2); print int($2) }')
[ -n "$PART_START_MIB" ] || finish "cannot read where partition $ROOT_PART_NUM starts"
PART_END_MIB=$(( PART_START_MIB + GIB * 1024 ))

say "resizing partition $ROOT_PART_NUM to end at ${PART_END_MIB}MiB"
parted -s "$ROOT_DEV" resizepart "$ROOT_PART_NUM" "${PART_END_MIB}MiB" \
    || finish "parted resizepart failed - the filesystem is smaller than the partition, which is safe"

#*******************************
# the data partition
#*******************************
say "creating the $LABEL partition"
parted -s "$ROOT_DEV" mkpart primary "${PART_END_MIB}MiB" 100% \
    || finish "parted mkpart failed - the root partition is shrunk, re-run the installer to try again"

NEW_PART_NUM=$(parted -ms "$ROOT_DEV" unit MiB print | awk -F: '/^[0-9]+:/ { n = $1 } END { print n }')
case "$ROOT_DEV" in
    *[0-9]) NEW_PART_DEV="${ROOT_DEV}p${NEW_PART_NUM}" ;;
    *)      NEW_PART_DEV="${ROOT_DEV}${NEW_PART_NUM}" ;;
esac

# 0x07 is the MBR type Windows reads as exFAT; without it the card's data partition is invisible there
sfdisk --part-type "$ROOT_DEV" "$NEW_PART_NUM" 7 || say "WARNING: could not set the partition type to 0x07"

partprobe "$ROOT_DEV" 2>/dev/null
sleep 2

say "formatting $NEW_PART_DEV as exFAT"
mkfs.exfat -n "$LABEL" "$NEW_PART_DEV" || finish "mkfs.exfat failed - partition $NEW_PART_NUM exists but is not formatted"

finish "done - re-run install.sh when the Pi comes back up"
