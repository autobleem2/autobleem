#!/usr/bin/env bash
#
# Runs once per boot, via autobleem-firstboot.service (WantedBy=multi-user.target), on a card written from
# an image tools/make_rpi_image.sh built - or a stick from tools/make_pc_image.sh's - until AutoBleem's own
# install.sh has completed successfully once. The platform (a Pi, or the PC stick) decides where the boot
# partition with autobleem.txt is and how the WiFi country is set; everything else is the same.
#
# The service hands this script a virtual terminal (tty8, switched onto the screen here), so the first boot
# is something the user watches and can answer, not a silent background job: it waits for network, and when there is none it asks
# for a WiFi network and password right here (the first real boot of the image had no network, install.sh
# failed at apt and nobody could tell why without ssh - which needs the network). Then it runs install.sh
# with the options from autobleem.txt on the boot partition, and reboots once that has succeeded.
# Everything is drawn by autobleem-install-ui.py - graphically on the framebuffer, or as text on the
# console in raspi-config's look: the PC stick's first boot asks which (choose_ui_mode), and the answer is
# kept in /etc/autobleem/installer-ui for the launcher's online updates (autobleem-update).
#
# This is the "first or second boot" story from docs/rpi-image-and-update-plan.md: Raspberry Pi Imager's own
# customisation (hostname, user, WiFi, SSH - cloud-init on this image) has run earlier in the same boot, so
# with WiFi preset here nothing is asked. A run that cannot finish (the Pi was switched off at the network
# question, install.sh failed) simply happens again on the next boot - the script is idempotent and re-arms itself by
# not disabling the service until it either succeeds or gives up after MAX_ATTEMPTS boots.
set -uo pipefail

IMAGE_DIR=/opt/autobleem-image
MARKER="$IMAGE_DIR/.done"
ATTEMPTS_FILE="$IMAGE_DIR/.attempts"
MAX_ATTEMPTS=20
SELF_SERVICE=autobleem-firstboot.service
# the staged package: autobleem-rpi.tar.gz (a Pi) or autobleem-pcusb-i386.tar.gz (the PC stick), one of
# them, its top directory named after the platform (tools/make_rpi_package.sh)
PACKAGE="$(ls "$IMAGE_DIR"/autobleem-*.tar.gz 2>/dev/null | head -1)"
PACKAGE="${PACKAGE:-$IMAGE_DIR/autobleem-rpi.tar.gz}"
case "$(basename "$PACKAGE")" in
    autobleem-pcusb*) PLATFORM=pcusb; UNPACK_DIR="$IMAGE_DIR/autobleem-pcusb" ;;
    *)                PLATFORM=rpi;   UNPACK_DIR="$IMAGE_DIR/autobleem-rpi" ;;
esac
# where autobleem.txt is: a Pi's firmware partition, the PC image's ESP (written there by the image builder
# from payload_linux/system/autobleem.txt or autobleem-pc.txt; editable from any PC, both are FAT)
if [ "$PLATFORM" = pcusb ]; then
    BOOT_DIR=/boot/efi
else
    BOOT_DIR=/boot/firmware
fi
OPTIONS_FILE="$BOOT_DIR/autobleem.txt"
# how the dialogs name this computer - a PC user must never read "Pi"
if [ "$PLATFORM" = pcusb ]; then MACHINE="this PC"; else MACHINE="this Pi"; fi

# The script's stdout is its console (tty8, see the service file). Once a screen is up (choose_ui_mode)
# nothing is printed there any more - the screen's program owns it; the journal and the install log
# get every line either way.
INSTALL_LOG=/var/log/autobleem-firstboot-install.log   # install.sh's output, for a look after the fact (ssh)
log() {
    [ "$UI_MODE" = plain ] && printf '\033[1;32m==>\033[0m %s\n' "$*"
    printf '==> %s\n' "$*" >>"$INSTALL_LOG" 2>/dev/null || true
    logger -t autobleem-firstboot -- "$*" 2>/dev/null || true
}
warn() {
    [ "$UI_MODE" = plain ] && printf '\033[1;33m[!]\033[0m %s\n' "$*"
    printf '[!] %s\n' "$*" >>"$INSTALL_LOG" 2>/dev/null || true
    logger -t autobleem-firstboot -p user.warning -- "$*" 2>/dev/null || true
}

disarm() {
    systemctl disable "$SELF_SERVICE" >/dev/null 2>&1 || true
}

