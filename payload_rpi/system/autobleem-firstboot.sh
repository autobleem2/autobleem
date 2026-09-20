#!/usr/bin/env bash
#
# Runs once per boot, via autobleem-firstboot.service (WantedBy=multi-user.target), on a card written from
# an image tools/make_rpi_image.sh built - until AutoBleem's own install.sh has completed successfully once.
#
# The service hands this script a virtual terminal (tty8, switched onto the screen here), so the first boot
# is something the user watches and can answer, not a silent background job: it waits for network, and when there is none it asks
# for a WiFi network and password right here (the first real boot of the image had no network, install.sh
# failed at apt and nobody could tell why without ssh - which needs the network). Then it runs install.sh
# with the options from autobleem.txt on the boot partition, and reboots once that has succeeded.
#
# This is the "first or second boot" story from docs/rpi-image-and-update-plan.md: Raspberry Pi Imager's own
# customisation (hostname, user, WiFi, SSH - cloud-init on this image) has run earlier in the same boot, so
# with WiFi preset here nothing is asked. A run that cannot finish (the user skipped the network question,
# install.sh failed) simply happens again on the next boot - the script is idempotent and re-arms itself by
# not disabling the service until it either succeeds or gives up after MAX_ATTEMPTS boots.
set -uo pipefail

IMAGE_DIR=/opt/autobleem-image
MARKER="$IMAGE_DIR/.done"
ATTEMPTS_FILE="$IMAGE_DIR/.attempts"
MAX_ATTEMPTS=20
SELF_SERVICE=autobleem-firstboot.service
PACKAGE="$IMAGE_DIR/autobleem-rpi.tar.gz"
UNPACK_DIR="$IMAGE_DIR/autobleem-rpi"
BOOT_DIR=/boot/firmware
OPTIONS_FILE="$BOOT_DIR/autobleem.txt"      # written by tools/make_rpi_image.sh from payload_rpi/system/autobleem.txt
INSTALL_LOG=/var/log/autobleem-firstboot-install.log   # install.sh's output, for a look after the fact (ssh)

# the script's stdout is tty1; the journal only gets what log() sends it
log() {
    printf '\033[1;32m==>\033[0m %s\n' "$*"
    logger -t autobleem-firstboot -- "$*" 2>/dev/null || true
}
warn() {
    printf '\033[1;33m[!]\033[0m %s\n' "$*"
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
# payload_rpi/system/autobleem.txt documents: root_gib, hdmi_mode, retroarch, thumbnails, bios, downloads.
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
    printf '\n'
    printf '   AutoBleem plays PlayStation games on its own. RetroArch adds the other systems - NES, SNES,\n'
    printf '   Mega Drive, arcade and about a hundred more - a ready-made build is downloaded (or, if the\n'
    printf '   download site cannot be reached, it is built here, 10-40 minutes), plus close to a GB of cores,\n'
    printf '   databases and BIOS files. It can be added later by running the installer again.\n\n'
    local answer=""
    if read -rt 60 -p "   Install RetroArch too? [Y/n] (yes in 60 seconds) " answer; then
        printf '\n'
    else
        printf '\n   (no answer - installing RetroArch)\n'
    fi
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
    v="${OPT[downloads]:-yes}"
    if [ "${OPT[retroarch]:-}" = none ]; then v=no; fi
    case "$v" in no|false|0) args+=(--no-downloads) ;; esac
    printf '%s\n' "${args[@]}"
}

#*******************************
# network
#*******************************
# "online" means a real fetch works, not just an interface with an address: DNS plus one HTTP round trip to
# the host install.sh downloads from first.
is_online() {
    getent hosts downloads.raspberrypi.com >/dev/null 2>&1 || return 1
    wget -q --spider --timeout=5 --tries=1 https://downloads.raspberrypi.com/ >/dev/null 2>&1
}

