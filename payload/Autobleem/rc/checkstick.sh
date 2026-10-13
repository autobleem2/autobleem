#!/bin/sh

# The stick's "dirty" flag at boot, before the launcher writes anything. Sony's usb_watch mounted the stick
# rw at boot and the console then went into its standby with it mounted (start_pman's "1st suspend") -
# a stick pulled during that standby comes back with the flag set, and the kernel never clears a flag it
# found set at mount time (fat_set_state's sbi->dirty gate): Windows would offer "scan and fix" at every
# insertion from then on. Nothing was being written during that standby, so the flag is cleared here -
# but only when the previous session ended through selection.sh's standby (System/.session is gone). A
# session that ended any other way - the stick pulled while the launcher ran, a crash - leaves the marker,
# the flag stays, and Windows gets to check the stick. The tool is copied to tmpfs for selection.sh,
# which needs it with the stick unmounted.

BIN=/media/Autobleem/bin/autobleem
LOG=/media/System/Logs/standby.log
DEV=$(awk '$2 == "/media" { print $1 }' /proc/mounts | head -1)

rm -f /tmp/ab_stick_owned
cp -f $BIN/abfatflag /tmp/abfatflag && chmod +x /tmp/abfatflag
mkdir -p /media/System/Logs 2> /dev/null

# AUTOBLEEM-29: the last session did not end through the standby (System/.session is still there: the stick pulled
# while the launcher ran, the power cut) and the filesystem says so - a FAT's dirty flag, or any stick the kernel
# already put read-only after an error - so it is checked and repaired before anything of ours writes to it: the
# check-stick screen with its bar (rc/ab_fsck.sh), the stick unmounted for the check and mounted again after it.
# boot.sh runs this with the cwd on tmpfs, so nothing of ours holds the stick. A stick that cannot be unmounted or
# checked is mounted as it was and left for Windows' own "scan and fix", as before.
stick_needs_check() {
    grep -q " /media [^ ]* ro[, ]" /proc/mounts && return 0
    [ -f /media/System/.session ] || return 1
    case "$(blkid "$DEV" 2> /dev/null | sed -n 's/.* TYPE="\([^"]*\)".*/\1/p')" in
    vfat | msdos | fat)
        # read with the stick read-only: mounted rw the kernel has set the flag itself (see below)
        [ -x /tmp/abfatflag ] || return 1
        sync
        mount -o remount,ro /media || return 1
        case "$(/tmp/abfatflag "$DEV")" in dirty*) return 0 ;; esac
        mount -o remount,rw /media
        return 1
        ;;
    esac
    return 0 # exFAT / ext: their checkers are quick on a clean filesystem
}
if [ -n "$DEV" ] && [ -f /tmp/ab_fsck.sh ] && stick_needs_check; then
    . /tmp/ab_fsck.sh
    rm -f $ABFSCK_LOG
    sync
    n=0
    until umount /media 2> /dev/null; do
        n=$((n + 1))
        [ $n -ge 5 ] && break
        sleep 1
    done
    if grep -q ' /media ' /proc/mounts; then
        echo "$(date) boot: $DEV needs a check but /media is busy - not checked" >> $ABFSCK_LOG
    else
        ab_fsck "$DEV"
        mount "$DEV" /media || echo "$(date) boot: $DEV did not mount again after the check" >> $ABFSCK_LOG
    fi
    if grep -q ' /media ' /proc/mounts; then
        mkdir -p /media/System/Logs
        cat $ABFSCK_LOG >> /media/System/Logs/fsck.log
    fi
fi

if [ -x /tmp/abfatflag ] && [ -n "$DEV" ] && [ ! -f /media/System/.session ]; then
    sync
    if mount -o remount,ro /media; then
        # a clean volume was cleared by the remount itself (the kernel owned it); a dirty one is ours from
        # here on: the kernel will not touch it, so selection.sh clears it after the standby's umount
        case "$(/tmp/abfatflag "$DEV")" in
        dirty*)
            echo "$(date) boot: $DEV was left dirty by a standby - clearing" >> $LOG
            /tmp/abfatflag "$DEV" clean && touch /tmp/ab_stick_owned
            ;;
        esac
        sync
        mount -o remount,rw /media
        # mounted rw again = dirty until unmounted, which the kernel would say itself if it could
        [ -f /tmp/ab_stick_owned ] && /tmp/abfatflag "$DEV" dirty
    fi
fi
touch /media/System/.session
sync
