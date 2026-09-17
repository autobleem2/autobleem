#!/bin/sh
#
# Shrink the root filesystem and put an exFAT data partition in the space that frees up.
#
# This is an initramfs-tools local-premount script: it runs inside the initramfs after the root device has
# been found and BEFORE it is mounted - the only moment ext4 can be shrunk, which is why the job is not done
# from the running system, nor from an init= script (by then the root filesystem is mounted, and resize2fs
# refuses to shrink a mounted filesystem; Raspberry Pi OS's own init= resize only ever grows). It is
# modelled on Raspberry Pi OS's resize_early, which sits in the same directory and grows the root partition
# the same way. shrink-root-hook.sh packs the tools it uses; install.sh --shrink-root installs both, rebuilds
# the initramfs, adds ab_shrink_gib=<GiB> ab_shrink_label=<label> to cmdline.txt and reboots. Without those
# two parameters this script does nothing, so a leftover copy in the initramfs is harmless.
#
# Order of business, and why: (1) find the devices; anything wrong here lets the boot go on normally, so the
# Pi comes up and can be fixed over ssh. (2) put the original cmdline.txt back - from here on a reboot is
# safe, because the next boot will not run this again. (3) only then shrink, repartition, format. A failure
# in (3) reboots into a normal system with at worst a half-resized root; never into a loop. Only shell
# builtins, busybox (awk, lsblk-less fallbacks) and the packed tools are used.
PREREQ=""
prereqs() { echo "$PREREQ"; }
case "${1:-}" in
    prereqs) prereqs; exit 0 ;;
esac

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
[ -n "$GIB" ] || exit 0    # not armed: an ordinary boot

. /scripts/functions
. /scripts/local

# Every message goes to the console, to the kernel log, and to a log file that finish() copies onto the boot
# partition as autobleem-shrink.log: the journal is not persistent on Raspberry Pi OS, so without that file
# nothing said here survives the reboot at the end. The tools' own output is appended to the same file.
LOG=/run/autobleem-shrink.log
say() {
    echo "autobleem-shrink: $*" > /dev/kmsg 2>/dev/null
    echo "autobleem-shrink: $*" > /dev/console 2>/dev/null
    echo "autobleem-shrink: $*" >> "$LOG" 2>/dev/null
}

# before the disarm: give up and let the boot continue, so the system comes up as it is
bail() {
    say "$1"
    say "NOT rebooting - the boot continues; remove ab_shrink_* from cmdline.txt and try again"
    sleep 5
    exit 0
}

# after the disarm: leave the log on the boot partition and reboot into the (now normal) system
finish() {
    say "$1"
    say "rebooting"
    if mount -t vfat -o rw "$BOOT_PART_DEV" /run/abboot 2>/dev/null; then
        cat "$LOG" >> /run/abboot/autobleem-shrink.log 2>/dev/null
        sync
        umount /run/abboot 2>/dev/null
    fi
    sync
    sleep 3
    reboot -f
    echo b > /proc/sysrq-trigger    # if reboot somehow returned, the kernel's own way
    sleep 60
    exit 0
}

say "started (target ${GIB}GiB, label $LABEL)"

#*******************************
# which devices
#*******************************
# $ROOT is still what cmdline.txt said (root=PARTUUID=...); local_device_setup waits for the device and
# leaves the /dev node in $DEV - exactly what resize_early does
local_device_setup "$ROOT" "root file system"
ROOT_PART_DEV="${DEV:-}"
[ -b "$ROOT_PART_DEV" ] || bail "cannot resolve root=$ROOT to a block device (got '$ROOT_PART_DEV')"

ROOT_PART_NAME="$(lsblk -no kname "$ROOT_PART_DEV" 2>/dev/null)"
ROOT_DEV_NAME="$(lsblk -no pkname "$ROOT_PART_DEV" 2>/dev/null)"
if [ -z "$ROOT_PART_NAME" ] || [ -z "$ROOT_DEV_NAME" ]; then
    # no lsblk in this initramfs: /dev/mmcblk0p2 -> mmcblk0p2 / mmcblk0, /dev/sda2 -> sda2 / sda
    ROOT_PART_NAME="${ROOT_PART_DEV#/dev/}"
    ROOT_DEV_NAME="${ROOT_PART_NAME%p[0-9]*}"
    [ "$ROOT_DEV_NAME" != "$ROOT_PART_NAME" ] || ROOT_DEV_NAME="${ROOT_PART_NAME%[0-9]*}"
fi
ROOT_DEV="/dev/$ROOT_DEV_NAME"
ROOT_PART_NUM="$(cat "/sys/block/$ROOT_DEV_NAME/$ROOT_PART_NAME/partition" 2>/dev/null)"
[ -n "$ROOT_PART_NUM" ] || bail "cannot read the partition number of $ROOT_PART_DEV"

# the boot partition is the first one on the same disk
case "$ROOT_DEV" in
    *[0-9]) BOOT_PART_DEV="${ROOT_DEV}p1" ;;
    *)      BOOT_PART_DEV="${ROOT_DEV}1" ;;
esac
say "root=$ROOT_PART_DEV disk=$ROOT_DEV part=$ROOT_PART_NUM boot=$BOOT_PART_DEV"

#*******************************
# disarm before doing anything destructive
#*******************************
modprobe vfat 2>/dev/null
mkdir -p /run/abboot
mount -t vfat -o rw "$BOOT_PART_DEV" /run/abboot 2>/dev/null \
    || bail "cannot mount the boot partition ($BOOT_PART_DEV) to disarm - refusing to touch the card"