# The unit runs on tty8 (see the service file); the screen is switched to it here, and back to tty1 - the
# login prompt, untouched all along - by a run that ends without a reboot.
show_our_tty() {
    chvt 8 >/dev/null 2>&1 || true
}
give_tty_back() {
    chvt 1 >/dev/null 2>&1 || true
}

# the screen's program leaves tty8 in graphics mode (the success path reboots from the picture) or, in
# text mode, painted blue; the failure path and the plain prompts want the bare console back
# (KDSETMODE = 0x4B3A, KD_TEXT = 0)
text_mode() {
    python3 - <<'PY' 2>/dev/null || true
import fcntl, os
f = os.open("/dev/tty8", os.O_RDWR)
fcntl.ioctl(f, 0x4B3A, 0)
PY
    printf '\033[0m\033[2J\033[H\033[?25h' >/dev/tty8 2>/dev/null || true
}

#*******************************
# the screen: dialogs
#*******************************
# Every question and every wait goes through these. The screen's program draws them - as pixels on the
# framebuffer under the logo (UI_MODE=gfx), or as text on the console in raspi-config's look (UI_MODE=
# text) - and reads the keys from the console; without it they are plain prompts (UI_MODE=plain). Which
# one is choose_ui_mode's decision; a program that fails drops to the next simpler one (downgrade_ui).
# The console stays in the screen's hands from the first dialog to the reboot; text_mode() brings the
# bare console back for a failure message.
UI="$IMAGE_DIR/autobleem-install-ui.py"
UI_MODE=plain
# The screen the user chose, kept for the updates that follow: the launcher's online update re-runs the
# installer through autobleem-update, which draws with the same screen (nobody is asked then). gfx or
# text; a missing file means gfx with the text screen as the fallback.
UI_MODE_FILE=/etc/autobleem/installer-ui
BACKTITLE="AutoBleem - first boot setup"

ui() {
    local backend=fb
    [ "$UI_MODE" = text ] && backend=text
    python3 "$UI" --backend "$backend" --logo "$IMAGE_DIR/splash.png" --tty /dev/tty8 --backtitle "$BACKTITLE" "$@"
}
ui_available() { [ -f "$UI" ] && command -v python3 >/dev/null 2>&1; }

set_ui_mode() {
    UI_MODE="$1"
    mkdir -p "$(dirname "$UI_MODE_FILE")" 2>/dev/null || true
    printf '%s\n' "$1" >"$UI_MODE_FILE" 2>/dev/null || true
}

# the screen's program failed (exit code other than 0 / 3): the next simpler screen. A graphical screen
# that fails here would fail the same way for an update, so text is what gets remembered.
downgrade_ui() {
    case "$UI_MODE" in
        gfx)  warn "the graphical screen failed - the text screen from here on"; set_ui_mode text ;;
        text) warn "the text screen failed - plain prompts from here on"; UI_MODE=plain; text_mode ;;
    esac
}

# ui_message TITLE [LINE...] [--wait S]: shown, and left on the screen while the script works
ui_message() {
    local title="$1"; shift
    local -a lines=() extra=()
    while [ $# -gt 0 ]; do
        case "$1" in --wait) extra+=(--wait "$2"); shift 2 ;; *) lines+=(--text "$1"); shift ;; esac
    done
    while [ "$UI_MODE" != plain ]; do
        ui message --title "$title" "${lines[@]}" "${extra[@]}" 2>>"$INSTALL_LOG" && return 0
        downgrade_ui
    done
    printf '\n   %s\n' "$title"
    local l
    for l in "${lines[@]}"; do [ "$l" = --text ] || printf '   %s\n' "$l"; done
    [ ${#extra[@]} -gt 0 ] && sleep "${extra[1]}"
    return 0
}

# ui_menu TITLE [LINE...] -- KEY=LABEL... [--default KEY] [--timeout S]: prints the chosen key
ui_menu() {
    local title="$1"; shift
    local -a lines=() items=() extra=()
    local default="" timeout=0
    while [ $# -gt 0 ] && [ "$1" != -- ]; do lines+=(--text "$1"); shift; done
    [ "${1:-}" = -- ] && shift
    while [ $# -gt 0 ]; do
        case "$1" in
            --default) default="$2"; extra+=(--default "$2"); shift 2 ;;
            --timeout) timeout="$2"; extra+=(--timeout "$2"); shift 2 ;;
            *) items+=("$1"); shift ;;
        esac
    done
    local -a args=()
    local it rc
    for it in "${items[@]}"; do args+=(--item "$it"); done
    while [ "$UI_MODE" != plain ]; do
        ui menu --title "$title" "${lines[@]}" "${args[@]}" "${extra[@]}" 2>>"$INSTALL_LOG"
        rc=$?
        [ "$rc" -eq 0 ] || [ "$rc" -eq 3 ] && return 0
        downgrade_ui
    done
    printf '\n   %s\n' "$title"
    local l
    for l in "${lines[@]}"; do [ "$l" = --text ] || printf '   %s\n' "$l"; done
    printf '\n'
    for it in "${items[@]}"; do printf '   %3s) %s\n' "${it%%=*}" "${it#*=}"; done
    local choice=""
    if [ "$timeout" -gt 0 ]; then
        read -rt "$timeout" -p "   Your choice${default:+ [$default]}: " choice || true
        printf '\n'
    else
        read -rp "   Your choice${default:+ [$default]}: " choice
    fi
    printf '%s\n' "${choice:-$default}"
}

