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
mkdir -p /media/System/Logs

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