# waits up to $1 seconds, printing a dot a second
wait_for_network() {
    local secs="${1:-30}" i
    for ((i = 0; i < secs; i++)); do
        is_online && { printf '\n'; return 0; }
        printf '.'
        sleep 1
    done
    printf '\n'
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
# command line - cmdline.txt first, it is the current setting, /proc/cmdline is what this boot started
# with), else the locale's territory, else nothing - the prompt then asks
wifi_country_default() {
    local cc
    cc="$(cat "$BOOT_DIR/cmdline.txt" /proc/cmdline 2>/dev/null | tr ' ' '\n' \
        | sed -n 's/^cfg80211\.ieee80211_regdom=//p' | head -1)"
    [ -n "$cc" ] && { echo "$cc"; return 0; }
    cc="$(sed -n 's/^LANG=[a-z]*_\([A-Z][A-Z]\).*/\1/p' /etc/default/locale 2>/dev/null | head -1)"
    echo "${cc:-}"
}

# Raspberry Pi OS keeps WiFi soft-blocked (rfkill) until a country is set - that is why a Lite image whose
# first-boot wizard skipped the WiFi step has no WiFi at all. raspi-config knows how to set it persistently.
set_wifi_country() {
    local cc="$1"
    if command -v raspi-config >/dev/null 2>&1; then
        raspi-config nonint do_wifi_country "$cc" >/dev/null 2>&1 || true
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

# The screen-and-keyboard WiFi setup. Returns 0 once online, 1 if the user chose to skip.
interactive_network() {
    local cc choice ssid sec sig pass hidden out
    local -a nets

    printf '\n'
    log "No network connection."
    printf '   AutoBleem needs the internet for its first setup (packages, RetroArch, BIOS files).\n'
    printf '   Plug in an Ethernet cable, or pick a WiFi network below.\n\n'

    if has_wifi_device; then
        cc="$(wifi_country_default)"
        read -rp "   WiFi country code (two letters, e.g. GB, PL, US)${cc:+ [$cc]}: " choice
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
        warn "no WiFi adapter found - Ethernet is the only option on this Pi"
    fi

    while :; do
        if is_online; then
            log "Connected."
            return 0
        fi
        nets=()
        if has_wifi_device; then
            printf '\n   Scanning for WiFi networks...\n'
            mapfile -t nets < <(wifi_scan)
            if [ ${#nets[@]} -eq 0 ]; then
                printf '   (no networks found)\n'
            else
                local i
                for ((i = 0; i < ${#nets[@]}; i++)); do
                    IFS=$'\t' read -r sig sec ssid <<<"${nets[i]}"
                    printf '   %2d) %-40.40s  %3s%%  %s\n' "$((i + 1))" "$ssid" "$sig" "${sec:---}"
                done
            fi
            printf '\n'
            printf '    r) scan again        h) hidden network (type its name)\n'
        fi
        printf '    e) I have plugged in an Ethernet cable - check again\n'
        printf '    s) skip for now - AutoBleem will ask again on the next boot\n\n'
        read -rp "   Your choice${nets:+ [1-${#nets[@]}]}: " choice

        hidden=no
        case "$choice" in
            r|R|"") continue ;;
            s|S) return 1 ;;
            e|E)
                printf '   Waiting for the network'
                wait_for_network 30 && { log "Connected."; return 0; }
                warn "still no network"
                continue ;;
            h|H)
                read -rp "   Network name (SSID): " ssid
                [ -n "$ssid" ] || continue
                hidden=yes; sec="?" ;;
            *)
                if ! [[ "$choice" =~ ^[0-9]+$ ]] || [ "$choice" -lt 1 ] || [ "$choice" -gt ${#nets[@]} ]; then
                    printf '   ?\n'
                    continue
                fi
                IFS=$'\t' read -r sig sec ssid <<<"${nets[choice - 1]}" ;;
        esac

        pass=""
        if [ "$hidden" = yes ] || { [ -n "$sec" ] && [ "$sec" != "--" ]; }; then
            read -rsp "   Password for '$ssid' (empty for an open network): " pass
            printf '\n'
        fi

        printf '   Connecting to %s...\n' "$ssid"
        # a failed attempt leaves a profile with the wrong password behind, which a retry would reuse
        nmcli connection delete "$ssid" >/dev/null 2>&1 || true
        local -a cmd=(nmcli device wifi connect "$ssid")
        [ -n "$pass" ] && cmd+=(password "$pass")
        [ "$hidden" = yes ] && cmd+=(hidden yes)
        if out="$("${cmd[@]}" 2>&1)"; then
            printf '   Checking the connection'
            if wait_for_network 40; then
                log "Connected to $ssid."
                return 0
            fi
            warn "connected to $ssid, but the internet is not reachable through it"
        else
            warn "could not connect: ${out##*$'\n'}"
            nmcli connection delete "$ssid" >/dev/null 2>&1 || true
        fi
    done
}

# waits for network-online, then falls back to asking. 0 = online, 1 = the user skipped
ensure_network() {
    printf 'Waiting for the network'
    if wait_for_network 40; then
        return 0
    fi
    interactive_network
}

#*******************************
# main
#*******************************
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

if ! ensure_network; then
    warn "no network - AutoBleem setup will try again on the next boot"
    sleep 3
    give_tty_back
    exit 1
fi

if wait_for_clock; then
    log "Clock synchronised: $(date)"
else
    warn "clock not synchronised yet ($(date)) - carrying on"
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
    if ! tar xzf "$PACKAGE" -C "$IMAGE_DIR" autobleem-rpi/install.sh; then
        warn "cannot extract install.sh from $PACKAGE - will retry next boot"
        sleep 3
        give_tty_back
        exit 1
    fi
    root_gib="${OPT[root_gib]:-8}"
    if [ "$root_gib" != 0 ] && [ "$root_gib" != none ]; then
        log "Growing the root filesystem to ${root_gib} GiB before unpacking"
        if ! bash "$UNPACK_DIR/install.sh" --yes --grow-root "$root_gib" --grow-only 2>&1 | tee -a "$INSTALL_LOG"; then
            warn "could not grow the root filesystem - will retry next boot. Log: $INSTALL_LOG"
            sleep 5
            give_tty_back
            exit 1
        fi
    fi
    # gzip's trailer carries the unpacked size, so this costs no decompression pass
    unpacked_bytes="$(gzip -l "$PACKAGE" 2>/dev/null | awk 'NR == 2 { print $2 }')"
    need_mib="$(( ${unpacked_bytes:-0} / 1048576 + 64 ))"
    free_mib="$(df -Pm "$IMAGE_DIR" | awk 'NR == 2 { print $4 }')"
    if [ "$free_mib" -lt "$need_mib" ]; then
        warn "only ${free_mib} MiB free on the root filesystem, the package needs ${need_mib} MiB to unpack - will retry next boot"
        note_in_data_logs "autobleem-firstboot: only ${free_mib} MiB free under $IMAGE_DIR, need ${need_mib} MiB (root_gib=$root_gib in $OPTIONS_FILE)"
        sleep 5
        give_tty_back
        exit 1
    fi
    log "Unpacking $PACKAGE (${need_mib} MiB)"
    if ! tar xzf "$PACKAGE" -C "$IMAGE_DIR"; then
        warn "extract failed - will retry next boot"
        rm -rf "$UNPACK_DIR"
        sleep 3
        give_tty_back
        exit 1
    fi
    touch "$EXTRACTED_MARKER"
fi

INSTALLER="$UNPACK_DIR/install.sh"
if [ ! -f "$INSTALLER" ]; then
    # tools/make_rpi_package.sh's tarball has autobleem-rpi/ as its single top-level entry, so extracting
    # into $IMAGE_DIR should always produce $UNPACK_DIR/install.sh directly - this only fires if that
    # layout ever changes, and retrying won't fix it.
    warn "no install.sh under $UNPACK_DIR - package layout unexpected, giving up"
    note_in_data_logs "autobleem-firstboot: no install.sh under $UNPACK_DIR after extracting $PACKAGE - package layout unexpected"
    disarm
    give_tty_back
    exit 1
fi

mapfile -t ARGS < <(install_args)
log "Running install.sh ${ARGS[*]}"
log "(this takes a while: packages, RetroArch built from source, cores and BIOS downloads)"
printf '\n'
{
    printf '=== %s: install.sh %s\n' "$(date)" "${ARGS[*]}"
} >>"$INSTALL_LOG" 2>/dev/null || true
if bash "$INSTALLER" "${ARGS[@]}" 2>&1 | tee -a "$INSTALL_LOG"; then
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
    sleep 5
    give_tty_back
    exit 1
fi