# ui_input TITLE [LINE...] [--secret] [--default V]: prints what was typed
ui_input() {
    local title="$1"; shift
    local -a lines=() extra=()
    local secret=0 default=""
    while [ $# -gt 0 ]; do
        case "$1" in
            --secret) secret=1; extra+=(--secret); shift ;;
            --default) default="$2"; extra+=(--default "$2"); shift 2 ;;
            *) lines+=(--text "$1"); shift ;;
        esac
    done
    local rc
    while [ "$UI_MODE" != plain ]; do
        ui input --title "$title" "${lines[@]}" "${extra[@]}" 2>>"$INSTALL_LOG"
        rc=$?
        [ "$rc" -eq 0 ] || [ "$rc" -eq 3 ] && return 0
        downgrade_ui
    done
    printf '\n   %s\n' "$title"
    local l
    for l in "${lines[@]}"; do [ "$l" = --text ] || printf '   %s\n' "$l"; done
    local value=""
    if [ "$secret" -eq 1 ]; then
        read -rsp "   > " value; printf '\n'
    else
        read -rp "   > ${default:+[$default] }" value
    fi
    printf '%s\n' "${value:-$default}"
}

# Which screen: autobleem.txt's installer= when set, else - on the PC stick - a question, asked with the
# text screen on every attempt (a graphical screen that stays black is exactly the case where the user
# reboots and wants to be asked again; a fresh unattended boot takes the recommended graphical one after
# the timeout); a Pi takes what was chosen before, else the graphical screen, which works as it is there.
# The choice is written to UI_MODE_FILE for the updates that follow.
choose_ui_mode() {
    local stored="" want="" answer
    [ -f "$UI_MODE_FILE" ] && stored="$(tr -d '[:space:]' <"$UI_MODE_FILE")"
    case "$stored" in gfx|text) ;; *) stored="" ;; esac
    case "${OPT[installer]:-}" in
        gfx|graphical|graphics) want=gfx ;;
        text|tui)               want=text ;;
        "")                     ;;
        *) warn "autobleem.txt: installer=${OPT[installer]} is not gfx or text - ignored" ;;
    esac
    if [ -z "$want" ]; then
        if [ "$PLATFORM" = pcusb ] && ui_available; then
            UI_MODE=text
            answer="$(ui_menu "How should the setup be shown?" \
                "The setup can draw its screens graphically - the AutoBleem logo with the questions and" \
                "the progress under it - or as text, the way this window is. Graphical is recommended;" \
                "choose Text if the graphical screen stays black or garbled on $MACHINE. The choice is" \
                "kept, and the updates that follow use the same screen." \
                -- "g=Graphical (recommended)" "t=Text" --default "$([ "$stored" = text ] && echo t || echo g)" --timeout 30)"
            case "$answer" in t|T) want=text ;; *) want=gfx ;; esac
        else
            want="${stored:-gfx}"
        fi
    fi
    set_ui_mode "$want"
    log "screen: $UI_MODE"
    # what this machine can actually draw with: the graphical screen needs a framebuffer (not remembered
    # - the file keeps the choice, not the hardware), either needs the program and python3
    if [ "$UI_MODE" = gfx ] && [ ! -c /dev/fb0 ]; then
        warn "no /dev/fb0 - the text screen instead"
        UI_MODE=text
    fi
    ui_available || UI_MODE=plain
}

