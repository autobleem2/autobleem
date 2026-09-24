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
SEL_UPDATE=6      # the online update: the launcher downloaded it, autobleem-update applies it (install.sh --update)

mkdir -p "$LOG_DIR"

#*******************************
# hdmi_audio
#*******************************
# ALSA's "default" device is card 0, and on a Pi with a DualShock 4 plugged in that is the pad's own USB
# audio (its headphone jack) - everything went silent the first time. Point ALSA at the HDMI output the
# screen is on instead, system-wide, for everything started from here: autobleem-gui and pcsx-ab (SDL ->
# ALSA "default") and RetroArch (its alsa driver). Re-done on every start: the screen may move ports.
# A Pi's HDMI ports are ALSA cards of their own (vc4hdmi0/1); a PC's are pcm devices of its HDA card, one
# per connector, and which one has a screen with speakers behind it is in the codec's ELD - so two ways.
hdmi_audio() {
    if grep -q vc4hdmi /proc/asound/cards 2>/dev/null; then
        pi_hdmi_audio
    else
        pc_hdmi_audio
    fi
}

# write ALSA's system-wide default: a card, and a device on it when given ("!" because alsa.conf declares
# these as integers; the name form needs the redefinition)
set_alsa_default() { # set_alsa_default CARD [DEVICE]
    local conf
    conf="$(printf 'defaults.pcm.!card "%s"\ndefaults.ctl.!card "%s"' "$1" "$1")"
    [ -n "${2:-}" ] && conf="$conf$(printf '\ndefaults.pcm.!device %s' "$2")"
    if [ "$(cat /etc/asound.conf 2>/dev/null)" != "$conf" ]; then
        printf '%s\n' "$conf" > /etc/asound.conf.autobleem-tmp && mv -f /etc/asound.conf.autobleem-tmp /etc/asound.conf
        echo "autobleem-session: audio -> card $1${2:+ device $2}"
    fi
}

# a PC: the first HDMI/DisplayPort pin whose ELD says a monitor is present (the screen the picture is on,
# with speakers) - its pcm is the n-th "HDMI n" device of that card. No such pin (a screen without audio, a
# VGA monitor, a VM): ALSA's default stays, which is the analog output.
pc_hdmi_audio() {
    local card eld n p dev pcms
    for card in /proc/asound/card[0-9]*; do
        [ -d "$card" ] || continue
        n=0
        for eld in "$card"/eld#*; do
            [ -f "$eld" ] || continue
            if grep -qE 'monitor_present[[:space:]]+1' "$eld"; then
                # the HDMI pcm devices of this card, in device order
                pcms="$(for p in "$card"/pcm[0-9]*p; do
                            grep -q '^name: HDMI' "$p/info" 2>/dev/null && basename "$p"
                        done | sed 's/^pcm//; s/p$//' | sort -n)"
                dev="$(printf '%s\n' "$pcms" | sed -n "$((n + 1))p")"
                if [ -n "$dev" ]; then
                    set_alsa_default "${card##*/card}" "$dev"
                    return 0
                fi
            fi
            n=$((n + 1))
        done
    done
    echo "autobleem-session: no HDMI/DP screen with audio - leaving ALSA's default alone"
}

pi_hdmi_audio() {
    local card="" conn
    for conn in /sys/class/drm/card*-HDMI-A-1 /sys/class/drm/card*-HDMI-A-2; do
        [ -f "$conn/status" ] || continue
        if [ "$(cat "$conn/status")" = connected ]; then
            case "$conn" in
                *HDMI-A-1) card=vc4hdmi0 ;;
                *HDMI-A-2) card=vc4hdmi1 ;;
            esac
            break
        fi
    done
    # no connector reports a screen (or a driver without the status file): the first HDMI card ALSA lists
    if [ -z "$card" ] || ! grep -q "\[$card" /proc/asound/cards; then
        card="$(awk '/vc4hdmi/ { gsub(/[^a-z0-9]/, "", $2); print $2; exit }' /proc/asound/cards)"
    fi
    [ -n "$card" ] || { echo "autobleem-session: no HDMI audio card - leaving ALSA's default alone"; return 0; }
    set_alsa_default "$card"
}

#*******************************
# boot_splash_down
#*******************************
# The boot splash (plymouth, the AutoBleem logo - see install.sh's install_boot_splash) holds the DRM
# device while it runs, and SDL needs to be the DRM master to draw, so it has to go before the launcher
# starts. --retain-splash leaves its last frame on the screen until SDL's first modeset, so what the user
# sees is the logo, then the launcher's own splash, and no text console in between. The service file keeps
# plymouth-quit.service from doing this earlier. Done before the binary check too: a session that bails out
# would otherwise leave the logo up forever with no way to tell what went wrong.
boot_splash_down() {
    command -v plymouth >/dev/null 2>&1 || return 0
    plymouth --ping 2>/dev/null || return 0
    plymouth quit --retain-splash
    echo "autobleem-session: boot splash taken down"
}

boot_splash_down

[ -x "$APP_DIR/autobleem-gui" ] || {
    echo "autobleem-session: no autobleem-gui in $APP_DIR" >&2
    exit 1
}

# The launcher's own logging is on stdout/stderr and systemd keeps that in the journal, but the console's
# AB_out.txt/AB_err.txt are what every AutoBleem instruction in the wild asks people for, so write them too.
while true; do
    cd "$APP_DIR" || exit 1
    hdmi_audio

    # stdbuf keeps the tee'd copy as unbuffered as the app makes its own stdout, so a crash does not eat the
    # last lines - the same reason main.cpp sets ios::unitbuf
    ./autobleem-gui "$DATA_MOUNT" \
        > >(stdbuf -oL tee "$LOG_DIR/AB_out.txt") \
        2> >(stdbuf -oL tee "$LOG_DIR/AB_err.txt" >&2)
    status=$?
    echo "autobleem-session: autobleem-gui exited with $status"

    selection=""
    if [ -f "$RC_DIR/autobleem_cfg.sh" ]; then
        # the file is a tiny generated shell fragment: AB_SELECTION=n, AB_THEME=..., AB_PCSX=...
        # shellcheck disable=SC1091
        . "$RC_DIR/autobleem_cfg.sh"
        selection="${AB_SELECTION:-}"
    fi
    echo "autobleem-session: selection=${selection:-none}"

    if [ "$selection" = "$SEL_RETROARCH" ] && [ -x "$RC_DIR/retroarch.sh" ]; then
        "$RC_DIR/retroarch.sh"
    elif [ "$selection" = "$SEL_UPDATE" ]; then
        if command -v autobleem-update >/dev/null 2>&1; then
            autobleem-update "$DATA_MOUNT"
        else
            echo "autobleem-session: no autobleem-update on this system - re-run install.sh from a package once" >&2
        fi
        # a stale selection must not run the update again on the next pass
        rm -f "$RC_DIR/autobleem_cfg.sh"
    fi

    sync

    # a launcher that dies instantly and forever would spin this loop as fast as the CPU allows
    sleep 1
done
