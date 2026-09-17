#!/usr/bin/env bash
#
# The AutoBleem session on a Raspberry Pi: what rc/selection.sh is on the PlayStation Classic.
#
# AutoBleem::run()'s loop starts games and comes back in-process; the only thing that actually ends the
# process is the L2+R2 system menu's "RetroArch / EmulationStation" item, which writes AB_SELECTION=4 into
# rc/autobleem_cfg.sh (LaunchService::writeSelectionScript) on the way out. The console reboots at that point;
# a Pi has no reason to, so this loops instead: hand over to RetroArch, then come back to the launcher.
set -uo pipefail

DATA_MOUNT="${1:-/media/autobleem}"
APP_DIR="$DATA_MOUNT/Autobleem/bin/autobleem"
RC_DIR="$DATA_MOUNT/Autobleem/rc"
LOG_DIR="$DATA_MOUNT/System/Logs"

SEL_RETROARCH=4

mkdir -p "$LOG_DIR"

[ -x "$APP_DIR/autobleem-gui" ] || {
    echo "autobleem-session: no autobleem-gui in $APP_DIR" >&2
    exit 1
}

# The launcher's own logging is on stdout/stderr and systemd keeps that in the journal, but the console's
# AB_out.txt/AB_err.txt are what every AutoBleem instruction in the wild asks people for, so write them too.
while true; do
    cd "$APP_DIR" || exit 1

    # stdbuf keeps the tee'd copy as unbuffered as the app makes its own stdout, so a crash does not eat the
    # last lines - the same reason main.cpp sets ios::unitbuf
    ./autobleem-gui "$DATA_MOUNT" \
        > >(stdbuf -oL tee "$LOG_DIR/AB_out.txt") \
        2> >(stdbuf -oL tee "$LOG_DIR/AB_err.txt" >&2)
    status=$?
    echo "autobleem-session: autobleem-gui exited with $status"

    selection=""
    if [ -f "$RC_DIR/autobleem_cfg.sh" ]; then
        # the file is a tiny generated shell fragment: AB_SELECTION=n, AB_THEME=..., AB_PCSX=..., AB_MIP=...
        # shellcheck disable=SC1091
        . "$RC_DIR/autobleem_cfg.sh"
        selection="${AB_SELECTION:-}"
    fi
    echo "autobleem-session: selection=${selection:-none}"

    if [ "$selection" = "$SEL_RETROARCH" ] && [ -x "$RC_DIR/retroarch.sh" ]; then
        "$RC_DIR/retroarch.sh"
    fi

    sync

    # a launcher that dies instantly and forever would spin this loop as fast as the CPU allows
    sleep 1
done