# a run that ends here without a reboot: the reason on the screen, the bare console and tty1 back, and
# the next boot tries again
bail() {
    local title="$1"; shift
    ui_message "$title" "$@" "It will try again on the next boot." \
        "The details are in $INSTALL_LOG (log in on another console)." --wait 8
    text_mode
    give_tty_back
    exit 1
}

# best-effort note in System/Logs, once the data partition exists to hold one
note_in_data_logs() {
    local logdir
    for logdir in /media/*/System/Logs /media/autobleem/System/Logs; do
        [ -d "$logdir" ] || continue
        printf '%s\n' "$*" >>"$logdir/autobleem-firstboot.log" 2>/dev/null || true
    done
}

#*******************************
# options: autobleem.txt on the boot partition
#*******************************
# key=value lines, # comments; edited from any PC (FAT), so CRLF and BOM are tolerated. Keys are what
# payload_linux/system/autobleem.txt documents: root_gib, hdmi_mode, retroarch, thumbnails, bios, downloads, samples.
# WiFi is deliberately not here - Raspberry Pi Imager's customisation and the boot partition's own
# network-config (cloud-init) already cover "preset WiFi"; this script only asks when neither did.
declare -A OPT=()
read_options() {
    [ -f "$OPTIONS_FILE" ] || return 0
    local line key value
    while IFS= read -r line || [ -n "$line" ]; do
        line="${line%$'\r'}"
        line="${line#$'\xEF\xBB\xBF'}"
        line="${line%%#*}"
        [[ "$line" == *=* ]] || continue
        key="${line%%=*}"; value="${line#*=}"
        key="$(echo "$key" | tr -d '[:space:]' | tr 'A-Z' 'a-z')"
        value="$(echo "$value" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')"
        [ -n "$key" ] && OPT["$key"]="$value"
    done <"$OPTIONS_FILE"
}

# RetroArch is optional - it is the slow, heavy half of the install (10-40 minutes of building, close to a
# GB of cores, assets and BIOS files), and without it AutoBleem is still complete for PS1 games. Asked on
# the screen unless autobleem.txt already says (retroarch=source|apt|none). A minute with no answer means
# yes: someone who preset everything in Raspberry Pi Imager and walked away gets the full install.
ask_retroarch() {
    local v="${OPT[retroarch]:-}"
    case "$v" in
        prebuilt|source|apt|none) return 0 ;;
        ""|ask) ;;
        *) warn "autobleem.txt: retroarch=$v is not prebuilt, source, apt or none - asking instead" ;;
    esac
    local answer
    answer="$(ui_menu "Install RetroArch too?" \
        "AutoBleem plays PlayStation games on its own. RetroArch adds the other systems - NES, SNES," \
        "Mega Drive, arcade and about a hundred more: a ready-made build is downloaded (or built here," \
        "10-40 minutes, if the download site cannot be reached), plus close to a GB of cores, databases" \
        "and BIOS files. It can be added later by running the installer again." \
        -- "y=Yes, install RetroArch" "n=No, PlayStation only" --default y --timeout 60)"
    case "$answer" in
        n|N|no|NO|No) OPT[retroarch]=none; log "RetroArch: no - a PS1-only AutoBleem" ;;
        *)            OPT[retroarch]=prebuilt; log "RetroArch: yes" ;;
    esac
}

# the install.sh arguments the options translate to. A PS1-only install skips the cores/assets download
# (nothing would run them) - install.sh itself keeps the PS1 box art and trims the BIOS pack to the two
# PS1 files when --retroarch none.
install_args() {
    local args=(--yes)
    local v
    v="${OPT[root_gib]:-8}";      [ "$v" != 0 ] && [ "$v" != none ] && args+=(--grow-root "$v")
    v="${OPT[hdmi_mode]:-}";      [ -n "$v" ] && args+=(--hdmi-mode "$v")
    v="${OPT[retroarch]:-}";      [ -n "$v" ] && args+=(--retroarch "$v")
    v="${OPT[repo]:-}";           [ -n "$v" ] && args+=(--repo "$v")
    v="${OPT[thumbnails]:-}";     [ -n "$v" ] && args+=(--thumbnails "$v")
    v="${OPT[bios]:-yes}";        case "$v" in no|false|0) args+=(--no-bios) ;; esac
    v="${OPT[samples]:-yes}";     case "$v" in no|false|0) args+=(--no-samples) ;; esac
    v="${OPT[downloads]:-yes}"
    if [ "${OPT[retroarch]:-}" = none ]; then v=no; fi
    case "$v" in no|false|0) args+=(--no-downloads) ;; esac
    printf '%s\n' "${args[@]}"
}

#*******************************
# network
#*******************************
# "online" means a real fetch works, not just an interface with an address: DNS plus one HTTP round trip to
# the host install.sh's apt downloads from first.
is_online() {
    getent hosts deb.debian.org >/dev/null 2>&1 || return 1
    wget -q --spider --timeout=5 --tries=1 https://deb.debian.org/ >/dev/null 2>&1
}

# waits up to $1 seconds, printing a dot a second (with plain prompts; a screen shows its own message)
wait_for_network() {
    local secs="${1:-30}" i
    for ((i = 0; i < secs; i++)); do
        is_online && { [ "$UI_MODE" = plain ] && printf '\n'; return 0; }
        [ "$UI_MODE" = plain ] && printf '.'
        sleep 1
    done
    [ "$UI_MODE" = plain ] && printf '\n'
    return 1
}

# NTP: apt refuses repository metadata "from the future"/"not valid yet" on a badly wrong clock, and a Pi
# has no battery clock - the first boot's clock is whatever the image was built at until NTP corrects it
wait_for_clock() {
    local i
    for ((i = 0; i < 30; i++)); do
        [ "$(timedatectl show -p NTPSynchronized --value 2>/dev/null)" = yes ] && return 0
        sleep 1
    done
    return 1
}

has_wifi_device() {
    nmcli -t -f TYPE device 2>/dev/null | grep -qx wifi
}

# the regulatory country: whatever is already set (Imager/the wizard/raspi-config put it on the kernel
# command line - a Pi's cmdline.txt first, it is the current setting, /proc/cmdline is what this boot
# started with; the PC stick's is in modprobe.d), else the locale's territory, else nothing - the prompt then asks
wifi_country_default() {
    local cc
    cc="$(cat "$BOOT_DIR/cmdline.txt" /proc/cmdline 2>/dev/null | tr ' ' '\n' \
        | sed -n 's/^cfg80211\.ieee80211_regdom=//p' | head -1)"
    [ -n "$cc" ] && { echo "$cc"; return 0; }
    cc="$(sed -n 's/^options cfg80211 ieee80211_regdom=//p' /etc/modprobe.d/cfg80211.conf 2>/dev/null | head -1)"
    [ -n "$cc" ] && { echo "$cc"; return 0; }
    cc="$(sed -n 's/^LANG=[a-z]*_\([A-Z][A-Z]\).*/\1/p' /etc/default/locale 2>/dev/null | head -1)"
    echo "${cc:-}"
}