if [ -f /run/abboot/cmdline.txt.autobleem-backup ]; then
    # cat rather than cp: a klibc initramfs has no cp
    cat /run/abboot/cmdline.txt.autobleem-backup > /run/abboot/cmdline.txt
else
    # no backup: strip the two parameters from the live line instead
    line="$(cat /run/abboot/cmdline.txt)"
    new=""
    for arg in $line; do
        case "$arg" in
            ab_shrink_gib=*|ab_shrink_label=*) ;;
            *) new="$new${new:+ }$arg" ;;
        esac
    done
    echo "$new" > /run/abboot/cmdline.txt
fi
sync
umount /run/abboot || bail "cannot unmount the boot partition after restoring cmdline.txt"
echo "==== $(date 2>/dev/null) target ${GIB}GiB label $LABEL root $ROOT_PART_DEV" >> "$LOG"
say "restored cmdline.txt - the next boot is a normal one whatever happens below"

case "$GIB" in
    ''|*[!0-9]*) finish "ab_shrink_gib=$GIB is not a whole number of GiB - nothing done" ;;
esac

#*******************************
# shrink
#*******************************
say "checking $ROOT_PART_DEV (this takes a while)"
e2fsck -fy "$ROOT_PART_DEV" >> "$LOG" 2>&1
rc=$?
say "e2fsck returned $rc"
[ "$rc" -le 2 ] || finish "e2fsck returned $rc - not resizing a filesystem it could not repair"

# leave the filesystem a little smaller than the partition will be, so the partition never ends up the
# smaller of the two - that is the one ordering mistake here that loses data
FS_MIB=$(( GIB * 1024 - 16 ))
# -f: resize2fs otherwise wants the "last checked" time to be newer than the last mount, and a Pi has no
# RTC - in the initramfs the clock reads 1970, so the e2fsck above stamps a time older than any mount and
# resize2fs asks for "e2fsck -f" for ever. The check did just run; -f only skips the timestamp test.
say "resizing the filesystem to ${FS_MIB}MiB (this takes a while too)"
resize2fs -f "$ROOT_PART_DEV" "${FS_MIB}M" >> "$LOG" 2>&1 || finish "resize2fs failed - the filesystem is unchanged"

# sfdisk rather than parted: parted's resizepart asks "shrinking can cause data loss, are you sure?" and in
# script mode answers itself with no. sfdisk is non-interactive: ",<size>" on partition N keeps its start.
say "resizing partition $ROOT_PART_NUM to ${GIB}GiB"
echo ",${GIB}GiB" | sfdisk -N "$ROOT_PART_NUM" "$ROOT_DEV" >> "$LOG" 2>&1 \
    || finish "sfdisk resize failed - the filesystem is smaller than the partition, which is safe"

#*******************************
# the data partition
#*******************************
# the new partition starts right after the root one: start + size of it, from sfdisk's dump
# ("/dev/mmcblk0p2 : start=     1064960, size=  33554432, type=83"). Without an explicit start sfdisk -a
# would take the first gap it finds, which is the few MiB before the boot partition.
NEW_START="$(sfdisk -d "$ROOT_DEV" 2>/dev/null | awk -v p="$ROOT_PART_DEV" '
    $1 == p { s = 0; z = 0
              for (i = 1; i <= NF; i++) { if ($i == "start=") s = $(i+1); if ($i == "size=") z = $(i+1) }
              gsub(",", "", s); gsub(",", "", z); print s + z }')"
case "$NEW_START" in
    ''|*[!0-9]*) finish "cannot work out where partition $ROOT_PART_NUM ends now - root is shrunk, nothing else done" ;;
esac

say "creating the $LABEL partition from sector $NEW_START to the end of the card"
# 0x07 is the MBR type Windows reads as exFAT; without it the card's data partition is invisible there
echo "${NEW_START},,7" | sfdisk -a "$ROOT_DEV" >> "$LOG" 2>&1 \
    || finish "sfdisk could not add the partition - the root partition is shrunk, re-run the installer to try again"

NEW_PART_NUM="$(sfdisk -d "$ROOT_DEV" 2>/dev/null | awk -v d="$ROOT_DEV" '
    index($1, d) == 1 && $2 == ":" { n = $1; sub(".*[^0-9]", "", n); last = n } END { print last }')"
case "$ROOT_DEV" in
    *[0-9]) NEW_PART_DEV="${ROOT_DEV}p${NEW_PART_NUM}" ;;
    *)      NEW_PART_DEV="${ROOT_DEV}${NEW_PART_NUM}" ;;
esac

partprobe "$ROOT_DEV" >> "$LOG" 2>&1 || true   # parted's; not packed, the rereadpt below is what counts
blockdev --rereadpt "$ROOT_DEV" >> "$LOG" 2>&1
sleep 2
[ -b "$NEW_PART_DEV" ] || finish "$NEW_PART_DEV did not appear - the partition exists but is not formatted; re-run the installer"

say "formatting $NEW_PART_DEV as exFAT"
modprobe exfat 2>/dev/null
LC_ALL=C.UTF-8 mkfs.exfat -n "$LABEL" "$NEW_PART_DEV" >> "$LOG" 2>&1 \
    || finish "mkfs.exfat failed - partition $NEW_PART_NUM exists but is not formatted"
sfdisk -d "$ROOT_DEV" >> "$LOG" 2>&1

finish "done - re-run install.sh when the Pi comes back up"
