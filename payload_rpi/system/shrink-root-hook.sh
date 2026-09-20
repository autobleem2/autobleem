#!/bin/sh
#
# initramfs-tools hook for AutoBleem's root shrink: packs the tools shrink-root-premount.sh needs into the
# initramfs, plus the filesystem modules it mounts. Installed by install.sh --shrink-root as
# /etc/initramfs-tools/hooks/autobleem-shrink and removed again once the data partition exists.
#
# Why the initramfs: ext4 can only be shrunk unmounted, and by the time anything in the root filesystem
# runs - init=, systemd, a service - the root filesystem is mounted. local-premount is the one moment
# where the root device is known and not yet mounted.
PREREQ=""
prereqs() { echo "$PREREQ"; }
case "${1:-}" in
    prereqs) prereqs; exit 0 ;;
esac

. /usr/share/initramfs-tools/hook-functions

# copy_exec brings each binary's shared libraries along
copy_exec /usr/sbin/e2fsck     /sbin
copy_exec /usr/sbin/resize2fs  /sbin
copy_exec /usr/sbin/sfdisk     /sbin
copy_exec /usr/sbin/mkfs.exfat /sbin
copy_exec /usr/sbin/blockdev   /sbin
copy_exec /usr/bin/lsblk       /bin

# the boot partition is FAT (to restore cmdline.txt), the new partition is exFAT (to label it)
manual_add_modules vfat nls_cp437 nls_ascii nls_utf8 exfat