# Raspberry Pi OS keeps WiFi soft-blocked (rfkill) until a country is set - that is why a Lite image whose
# first-boot wizard skipped the WiFi step has no WiFi at all. raspi-config knows how to set it persistently
# on a Pi; a plain Debian takes it as a cfg80211 module option, kept in modprobe.d for the next boots.
set_wifi_country() {
    local cc="$1"
    if [ "$PLATFORM" = rpi ] && command -v raspi-config >/dev/null 2>&1; then
        raspi-config nonint do_wifi_country "$cc" >/dev/null 2>&1 || true
    else
        printf 'options cfg80211 ieee80211_regdom=%s\n' "$cc" > /etc/modprobe.d/cfg80211.conf 2>/dev/null || true
    fi
    iw reg set "$cc" >/dev/null 2>&1 || true
    rfkill unblock wifi >/dev/null 2>&1 || true
    nmcli radio wifi on >/dev/null 2>&1 || true
}

# one line per network, strongest first, each SSID once: "SIGNAL<TAB>SECURITY<TAB>SSID". SSID is asked for
# last so a ':' inside it cannot be mistaken for nmcli's field separator (and its escaped '\:' is undone).
wifi_scan() {
    nmcli -t -f SIGNAL,SECURITY,SSID device wifi list --rescan yes 2>/dev/null \
        | awk -F: '{
            sig = $1; sec = $2; ssid = $3
            for (i = 4; i <= NF; i++) ssid = ssid ":" $i
            gsub(/\\:/, ":", ssid)
            if (length(ssid) && !seen[ssid]++) print sig "\t" sec "\t" ssid
        }' | sort -rn
}

