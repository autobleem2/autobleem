#!/usr/bin/env bash
#
# The online update's apply step on a Raspberry Pi - what autobleem-session runs when the launcher left
# with AB_SELECTION=6 (MENU_OPTION_UPDATE): the launcher has already downloaded and sha256-checked the new
# package(s) into <data>/System/Updates/ and written pending.json there (UpdateService); this unpacks the
# AutoBleem package and re-runs its install.sh in update mode - the same first-boot progress screen
# (autobleem-install-ui.py: logo, a bar per phase, the last lines) on tty1, the launcher's own console -
# and/or installs the RetroArch build from its tarball. Installed as /usr/local/bin/autobleem-update by
# install.sh, together with the screen's script and the splash under /usr/local/share/autobleem/.
#
#   autobleem-update [DATA_MOUNT]        (default /media/autobleem)
#
# Everything is logged to <data>/System/Logs/update.log. A failed update leaves the downloads in place -
# the launcher offers the same update again and the files already there are not fetched twice.
set -uo pipefail

DATA_MOUNT="${1:-/media/autobleem}"
UPDATES="$DATA_MOUNT/System/Updates"
PENDING="$UPDATES/pending.json"
LOG="$DATA_MOUNT/System/Logs/update.log"
SHARE=/usr/local/share/autobleem
UI="$SHARE/autobleem-install-ui.py"
LOGO="$SHARE/splash.png"
INSTALLER_COPY="$SHARE/installer/install.sh"   # the installed release's own installer, for a RetroArch-only update
# the package is unpacked on the root filesystem, not the data partition: exFAT cannot take the archive's
# ownership (tar, run as root, restores uid/gid by default and exits 2 on "Operation not permitted") nor
# its modes; the first real update (2026-09-20) died right there
STAGE=/var/tmp/autobleem-update
TTY=/dev/tty1

mkdir -p "$(dirname "$LOG")"
log() { printf '%s autobleem-update: %s\n' "$(date '+%F %T')" "$*" | tee -a "$LOG"; }

[ -f "$PENDING" ] || { log "nothing pending in $UPDATES"; exit 0; }

read -r ab_version ab_file ra_version ra_file < <(python3 - "$PENDING" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
print(d.get("autobleem_version", "") or "-", d.get("autobleem_file", "") or "-",
      d.get("retroarch_version", "") or "-", d.get("retroarch_file", "") or "-")
PY
)
[ "$ab_file" = "-" ] && ab_file=""
[ "$ra_file" = "-" ] && ra_file=""
log "pending: AutoBleem ${ab_version} (${ab_file:-no package}), RetroArch ${ra_version} (${ra_file:-no build})"

# The screen: the first boot's program, drawing the way the first boot chose - /etc/autobleem/installer-ui
# says gfx (the logo and bars on the framebuffer) or text (the same as text on the console); a missing
# file means gfx, and gfx without a framebuffer means text. Plain output only without the program.
UI_MODE_FILE=/etc/autobleem/installer-ui
UI_OK=0
BACKEND=fb
if [ -f "$UI" ] && command -v python3 >/dev/null 2>&1; then
    UI_OK=1
    [ "$(tr -d '[:space:]' <"$UI_MODE_FILE" 2>/dev/null)" = text ] && BACKEND=text
    [ "$BACKEND" = fb ] && [ ! -e /dev/fb0 ] && BACKEND=text
fi
ui() { python3 "$UI" --backend "$BACKEND" --logo "$LOGO" --tty "$TTY" --backtitle "AutoBleem - update" "$@"; }

# what to run: the new package's install.sh over the unpacked tree, or the installed release's for a
# RetroArch-only update
installer=""
if [ -n "$ab_file" ]; then
    [ -f "$UPDATES/$ab_file" ] || { log "missing $UPDATES/$ab_file"; exit 1; }
    rm -rf "$STAGE"
    mkdir -p "$STAGE"
    log "unpacking $ab_file"
    if ! tar --no-same-owner -xzf "$UPDATES/$ab_file" -C "$STAGE"; then
        log "could not unpack $ab_file"
        rm -rf "$STAGE"
        exit 1
    fi
    installer="$(find "$STAGE" -maxdepth 2 -name install.sh | head -1)"
    [ -n "$installer" ] || { log "no install.sh in $ab_file"; rm -rf "$STAGE"; exit 1; }
elif [ -n "$ra_file" ]; then
    installer="$INSTALLER_COPY"
    [ -f "$installer" ] || { log "no installer copy at $installer - re-run install.sh from a package once"; exit 1; }
fi
[ -n "$installer" ] || { log "nothing to do"; exit 0; }

args=(--update)
if [ -n "$ra_file" ]; then
    [ -f "$UPDATES/$ra_file" ] || { log "missing $UPDATES/$ra_file"; exit 1; }
    args+=(--retroarch-tarball "$UPDATES/$ra_file")
fi

log "running $installer ${args[*]}"
if [ "$UI_OK" -eq 1 ]; then
    AB_UI_MARKERS=1 bash "$installer" "${args[@]}" 2>&1 | tee -a "$LOG" | ui --text-mode progress
    status="${PIPESTATUS[0]}"
else
    bash "$installer" "${args[@]}" 2>&1 | tee -a "$LOG"
    status="${PIPESTATUS[0]}"
fi

if [ "$status" -eq 0 ]; then
    log "update done: AutoBleem ${ab_version}, RetroArch ${ra_version}"
    rm -rf "$UPDATES" "$STAGE"
    sync
else
    log "install.sh failed with $status - the downloads stay in $UPDATES for another try"
    rm -rf "$STAGE"
    [ "$UI_OK" -eq 1 ] && ui --text-mode message --title "Update failed" \
        --text "install.sh returned $status - see System/Logs/update.log" --wait 6
fi
exit "$status"
