#!/usr/bin/env bash
#
# Runs once per boot, via autobleem-firstboot.service (WantedBy=multi-user.target), on a card written from
# an image tools/make_rpi_image.sh built - until AutoBleem's own install.sh has completed successfully once.
#
# This is the "first or second boot" story from docs/rpi-image-and-update-plan.md: Raspberry Pi Imager's own
# customisation (hostname, user, WiFi, SSH) runs earlier in the same boot (the service's
# After=multi-user.target), and a Pi with no working network yet on boot 1 just gets tried again on boot 2 -
# this script is idempotent, safe to run more than once, and re-arms itself by simply not disabling the
# service until it either succeeds or gives up after MAX_ATTEMPTS boots.
set -uo pipefail

IMAGE_DIR=/opt/autobleem-image
MARKER="$IMAGE_DIR/.done"
ATTEMPTS_FILE="$IMAGE_DIR/.attempts"
MAX_ATTEMPTS=20
SELF_SERVICE=autobleem-firstboot.service
PACKAGE="$IMAGE_DIR/autobleem-rpi.tar.gz"
UNPACK_DIR="$IMAGE_DIR/autobleem-rpi"

log() { printf 'autobleem-firstboot: %s\n' "$*"; }

disarm() {
    systemctl disable "$SELF_SERVICE" >/dev/null 2>&1 || true
}

# best-effort note in System/Logs, once the data partition exists to hold one - install.sh itself already
# logs everything it does to the journal, this is only for the "gave up" case an owner might not think to
# check journalctl for
note_in_data_logs() {
    local logdir
    for logdir in /media/*/System/Logs /media/autobleem/System/Logs; do
        [ -d "$logdir" ] || continue
        printf '%s\n' "$*" >>"$logdir/autobleem-firstboot.log" 2>/dev/null || true
    done
}

if [ -f "$MARKER" ]; then
    log "already applied - disabling myself"
    disarm
    exit 0
fi

if [ ! -f "$PACKAGE" ]; then
    log "no $PACKAGE - nothing staged to install, disabling myself"
    disarm
    exit 0
fi

attempts=0
[ -f "$ATTEMPTS_FILE" ] && attempts="$(cat "$ATTEMPTS_FILE" 2>/dev/null || echo 0)"
attempts=$((attempts + 1))
echo "$attempts" >"$ATTEMPTS_FILE"

if [ "$attempts" -gt "$MAX_ATTEMPTS" ]; then
    log "gave up after $((attempts - 1)) attempts - run install.sh by hand"
    note_in_data_logs "autobleem-firstboot gave up after $((attempts - 1)) attempts (no working network?). Run install.sh by hand: sudo bash $UNPACK_DIR/install.sh --yes (re-extract $PACKAGE first if $UNPACK_DIR is gone)"
    disarm
    exit 0
fi

log "attempt $attempts/$MAX_ATTEMPTS"

if [ ! -d "$UNPACK_DIR" ]; then
    log "unpacking $PACKAGE"
    mkdir -p "$IMAGE_DIR"
    if ! tar xzf "$PACKAGE" -C "$IMAGE_DIR"; then
        log "extract failed - will retry next boot"
        exit 1
    fi
fi

INSTALLER="$UNPACK_DIR/install.sh"
if [ ! -f "$INSTALLER" ]; then
    # tools/make_rpi_package.sh's tarball has autobleem-rpi/ as its single top-level entry, so extracting
    # into $IMAGE_DIR should always produce $UNPACK_DIR/install.sh directly - this only fires if that
    # layout ever changes, and retrying won't fix it.
    log "no install.sh under $UNPACK_DIR - package layout unexpected, giving up"
    note_in_data_logs "autobleem-firstboot: no install.sh under $UNPACK_DIR after extracting $PACKAGE - package layout unexpected"
    disarm
    exit 1
fi

log "running install.sh --yes"
if bash "$INSTALLER" --yes; then
    log "install.sh succeeded"
    touch "$MARKER"
    rm -f "$ATTEMPTS_FILE"
    disarm
    # the staged tarball/tree already did their job (install.sh copied everything onto the data partition) -
    # drop them rather than leaving a redundant copy on the root filesystem
    rm -rf "$UNPACK_DIR" "$PACKAGE"
    log "rebooting to finish - the HDMI mode and boot splash only take full effect on the next boot"
    sync
    reboot
else
    rc=$?
    log "install.sh failed (exit $rc) - will retry next boot"
    exit 1
fi