# The screen-and-keyboard WiFi setup. Returns 0 once online - there is no other way out.
interactive_network() {
    local cc choice ssid sec sig pass hidden out
    local -a nets items

    log "No network connection."
    if has_wifi_device; then
        cc="$(wifi_country_default)"
        choice="$(ui_input "WiFi country" "Two letters, e.g. GB, PL, US - WiFi stays blocked until one is set." \
            ${cc:+--default "$cc"})"
        cc="${choice:-$cc}"
        cc="$(echo "$cc" | tr 'a-z' 'A-Z' | tr -cd 'A-Z')"
        if [ -n "$cc" ]; then
            set_wifi_country "$cc"
        else
            warn "no country code - WiFi may stay blocked (rfkill); Ethernet still works"
            rfkill unblock wifi >/dev/null 2>&1 || true
            nmcli radio wifi on >/dev/null 2>&1 || true
        fi
    else
        ui_message "No WiFi adapter found" "Ethernet is the only option on $MACHINE." --wait 3
    fi

    while :; do
        if is_online; then
            log "Connected."
            return 0
        fi
        nets=()
        items=()
        if has_wifi_device; then
            ui_message "Scanning for WiFi networks..."
            mapfile -t nets < <(wifi_scan)
            local i
            for ((i = 0; i < ${#nets[@]}; i++)); do
                IFS=$'\t' read -r sig sec ssid <<<"${nets[i]}"
                items+=("$(printf '%d=%-32.32s %3s%%  %s' "$((i + 1))" "$ssid" "$sig" "${sec:---}")")
            done
            items+=("r=Scan again" "h=Hidden network (type its name)")
        fi
        # no "skip": the install cannot happen without the network (the owner's rule) - the menu comes back
        # until something works; a power-off simply brings the question back on the next boot
        items+=("e=I have plugged in an Ethernet cable - check again")
        choice="$(ui_menu "No network connection" \
            "AutoBleem needs the internet for its first setup (packages, RetroArch, BIOS files)." \
            "$([ ${#nets[@]} -gt 0 ] && echo "Pick a WiFi network, or plug in an Ethernet cable:" || echo "No WiFi networks found - plug in an Ethernet cable, or scan again:")" \
            -- "${items[@]}" ${nets:+--default 1})"

        hidden=no
        case "$choice" in
            r|R|"") continue ;;
            e|E)
                ui_message "Waiting for the network..." "Checking the connection for up to 30 seconds."
                wait_for_network 30 && { log "Connected."; return 0; }
                ui_message "Still no network" "Nothing reachable through the cable yet." --wait 3
                continue ;;
            h|H)
                ssid="$(ui_input "Hidden network" "The network's name (SSID):")"
                [ -n "$ssid" ] || continue
                hidden=yes; sec="?" ;;
            *)
                if ! [[ "$choice" =~ ^[0-9]+$ ]] || [ "$choice" -lt 1 ] || [ "$choice" -gt ${#nets[@]} ]; then
                    continue
                fi
                IFS=$'\t' read -r sig sec ssid <<<"${nets[choice - 1]}" ;;
        esac

        pass=""
        if [ "$hidden" = yes ] || { [ -n "$sec" ] && [ "$sec" != "--" ]; }; then
            pass="$(ui_input "Password for '$ssid'" "Leave it empty for an open network." --secret)"
        fi

        ui_message "Connecting to $ssid..."
        # a failed attempt leaves a profile with the wrong password behind, which a retry would reuse
        nmcli connection delete "$ssid" >/dev/null 2>&1 || true
        local -a cmd=(nmcli device wifi connect "$ssid")
        [ -n "$pass" ] && cmd+=(password "$pass")
        [ "$hidden" = yes ] && cmd+=(hidden yes)
        if out="$("${cmd[@]}" 2>&1)"; then
            ui_message "Connected to $ssid" "Checking that the internet is reachable..."
            if wait_for_network 40; then
                log "Connected to $ssid."
                return 0
            fi
            ui_message "Connected to $ssid, but the internet is not reachable through it" --wait 4
        else
            ui_message "Could not connect to $ssid" "${out##*$'\n'}" --wait 4
            nmcli connection delete "$ssid" >/dev/null 2>&1 || true
        fi
    done
}

# waits for network-online, then asks until it is online (there is no skipping the network)
ensure_network() {
    ui_message "Waiting for the network..." "Up to 40 seconds for WiFi or Ethernet to come up."
    if wait_for_network 40; then
        return 0
    fi
    interactive_network
}

#*******************************
# main
#*******************************
# an image built from packages (tools/make_pc_image.sh) ships without ssh host keys - every stick would
# share them otherwise - and Debian's sshd does not make its own the way Raspberry Pi OS's
# regenerate_ssh_host_keys.service does; done first thing, so ssh works whatever happens below
if [ -d /etc/ssh ] && ! ls /etc/ssh/ssh_host_*_key >/dev/null 2>&1; then
    ssh-keygen -A >/dev/null 2>&1 && systemctl restart ssh >/dev/null 2>&1 || true
fi
show_our_tty
clear 2>/dev/null || true
printf '\n\033[1m  AutoBleem - first boot setup\033[0m\n\n'

if [ -f "$MARKER" ]; then
    log "already applied - disabling myself"
    disarm
    give_tty_back
    exit 0
fi

if [ ! -f "$PACKAGE" ]; then
    log "no $PACKAGE - nothing staged to install, disabling myself"
    disarm
    give_tty_back
    exit 0
fi

attempts=0
[ -f "$ATTEMPTS_FILE" ] && attempts="$(cat "$ATTEMPTS_FILE" 2>/dev/null || echo 0)"
attempts=$((attempts + 1))
echo "$attempts" >"$ATTEMPTS_FILE"

if [ "$attempts" -gt "$MAX_ATTEMPTS" ]; then
    warn "gave up after $((attempts - 1)) attempts - run install.sh by hand"
    note_in_data_logs "autobleem-firstboot gave up after $((attempts - 1)) attempts. Run install.sh by hand: sudo bash $UNPACK_DIR/install.sh --yes (re-extract $PACKAGE first if $UNPACK_DIR is gone)"
    disarm
    give_tty_back
    exit 0
fi
log "attempt $attempts of $MAX_ATTEMPTS"

read_options
choose_ui_mode

if ! ensure_network; then
    warn "no network - AutoBleem setup will try again on the next boot"
    bail "No network" "AutoBleem could not reach the internet."
fi

# a Pi has no battery clock: its time is the image's build date until NTP sets it, and apt refuses
# repository metadata "from the future". A PC has an RTC - nothing to wait for there.
if [ "$PLATFORM" = rpi ]; then
    ui_message "Setting the clock..." "A Pi has no battery clock; the time comes from the network (NTP)."
    if wait_for_clock; then
        log "Clock synchronised: $(date)"
    else
        warn "clock not synchronised yet ($(date)) - carrying on"
    fi
fi

ask_retroarch

# The package is staged on the root filesystem, and until install.sh has grown the root that filesystem is
# the base image's size - a fresh Lite root has a few hundred MB free, less than the package unpacks to
# (the cover databases alone are ~290 MB). So the root is grown *before* the tarball is extracted: only
# install.sh itself comes out first, and runs with --grow-only (a no-op once the root is that big, so a
# retry is harmless). The first armhf flash died here with "No space left on device".
EXTRACTED_MARKER="$UNPACK_DIR/.extracted"
if [ -d "$UNPACK_DIR" ] && [ ! -f "$EXTRACTED_MARKER" ]; then
    log "Removing a partly unpacked $UNPACK_DIR from an earlier attempt"
    rm -rf "$UNPACK_DIR"
fi
if [ ! -f "$EXTRACTED_MARKER" ]; then
    mkdir -p "$IMAGE_DIR"
    if ! tar xzf "$PACKAGE" -C "$IMAGE_DIR" "$(basename "$UNPACK_DIR")/install.sh"; then
        warn "cannot extract install.sh from $PACKAGE - will retry next boot"
        bail "Setup failed" "Could not extract the installer from $(basename "$PACKAGE")."
    fi
    root_gib="${OPT[root_gib]:-8}"
    if [ "$root_gib" != 0 ] && [ "$root_gib" != none ]; then
        log "Growing the root filesystem to ${root_gib} GiB before unpacking"
        ui_message "Preparing the system partition..." "Growing it to ${root_gib} GiB."
        if ! bash "$UNPACK_DIR/install.sh" --yes --grow-root "$root_gib" --grow-only >>"$INSTALL_LOG" 2>&1; then
            # the reason on the screen, not only in the log (the first PC-stick boot sat on "Preparing the
            # system partition" with the reason only there)
            warn "could not grow the root filesystem - will retry next boot. Log: $INSTALL_LOG"
            bail "Setup failed" "Could not grow the system partition."
        fi
    fi
    # gzip's trailer carries the unpacked size, so this costs no decompression pass
    unpacked_bytes="$(gzip -l "$PACKAGE" 2>/dev/null | awk 'NR == 2 { print $2 }')"
    need_mib="$(( ${unpacked_bytes:-0} / 1048576 + 64 ))"
    free_mib="$(df -Pm "$IMAGE_DIR" | awk 'NR == 2 { print $4 }')"
    if [ "$free_mib" -lt "$need_mib" ]; then
        warn "only ${free_mib} MiB free on the root filesystem, the package needs ${need_mib} MiB to unpack - will retry next boot"
        note_in_data_logs "autobleem-firstboot: only ${free_mib} MiB free under $IMAGE_DIR, need ${need_mib} MiB (root_gib=$root_gib in $OPTIONS_FILE)"
        bail "Setup failed" "Only ${free_mib} MiB free on the system partition; the package needs ${need_mib} MiB to unpack." \
            "(root_gib=$root_gib in $OPTIONS_FILE)"
    fi
    log "Unpacking $PACKAGE (${need_mib} MiB)"
    ui_message "Unpacking AutoBleem..." "${need_mib} MiB"
    if ! tar xzf "$PACKAGE" -C "$IMAGE_DIR"; then
        warn "extract failed - will retry next boot"
        rm -rf "$UNPACK_DIR"
        bail "Setup failed" "Could not unpack $(basename "$PACKAGE")."
    fi
    touch "$EXTRACTED_MARKER"
fi

INSTALLER="$UNPACK_DIR/install.sh"
if [ ! -f "$INSTALLER" ]; then
    # tools/make_rpi_package.sh's tarball has autobleem-<platform>/ as its single top-level entry, so extracting
    # into $IMAGE_DIR should always produce $UNPACK_DIR/install.sh directly - this only fires if that
    # layout ever changes, and retrying won't fix it.
    warn "no install.sh under $UNPACK_DIR - package layout unexpected, giving up"
    note_in_data_logs "autobleem-firstboot: no install.sh under $UNPACK_DIR after extracting $PACKAGE - package layout unexpected"
    disarm
    ui_message "Setup failed" "No install.sh in $(basename "$PACKAGE") - the package is not laid out as expected." \
        "AutoBleem's setup gives up; run install.sh by hand." --wait 8
    text_mode
    give_tty_back
    exit 1
fi

mapfile -t ARGS < <(install_args)
log "Running install.sh ${ARGS[*]}"
log "(this takes a while: packages, RetroArch, cores and BIOS downloads)"
[ "$UI_MODE" = plain ] && printf '\n'
{
    printf '=== %s: install.sh %s\n' "$(date)" "${ARGS[*]}"
} >>"$INSTALL_LOG" 2>/dev/null || true
# The installer's output goes through the screen (autobleem-install-ui.py: a bar per phase from
# install.sh's @@phase lines, a bar for the download in progress, the last lines of output - under the
# logo, or as text) - and, whole, into the log. With plain prompts the output shows as it comes.
if [ "$UI_MODE" != plain ]; then
    run_installer() {
        AB_UI_MARKERS=1 bash "$INSTALLER" "${ARGS[@]}" 2>&1 | tee -a "$INSTALL_LOG" | ui progress
        return "${PIPESTATUS[0]}"
    }
else
    run_installer() {
        bash "$INSTALLER" "${ARGS[@]}" 2>&1 | tee -a "$INSTALL_LOG"
        return "${PIPESTATUS[0]}"
    }
fi
if run_installer; then
    log "install.sh succeeded"
    touch "$MARKER"
    rm -f "$ATTEMPTS_FILE"
    disarm
    # the staged tarball/tree already did their job (install.sh copied everything onto the data partition)
    rm -rf "$UNPACK_DIR" "$PACKAGE"
    log "Rebooting to finish - the HDMI mode and boot splash only take full effect on the next boot"
    sleep 3
    # install.sh mounted the data partition itself, so it is handed back clean here: the first 64-bit
    # boot (2026-09-20) came up with "exFAT-fs: Volume was not properly unmounted" and an empty config.ini
    # (the launcher then started with the default theme) - a sync alone did not do it
    sync
    for m in $(findmnt -rno TARGET -t exfat 2>/dev/null); do
        umount "$m" 2>/dev/null && log "Unmounted $m" || warn "could not unmount $m - syncing instead"
    done
    sync
    reboot
else
    rc=$?
    warn "install.sh failed (exit $rc) - will retry next boot. Log: $INSTALL_LOG"
    bail "Setup failed" "install.sh ended with an error (exit code $rc)."
fi
