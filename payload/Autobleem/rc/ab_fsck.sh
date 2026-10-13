#!/bin/sh
#
# AUTOBLEEM-29: the stick's filesystem check with a progress screen. Sourced (from its tmpfs copy, /tmp/ab_fsck.sh -
# boot.sh copies it there with the tools) by checkstick.sh at boot and by selection.sh when a stick pulled while the
# launcher ran comes back. The caller unmounts the stick first and mounts it again after; this only checks:
#
#   ab_fsck DEV      checks and repairs DEV (an unmounted partition) by its filesystem: FAT with fsck.fat (our build,
#                    which reports its progress), exFAT with fsck.exfat, ext2/3/4 with e2fsck (-C: its own progress).
#                    The check-stick picture is up meanwhile with the bar filled from /tmp/abfsck.pct. Returns 0 when
#                    the filesystem is clean or was repaired, 1 when the check failed or could not run. What it did
#                    is in /tmp/abfsck.log (the caller appends it to System/Logs/fsck.log once the stick is mounted).
#   ab_fsck_copy     copies the tools and the pictures to tmpfs (/tmp/abfsck/) - while the stick is mounted
#
# The tools are static armhf builds (Autobleem/bin/fsck/, built once - README there); the console's own /sbin/e2fsck
# (Sony's 1.43) is the fallback for ext. Nothing on the console's own storage is written.

ABFSCK_DIR=/tmp/abfsck
ABFSCK_PCT=/tmp/abfsck.pct
ABFSCK_LOG=/tmp/abfsck.log
ABFSCK_FLAG=/tmp/.abfsck

ab_fsck_copy() {
    mkdir -p $ABFSCK_DIR
    for t in fsck.fat fsck.exfat e2fsck; do
        cp -f /media/Autobleem/bin/fsck/$t $ABFSCK_DIR/$t 2> /dev/null && chmod +x $ABFSCK_DIR/$t
    done
}

# the filesystem type blkid reports for DEV (vfat, exfat, ext4 ...)
ab_fsck_type() {
    blkid "$1" 2> /dev/null | sed -n 's/.* TYPE="\([^"]*\)".*/\1/p' | head -1
}

# the bar for a tool that reports nothing (fsck.exfat): from 5 towards 90 % over time, in the background
ab_fsck_ticker() {
    p=5
    while [ -f $ABFSCK_FLAG ]; do
        echo $p > $ABFSCK_PCT.tmp && mv -f $ABFSCK_PCT.tmp $ABFSCK_PCT
        [ $p -lt 90 ] && p=$((p + (90 - p) / 12 + 1))
        sleep 1
    done
}

ab_fsck() {
    dev=$1
    type=$(ab_fsck_type "$dev")
    echo "$(date) check of $dev (${type:-unknown filesystem}) - uptime $(cut -d' ' -f1 /proc/uptime)" >> $ABFSCK_LOG
    echo 0 > $ABFSCK_PCT
    touch $ABFSCK_FLAG
    if [ -x /tmp/absplash ] && [ -f /tmp/check-stick.jpg ]; then
        # the bar's box in the picture's own pixels (Kasia's bar.json: 1920x1080 and the 720x480 twin)
        LD_LIBRARY_PATH=/tmp/lib /tmp/absplash /tmp/check-stick.jpg --until-gone $ABFSCK_FLAG --timeout 3600 \
            --progress-file $ABFSCK_PCT --bar 484,824,952,14 --bar-4x3 124,366,471,8 --bar-color 36D9E0 \
            > /tmp/absplash-fsck.log 2>&1 &
    fi
    st=8
    case "$type" in
    vfat | msdos | fat)
        if [ -x $ABFSCK_DIR/fsck.fat ]; then
            AB_FSCK_PROGRESS=$ABFSCK_PCT $ABFSCK_DIR/fsck.fat -a "$dev" >> $ABFSCK_LOG 2>&1 < /dev/null
            st=$?
            # 0 nothing found, 1 found and repaired (-a); 2 and up: not checked
            [ $st -le 1 ] && st=0
        else
            echo "no fsck.fat on the stick (Autobleem/bin/fsck)" >> $ABFSCK_LOG
        fi
        ;;
    exfat)
        if [ -x $ABFSCK_DIR/fsck.exfat ]; then
            ab_fsck_ticker &
            $ABFSCK_DIR/fsck.exfat -y "$dev" >> $ABFSCK_LOG 2>&1 < /dev/null
            st=$?
            [ $st -le 1 ] && st=0 # 0 clean, 1 corrected
        else
            echo "no fsck.exfat on the stick (Autobleem/bin/fsck)" >> $ABFSCK_LOG
        fi
        ;;
    ext2 | ext3 | ext4)
        E2=$ABFSCK_DIR/e2fsck
        [ -x $E2 ] || E2=/sbin/e2fsck
        if [ -x $E2 ]; then
            # -C 3: "pass current max device" lines on fd 3 (into the pipe; 0 would be a text bar), its messages to
            # the log; the passes' share of the time as e2fsck's own bar weighs them
            { $E2 -p -C 3 "$dev" 3>&1 >> $ABFSCK_LOG 2>&1 < /dev/null; echo "status $?" > /tmp/abfsck.st; } |
                awk -v f=$ABFSCK_PCT '
                    BEGIN { split("0 70 90 92 97 100", start, " ") }
                    $1 ~ /^[1-5]$/ && $3 > 0 {
                        p = start[$1] + (start[$1 + 1] - start[$1]) * $2 / $3
                        if (int(p) != last) { last = int(p); print last > f; close(f) }
                        next
                    }
                    { print >> "'$ABFSCK_LOG'" }'
            st=$(sed -n 's/^status //p' /tmp/abfsck.st 2> /dev/null)
            rm -f /tmp/abfsck.st
            [ -n "$st" ] || st=8
            [ $st -le 2 ] && st=0 # 0 clean, 1 corrected, 2 corrected (reboot advised - not mounted here)
        else
            echo "no e2fsck" >> $ABFSCK_LOG
        fi
        ;;
    *)
        echo "no check for this filesystem" >> $ABFSCK_LOG
        ;;
    esac
    echo 100 > $ABFSCK_PCT
    sleep 1 # the full bar, seen
    rm -f $ABFSCK_FLAG
    echo "$(date) check of $dev ended: $([ $st = 0 ] && echo clean or repaired || echo "FAILED (status $st)")" >> $ABFSCK_LOG
    [ $st = 0 ]
}
