#!/usr/bin/env bash
#
# AutoBleem for Raspberry Pi - installer for Raspberry Pi OS Lite (32-bit).
#
# Turns a plain Lite install into an AutoBleem appliance: an exFAT data partition that behaves like the
# PlayStation Classic's USB stick (drop games onto it from Windows/macOS/Linux), the launcher started at boot
# on tty1 with no desktop, and RetroArch behind it.
#
# Run it from the unpacked release package:   sudo bash install.sh
# See README.md in this directory for the whole story, including what has and has not run on real hardware.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

#*******************************
# defaults
#*******************************
DATA_LABEL="AUTOBLEEM"          # the exFAT partition's label - also how the README tells users to find it
DATA_MOUNT="/media/autobleem"
MIN_DATA_MIB=2048               # refuse to make a data partition smaller than this - it holds every game
SHRINK_ROOT_GIB=""              # --shrink-root: repartition, see shrink_root() (opt-in, it is destructive)
STAGE_DIR="$SCRIPT_DIR"         # the payload tree to install (Autobleem/, themes/, Games/, Apps/)
DISK=""                         # --disk: the SD card. autodetected from where /boot/firmware lives
DRY_RUN=0
ASSUME_YES=0                    # --yes: answer every confirmation, for unattended runs over ssh
DO_PACKAGES=1
DO_BOOT_CONFIG=1
QUIET_BOOT=1                    # strip the kernel log/rainbow splash so the launcher is the only thing seen
RETROARCH_MODE=source           # --retroarch: source (latest release, built here) | apt | none
DO_DOWNLOADS=1                  # --no-downloads: skip the RetroArch cores/assets from buildbot.libretro.com
RA_ROOT=""                      # $DATA_MOUNT/RetroArch once the mount point is known

#*******************************
# output helpers
#*******************************
log()  { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[!]\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31m[x]\033[0m %s\n' "$*" >&2; exit 1; }

# every command that changes the machine goes through this, so --dry-run is honest. Commands get no stdin:
# apt/git/make would otherwise swallow the answer a piped "YES" meant for confirm() (seen over ssh).
run() {
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would run: %s\n' "$*"
    else
        "$@" </dev/null
    fi
}

# write_file TARGET < content: atomically (temp file in the same directory, then rename) and synced, so a
# power cut right after the installer - the Pi 400 has no power button - cannot leave a truncated file
# behind. It did once: an empty autobleem.service, which systemd treats as masked.
write_file() {
    local target="$1"
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would write %s\n' "$target"
        cat > /dev/null
        return 0
    fi
    local tmp="$target.autobleem-tmp"
    cat > "$tmp" && sync "$tmp" 2>/dev/null; mv -f "$tmp" "$target" && sync
}

confirm() {
    local prompt="$1"
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would ask: %s\n' "$prompt"
        return 0
    fi
    if [ "$ASSUME_YES" -eq 1 ]; then
        printf '%s [--yes]\n' "$prompt"
        return 0
    fi
    local answer
    read -r -p "$prompt [type YES to continue] " answer
    [ "$answer" = "YES" ] || die "aborted"
}

usage() {
    cat <<'EOF'
Usage: sudo bash install.sh [options]

  --disk DEVICE        SD card to put the data partition on (default: the disk /boot/firmware is on)
  --mount PATH         where to mount it (default: /media/autobleem)
  --stage DIR          the payload tree to install from (default: this directory)
  --shrink-root GIB    shrink the root filesystem to GIB and use the freed space for the data partition.
                       REPARTITIONS THE CARD. Needs a reboot to do the work offline. Back up first.
  --no-packages        skip apt - assume SDL2/RetroArch/exfatprogs are already installed
  --no-boot-config     do not touch cmdline.txt/config.txt
  --no-quiet-boot      keep the kernel messages and rainbow splash on screen while booting
  --retroarch MODE     source (default): build the latest RetroArch release here, 10-40 min on a Pi;
                       apt: the distribution's package; none: leave RetroArch alone
  --no-downloads       do not download the RetroArch cores, core info, assets, databases from
                       buildbot.libretro.com (a few hundred MB; RetroArch's Online Updater can do it later)
  --yes                answer every confirmation with YES (unattended runs; --shrink-root repartitions!)
  --dry-run            print what would happen and change nothing
  -h, --help           this text
EOF
}

#*******************************
# parse_args
#*******************************
parse_args() {
    while [ $# -gt 0 ]; do
        case "$1" in
            --disk)           DISK="${2:?--disk needs a device}"; shift 2 ;;
            --mount)          DATA_MOUNT="${2:?--mount needs a path}"; shift 2 ;;
            --stage)          STAGE_DIR="${2:?--stage needs a directory}"; shift 2 ;;
            --shrink-root)    SHRINK_ROOT_GIB="${2:?--shrink-root needs a size in GiB}"; shift 2 ;;
            --no-packages)    DO_PACKAGES=0; shift ;;
            --no-boot-config) DO_BOOT_CONFIG=0; shift ;;
            --no-quiet-boot)  QUIET_BOOT=0; shift ;;
            --retroarch)      RETROARCH_MODE="${2:?--retroarch needs source, apt or none}"; shift 2 ;;
            --no-downloads)   DO_DOWNLOADS=0; shift ;;
            --yes)            ASSUME_YES=1; shift ;;
            --dry-run)        DRY_RUN=1; shift ;;
            -h|--help)        usage; exit 0 ;;
            *)                usage; die "unknown option: $1" ;;
        esac
    done
}

#*******************************
# preflight
#*******************************
# Everything that would make the rest pointless, checked before anything is changed.
preflight() {
    [ "$(id -u)" -eq 0 ] || die "run this with sudo"

    if [ -r /proc/device-tree/model ]; then
        log "Model: $(tr -d '\0' < /proc/device-tree/model)"
    else
        warn "this does not look like a Raspberry Pi (no /proc/device-tree/model)"
    fi

    # The cross build in toolchains/rpi is arm-linux-gnueabihf, so the userland has to be 32-bit. A 64-bit
    # Raspberry Pi OS will not run it without setting up armhf multiarch, which is not what this installer does.
    local arch
    arch="$(dpkg --print-architecture)"
    if [ "$arch" != "armhf" ]; then
        die "this package is 32-bit (armhf) and the system is '$arch'. Flash the 32-bit Raspberry Pi OS Lite,
    or rebuild AutoBleem with an aarch64 toolchain."
    fi

    if [ -r /etc/os-release ]; then
        # shellcheck disable=SC1091
        . /etc/os-release
        log "OS: ${PRETTY_NAME:-unknown}"
    fi

    if systemctl list-unit-files 2>/dev/null | grep -qE '^(lightdm|gdm3|sddm)\.service'; then
        warn "a display manager is installed - this is not the Lite image. AutoBleem will still be set up to
    own tty1, but the desktop may fight it for the screen. 'sudo systemctl disable lightdm' if it does."
    fi

    BOOT_DIR=/boot/firmware
    [ -d "$BOOT_DIR" ] || BOOT_DIR=/boot     # pre-bookworm images keep the firmware files in /boot
    [ -f "$BOOT_DIR/cmdline.txt" ] || die "no cmdline.txt under $BOOT_DIR - is this Raspberry Pi OS?"
    log "Boot files: $BOOT_DIR"

    if [ -z "$DISK" ]; then
        local bootpart
        bootpart="$(findmnt -no SOURCE "$BOOT_DIR")" || die "cannot tell which device $BOOT_DIR is on"
        DISK="/dev/$(lsblk -no PKNAME "$bootpart")"
    fi
    [ -b "$DISK" ] || die "not a block device: $DISK"
    log "SD card: $DISK"
}

#*******************************
# pkg_first_available
#*******************************
# echoes the first of the given package names apt knows about. Trixie renamed a number of 32-bit libraries
# for the 64-bit time_t transition (libpng16-16 -> libpng16-16t64) and the Mesa dev packages
# (libegl1-mesa-dev -> libegl-dev), so every list that has to work on both Bookworm and Trixie goes through
# this rather than naming one of them.
pkg_first_available() {
    local name
    for name in "$@"; do
        if apt-cache show "$name" >/dev/null 2>&1; then
            echo "$name"
            return 0
        fi
    done
    echo "$1"    # let apt-get report the miss itself
}

#*******************************
# install_packages
#*******************************
install_packages() {
    [ "$DO_PACKAGES" -eq 1 ] || { log "skipping apt (--no-packages)"; return 0; }

    log "Installing packages"
    # not fatal: a Pi with no network but a warm apt cache can still have everything that is needed
    if ! run apt-get update; then
        warn "apt-get update failed - carrying on with whatever is already cached"
    fi

    # SDL2 is what autobleem-gui draws with (pcsx-ab too, plus libpng16 for its screenshots and skin);
    # exfatprogs formats the data partition; parted creates it; wget/unzip fetch the RetroArch cores.
    run apt-get install -y \
        libsdl2-2.0-0 libsdl2-image-2.0-0 libsdl2-mixer-2.0-0 libsdl2-ttf-2.0-0 \
        "$(pkg_first_available libpng16-16t64 libpng16-16)" zlib1g \
        exfatprogs parted alsa-utils wget unzip ca-certificates
}

#*******************************
# install_retroarch
#*******************************
# RetroArch is the second half of the launcher: the RetroArch set and playlists, "RetroArch" in the L2+R2
# system menu, and a PS1 game's "Play using RA" option. libretro's buildbot has every core for armhf but no
# frontend build, and the distribution's package trails the releases, so the default is to build the latest
# tagged release here, from source (10-40 minutes depending on the Pi). --retroarch apt takes the
# distribution's instead; --retroarch none leaves whatever is installed alone.
install_retroarch() {
    case "$RETROARCH_MODE" in
        none)
            log "leaving RetroArch alone (--retroarch none)"
            return 0 ;;
        apt)
            install_retroarch_apt
            return 0 ;;
        source)
            install_retroarch_source || {
                warn "building RetroArch failed - installing the distribution's package instead"
                install_retroarch_apt
            }
            return 0 ;;
        *)
            die "--retroarch takes source, apt or none (got '$RETROARCH_MODE')" ;;
    esac
}

install_retroarch_apt() {
    [ "$DO_PACKAGES" -eq 1 ] || { log "skipping RetroArch from apt (--no-packages)"; return 0; }
    if ! run apt-get install -y retroarch; then
        warn "could not install retroarch from apt - the RetroArch set and 'Play using RA' will not work"
    fi
}

install_retroarch_source() {
    local src=/usr/local/src/RetroArch
    local stamp=/usr/local/share/autobleem/retroarch.version

    log "RetroArch: looking up the latest release"
    if [ "$DRY_RUN" -eq 1 ] && ! command -v git >/dev/null 2>&1; then
        printf '    would install git, look up the latest tag on github.com and build it into /usr/local\n'
        return 0
    fi
    local tag
    tag="$(git ls-remote --tags --refs https://github.com/libretro/RetroArch.git 2>/dev/null \
            | awk -F/ '{print $NF}' | grep -E '^v[0-9]+\.[0-9]+(\.[0-9]+)?$' | sort -V | tail -1)" || true
    if [ -z "$tag" ]; then
        # git is not installed yet on a fresh Lite image, or there is no network
        if [ "$DO_PACKAGES" -eq 1 ] && run apt-get install -y git; then
            tag="$(git ls-remote --tags --refs https://github.com/libretro/RetroArch.git 2>/dev/null \
                    | awk -F/ '{print $NF}' | grep -E '^v[0-9]+\.[0-9]+(\.[0-9]+)?$' | sort -V | tail -1)" || true
        fi
    fi
    [ -n "$tag" ] || { warn "cannot reach github.com to find the latest RetroArch release"; return 1; }

    if [ -f "$stamp" ] && [ "$(cat "$stamp")" = "$tag" ] && [ -x /usr/local/bin/retroarch ]; then
        log "RetroArch $tag is already built and installed"
        return 0
    fi
    log "RetroArch: building $tag from source (this takes a while on a Pi)"

    if [ "$DO_PACKAGES" -eq 1 ]; then
        # KMS/EGL/GLES output, udev pads, ALSA sound. No X11, no Wayland, no Qt, no ffmpeg recording.
        run apt-get install -y build-essential git pkg-config \
            libasound2-dev libudev-dev libusb-1.0-0-dev libgbm-dev libdrm-dev \
            "$(pkg_first_available libegl-dev libegl1-mesa-dev)" \
            "$(pkg_first_available libgles-dev libgles2-mesa-dev)" \
            libfreetype-dev zlib1g-dev libxml2-dev libsdl2-dev libflac-dev || return 1
    fi

    if [ -d "$src/.git" ]; then
        run git -C "$src" fetch --depth 1 origin "refs/tags/$tag:refs/tags/$tag" || return 1
        run git -C "$src" checkout -q "$tag" || return 1
    else
        run rm -rf "$src"
        run git clone --depth 1 --branch "$tag" https://github.com/libretro/RetroArch.git "$src" || return 1
    fi

    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would run: (cd %s && ./configure ... && make -j%s && make install)\n' "$src" "$(nproc)"
        return 0
    fi
    (
        cd "$src" || exit 1
        exec </dev/null
        ./configure --prefix=/usr/local \
            --disable-x11 --disable-wayland --disable-videocore --disable-vulkan --disable-qt \
            --disable-ffmpeg --disable-jack --disable-oss --disable-pulse --disable-sdl \
            --enable-sdl2 --enable-kms --enable-egl --enable-opengles --enable-opengles3 \
            --enable-udev --enable-alsa --enable-networking \
        && make -j"$(nproc)" \
        && make install
    ) || return 1

    mkdir -p "$(dirname "$stamp")"
    echo "$tag" > "$stamp"
    hash -r
    log "RetroArch: installed $tag as /usr/local/bin/retroarch"
}

#*******************************
# create_retroarch_tree
#*******************************
# RetroArch's standard directory layout, on the data partition so that games, BIOS files, saves and
# playlists can be reached from a PC the same way the PS1 games can. retroarch.cfg (write_retroarch_config)
# points every RetroArch directory setting in here, and AutoBleem reads info/ and playlists/ for its
# RetroArch set (Env::getPathToRetroarchDir() is this folder on a Pi).
RA_SUBDIRS="cores info system roms saves states playlists config assets autoconfig database/rdb database/cursors
            cheats overlays shaders filters/video filters/audio thumbnails screenshots records logs downloads"

# roms/ gets a folder per system, named the way RetroArch's databases and playlists name them, so that
# "Import Content -> Scan Directory" on roms/ sorts every game into the matching playlist - which is what
# AutoBleem's RetroArch set then shows. Any layout works for RetroArch; these are the names that make the
# scanner's job obvious, and tell the user where a NES or a Mega Drive game goes.
RA_ROM_SYSTEMS="Arcade
Atari - 2600
Atari - 7800
Atari - Lynx
Bandai - WonderSwan Color
NEC - PC Engine - TurboGrafx 16
NEC - PC Engine CD - TurboGrafx-CD
Nintendo - Game Boy
Nintendo - Game Boy Color
Nintendo - Game Boy Advance
Nintendo - Nintendo Entertainment System
Nintendo - Super Nintendo Entertainment System
Nintendo - Nintendo 64
Nintendo - Virtual Boy
Sega - Master System - Mark III
Sega - Mega Drive - Genesis
Sega - Mega-CD - Sega CD
Sega - 32X
Sega - Game Gear
SNK - Neo Geo Pocket Color
Sony - PlayStation Portable"

create_retroarch_tree() {
    log "Creating the RetroArch tree under $RA_ROOT"
    local d
    for d in $RA_SUBDIRS; do
        run mkdir -p "$RA_ROOT/$d"
    done
    log "Creating the roms/ folders (one per system, named as RetroArch's playlists are)"
    printf '%s
' "$RA_ROM_SYSTEMS" | while IFS= read -r d; do
        [ -n "$d" ] || continue
        run mkdir -p "$RA_ROOT/roms/$d"
    done
}

#*******************************
# write_retroarch_config
#*******************************
# Only when there is none yet: RetroArch rewrites this file itself on exit, and AutoBleem edits a few keys
# around each launch (LaunchService::transferRaConfig) - both must keep what the user has set since.
write_retroarch_config() {
    local cfg="$RA_ROOT/retroarch.cfg"
    if [ -f "$cfg" ]; then
        log "Keeping the existing $cfg"
        return 0
    fi
    log "Writing $cfg"
    write_file "$cfg" <<EOF
# Written by AutoBleem's installer. RetroArch keeps this file up to date itself; AutoBleem edits a few
# display keys around each launch. Every directory lives under $RA_ROOT.
libretro_directory = "$RA_ROOT/cores"
libretro_info_path = "$RA_ROOT/info"
system_directory = "$RA_ROOT/system"
rgui_browser_directory = "$RA_ROOT/roms"
core_assets_directory = "$RA_ROOT/downloads"
savefile_directory = "$RA_ROOT/saves"
savestate_directory = "$RA_ROOT/states"
playlist_directory = "$RA_ROOT/playlists"
content_database_path = "$RA_ROOT/database/rdb"
cursor_directory = "$RA_ROOT/database/cursors"
cheat_database_path = "$RA_ROOT/cheats"
assets_directory = "$RA_ROOT/assets"
joypad_autoconfig_dir = "$RA_ROOT/autoconfig"
overlay_directory = "$RA_ROOT/overlays"
video_shader_dir = "$RA_ROOT/shaders"
video_filter_dir = "$RA_ROOT/filters/video"
audio_filter_dir = "$RA_ROOT/filters/audio"
thumbnails_directory = "$RA_ROOT/thumbnails"
screenshot_directory = "$RA_ROOT/screenshots"
recording_output_directory = "$RA_ROOT/records"
recording_config_directory = "$RA_ROOT/records"
rgui_config_directory = "$RA_ROOT/config"
core_options_path = "$RA_ROOT/config/retroarch-core-options.cfg"
log_dir = "$RA_ROOT/logs"
video_fullscreen = "true"
input_autodetect_enable = "true"
menu_show_core_updater = "true"
EOF
}

#*******************************
# download_retroarch_content
#*******************************
# What RetroArch's own Online Updater would fetch, done here so it is complete before the first start: every
# core the buildbot has for armhf (its .index-extended is the list RetroArch itself uses), the core info
# files AutoBleem's RetroArch set is built from, and the menu assets, pad autoconfigs, scanner databases,
# cheats, overlays and GLSL shaders. A few hundred MB; --no-downloads skips all of it.
download_retroarch_content() {
    [ "$DO_DOWNLOADS" -eq 1 ] || { log "skipping the RetroArch cores and assets (--no-downloads)"; return 0; }

    local base=https://buildbot.libretro.com
    local cores_url="$base/nightly/linux/armhf/latest"
    local tmp=/tmp/autobleem-ra
    run mkdir -p "$tmp"

    log "RetroArch: downloading the core list"
    local index="$tmp/.index-extended"
    if ! run wget -q -O "$index" "$cores_url/.index-extended"; then
        warn "cannot reach $base - no cores or assets downloaded. Re-run later, or use RetroArch's Online Updater."
        return 0
    fi

    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would download every *_libretro.so.zip listed there into %s/cores\n' "$RA_ROOT"
    else
        local total count=0 failed=0 zip
        total="$(wc -l < "$index")"
        log "RetroArch: downloading $total cores into $RA_ROOT/cores"
        while read -r _date _crc zip; do
            [ -n "$zip" ] || continue
            count=$((count + 1))
            printf '\r    [%3d/%3d] %-50s' "$count" "$total" "$zip"
            # a re-run keeps the cores it has; RetroArch's Online Updater is the way to refresh them
            [ -f "$RA_ROOT/cores/${zip%.zip}" ] && continue
            if wget -q -O "$tmp/$zip" "$cores_url/$zip" && unzip -oq "$tmp/$zip" -d "$RA_ROOT/cores"; then
                rm -f "$tmp/$zip"
            else
                failed=$((failed + 1))
            fi
        done < "$index"
        printf '\n'
        [ "$failed" -eq 0 ] || warn "$failed cores did not download - RetroArch's Online Updater can fetch them later"
    fi

    # bundle -> where it unpacks. info/ is what AutoBleem reads to know which core plays what.
    local bundle dest
    for bundle in info:info assets:assets autoconfig:autoconfig database-rdb:database/rdb \
                  database-cursors:database/cursors cheats:cheats overlays:overlays shaders_glsl:shaders; do
        dest="${bundle#*:}"; bundle="${bundle%%:*}"
        if [ -n "$(ls -A "$RA_ROOT/$dest" 2>/dev/null)" ]; then
            log "RetroArch: $dest/ already has content - keeping it"
            continue
        fi
        log "RetroArch: $bundle -> $RA_ROOT/$dest"
        if run wget -q -O "$tmp/$bundle.zip" "$base/assets/frontend/$bundle.zip"; then
            run unzip -oq "$tmp/$bundle.zip" -d "$RA_ROOT/$dest" || warn "could not unpack $bundle.zip"
        else
            warn "could not download $bundle.zip"
        fi
        run rm -f "$tmp/$bundle.zip"
    done
    run rm -rf "$tmp"
}

#*******************************
# existing_data_partition
#*******************************
# echoes the device of the exFAT partition labelled $DATA_LABEL, if there already is one
existing_data_partition() {
    blkid -L "$DATA_LABEL" 2>/dev/null || true
}

#*******************************
# largest_free_span
#*******************************
# echoes "<start MiB> <end MiB>" of the biggest unpartitioned span on $DISK, or nothing.
# parted's machine-readable output ends free spans with ":free;" - the fields are start:end:size.
largest_free_span() {
    parted -ms "$DISK" unit MiB print free 2>/dev/null | awk -F: '
        /:free;$/ {
            start = $2 + 0; end = $3 + 0; size = end - start
            if (size > best) { best = size; bs = start; be = end }
        }
        END { if (best > 0) printf "%d %d\n", bs, be }'
}

#*******************************
# create_data_partition
#*******************************
# sets DATA_DEV. (It does not echo the device: log() writes to stdout, so a caller capturing this function's
# output would capture the progress lines too.)
create_data_partition() {
    local start="$1" end="$2"
    log "Creating a ${DATA_LABEL} partition on $DISK (${start}MiB - ${end}MiB)"
    confirm "This writes a new partition table entry to $DISK. Continue?"

    run parted -s "$DISK" mkpart primary "${start}MiB" "${end}MiB"
    run partprobe "$DISK"
    run udevadm settle

    # the new partition is the highest-numbered one on the disk
    local partnum
    partnum="$(parted -ms "$DISK" unit MiB print 2>/dev/null | awk -F: '/^[0-9]+:/ { n = $1 } END { print n }')"
    [ -n "$partnum" ] || die "cannot work out the new partition number on $DISK"

    # 0x07 is what Windows expects to see for an exFAT partition in an MBR table
    run sfdisk --part-type "$DISK" "$partnum" 7
    run partprobe "$DISK"
    run udevadm settle

    DATA_DEV="$(partition_device "$partnum")"
    log "Formatting $DATA_DEV as exFAT"
    run mkfs.exfat -n "$DATA_LABEL" "$DATA_DEV"
    run udevadm settle
}

#*******************************
# partition_device
#*******************************
# /dev/mmcblk0 + 3 -> /dev/mmcblk0p3, but /dev/sda + 3 -> /dev/sda3
partition_device() {
    local n="$1"
    case "$DISK" in
        *[0-9]) echo "${DISK}p${n}" ;;
        *)      echo "${DISK}${n}" ;;
    esac
}

#*******************************
# shrink_root
#*******************************
# Raspberry Pi OS grows the root filesystem over the whole card on first boot, so on a normal install there is
# no free space left to put the data partition in. ext4 can only be shrunk unmounted, and everything that
# runs from the root filesystem - init=, systemd, a service - runs with it mounted (Raspberry Pi OS's own
# init= resize gets away with it because growing works online; shrinking does not). So the work is done in
# the initramfs, at local-premount, when the root device is known and not yet mounted:
# system/shrink-root-hook.sh packs e2fsck/resize2fs/parted/sfdisk/mkfs.exfat into the initramfs and
# system/shrink-root-premount.sh does the shrink, restores cmdline.txt from the backup first and reboots.
# The two are removed again, and the initramfs rebuilt, by disarm_shrink() once the partition exists.
SHRINK_HOOK=/etc/initramfs-tools/hooks/autobleem-shrink
SHRINK_SCRIPT=/etc/initramfs-tools/scripts/local-premount/autobleem-shrink

shrink_root() {
    local gib="$1"
    local rootdev
    rootdev="$(findmnt -no SOURCE /)"

    log "Staging an offline shrink of $rootdev to ${gib}GiB"
    warn "This repartitions $DISK on the next boot. If it goes wrong the card may not boot."
    warn "Back up anything you care about first."
    confirm "Shrink $rootdev to ${gib}GiB and reboot now?"

    [ -d /etc/initramfs-tools ] || die "no /etc/initramfs-tools - this image does not boot through an initramfs,
    and the shrink relies on one. Shrink partition 2 from another machine (GParted) instead."

    run install -m 0755 "$SCRIPT_DIR/system/shrink-root-hook.sh" "$SHRINK_HOOK"
    run install -m 0755 "$SCRIPT_DIR/system/shrink-root-premount.sh" "$SHRINK_SCRIPT"
    # the running kernel is the one that boots next; Pi OS's post-update hook copies the result to
    # /boot/firmware/initramfs<N> itself
    log "Rebuilding the initramfs for $(uname -r) with the shrink tools in it"
    run update-initramfs -u -k "$(uname -r)" || die "update-initramfs failed - nothing armed"

    run cp -n "$BOOT_DIR/cmdline.txt" "$BOOT_DIR/cmdline.txt.autobleem-backup"

    # the premount script reads these off the kernel command line, restores cmdline.txt from the backup above
    # and reboots - whether it succeeded or not, so a failure never leaves a card that loops into it
    local cmdline
    cmdline="$(tr -d '\n' < "$BOOT_DIR/cmdline.txt")"
    cmdline="$cmdline ab_shrink_gib=$gib ab_shrink_label=$DATA_LABEL"

    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would write to %s: %s\n' "$BOOT_DIR/cmdline.txt" "$cmdline"
    else
        printf '%s\n' "$cmdline" | write_file "$BOOT_DIR/cmdline.txt"
    fi

    log "Rebooting to do the shrink (a few minutes - the console shows autobleem-shrink: lines)."
    log "Run this installer again when the Pi comes back up."
    run sync
    run reboot
    exit 0
}

#*******************************
# disarm_shrink
#*******************************
# after a shrink (or an abandoned one): take the hook and script out of the initramfs again
disarm_shrink() {
    [ -f "$SHRINK_HOOK" ] || [ -f "$SHRINK_SCRIPT" ] || return 0
    log "Removing the shrink tools from the initramfs"
    run rm -f "$SHRINK_HOOK" "$SHRINK_SCRIPT"
    run update-initramfs -u -k "$(uname -r)" || warn "update-initramfs failed - the (inert) shrink script stays in it"
}

#*******************************
# ensure_data_partition
#*******************************
# sets DATA_DEV to the exFAT partition to use, creating it if that is possible without destroying anything.
ensure_data_partition() {
    DATA_DEV="$(existing_data_partition)"
    if [ -n "$DATA_DEV" ]; then
        log "Using the existing $DATA_LABEL partition: $DATA_DEV"
        disarm_shrink
        return 0
    fi

    # everything below reads or writes the partition table
    command -v parted >/dev/null 2>&1 || die "parted is not installed (sudo apt install parted exfatprogs)"

    if [ -n "$SHRINK_ROOT_GIB" ]; then
        shrink_root "$SHRINK_ROOT_GIB"      # reboots, does not return
    fi

    local span start end size
    span="$(largest_free_span)"
    if [ -n "$span" ]; then
        start="${span% *}"; end="${span#* }"; size=$((end - start))
        if [ "$size" -ge "$MIN_DATA_MIB" ]; then
            create_data_partition "$start" "$end"
            return 0
        fi
        warn "the largest free span on $DISK is only ${size}MiB"
    fi

    die "no $DATA_LABEL partition and no room to make one.

    The root filesystem was grown over the whole card on first boot, which is normal. Pick one:

      * sudo bash install.sh --shrink-root 8     shrink the root filesystem to 8GiB and use the rest here.
                                              Repartitions the card on the next boot - back up first.
      * shrink partition 2 from another machine (GParted), then re-run this installer.
      * re-flash, and before the first boot delete the 'init=...firstboot' part of cmdline.txt on the FAT
        partition so the root filesystem is never expanded. Everything left over is then free space."
}

#*******************************
# mount_data
#*******************************
mount_data() {
    log "Mounting $DATA_DEV at $DATA_MOUNT"
    run mkdir -p "$DATA_MOUNT"

    local uuid opts fstab_line
    # in a --dry-run the partition was never created or formatted, so there is no UUID to read yet
    uuid="$(blkid -s UUID -o value "$DATA_DEV" 2>/dev/null || true)"
    [ -n "$uuid" ] || uuid="<uuid-of-$DATA_DEV>"
    # exFAT has no owners or permissions of its own: everything is owned by whoever mounts it, so the mode has
    # to be handed to the driver here. nofail keeps the Pi booting if the card is ever swapped.
    opts="defaults,noatime,nofail,uid=0,gid=0,umask=000"
    fstab_line="UUID=$uuid  $DATA_MOUNT  exfat  $opts  0  0"

    if grep -q "$DATA_MOUNT" /etc/fstab; then
        log "fstab already has an entry for $DATA_MOUNT"
    else
        run cp /etc/fstab /etc/fstab.autobleem-backup
        if [ "$DRY_RUN" -eq 1 ]; then
            printf '    would append to /etc/fstab: %s\n' "$fstab_line"
        else
            printf '%s\n' "$fstab_line" >> /etc/fstab
        fi
    fi

    mountpoint -q "$DATA_MOUNT" || run mount "$DATA_MOUNT"
}

#*******************************
# create_tree
#*******************************
# The same layout as the PSC's USB stick, because that is what the app expects: main.cpp's setupEnvironment
# derives every path from this root (Autobleem/bin/autobleem, Games, System/Databases, themes, ...).
create_tree() {
    log "Creating the AutoBleem tree under $DATA_MOUNT"
    local d
    for d in Autobleem/bin/autobleem Autobleem/bin/db Autobleem/bin/emu Autobleem/rc \
             Games System/Bios System/Databases System/Logs themes Apps; do
        run mkdir -p "$DATA_MOUNT/$d"
    done
    RA_ROOT="$DATA_MOUNT/RetroArch"
    create_retroarch_tree
    write_retroarch_config
}

#*******************************
# install_payload
#*******************************
# payload_rpi/ is the tree that goes on the data partition, laid out exactly like the console's payload/:
# Autobleem/bin/autobleem (the app and its resources), Autobleem/bin/db (covers), Autobleem/rc (the launch
# scripts), themes/, Games/, Apps/. Installing is copying it across - tools/make_rpi_package.sh is what fills
# in the parts that are built rather than checked in (the binary, the resources, the cover databases).
install_payload() {
    local app_dest="$DATA_MOUNT/Autobleem/bin/autobleem"
    # -f, not -x: the package is often built on Windows, where the executable bit does not survive the trip
    [ -f "$STAGE_DIR/Autobleem/bin/autobleem/autobleem-gui" ] || die "no autobleem-gui in \
$STAGE_DIR/Autobleem/bin/autobleem
    Build it on the PC with ./make_rpi.sh and package it with tools/make_rpi_package.sh, then run this
    installer from the unpacked package (--stage points at the tree if you keep it somewhere else)."

    # a re-install should not throw away the theme, language and aspect the user picked in the Options menu
    if [ -f "$app_dest/config.ini" ]; then
        log "Keeping the existing config.ini"
        run cp "$app_dest/config.ini" "$app_dest/config.ini.keep"
    fi

    log "Installing the AutoBleem tree"
    # cp -r, not -a: exFAT has no owners or modes to preserve (the mount forces them), and cp -a's failure
    # to preserve them is a non-zero exit even though every file was copied
    local d
    for d in Autobleem themes Games Apps RetroArch; do
        [ -d "$STAGE_DIR/$d" ] || continue
        run cp -r "$STAGE_DIR/$d/." "$DATA_MOUNT/$d/"
    done

    # exFAT has no permission bits of its own - the mount's umask=000 already makes everything 0777 - so a
    # chmod there is either a no-op or refused by the driver. Neither is a reason to stop.
    run chmod +x "$app_dest/autobleem-gui" "$DATA_MOUNT/Autobleem/bin/emu/pcsx-ab" || true
    run chmod +x "$DATA_MOUNT/Autobleem/rc/"*.sh || true

    if [ -f "$app_dest/config.ini.keep" ]; then
        run mv -f "$app_dest/config.ini.keep" "$app_dest/config.ini"
    fi

    # config.ini's Cfg= names the file LaunchService::writeSelectionScript drops the handover selection into,
    # and autobleem-session reads back. It ships as the console's own absolute path, which does not exist here.
    local cfg_path="$DATA_MOUNT/Autobleem/rc/autobleem_cfg.sh"
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would point config.ini Cfg= at %s\n' "$cfg_path"
    else
        sed -i "s|^[Cc]fg=.*|Cfg=$cfg_path|" "$app_dest/config.ini"
    fi

    [ -f "$DATA_MOUNT/Autobleem/bin/emu/pcsx-ab" ] || warn "no pcsx-ab in the package - PS1 games will fall back
    to RetroArch's pcsx_rearmed core (no AutoBleem save states). Build it with pcsx-rearmed-develop/make_rpi.sh."

    if ! ls "$STAGE_DIR/Autobleem/bin/db"/covers*.db >/dev/null 2>&1; then
        warn "no cover databases in the package - AutoBleem will warn about this on every start, and scanned
    games will have no title or cover art. Copy covers*.db into $DATA_MOUNT/Autobleem/bin/db/ and re-scan."
    fi
}

#*******************************
# install_service
#*******************************
# The launcher runs as root on tty1, the way the console runs it: it needs the DRM device, every /dev/input
# node and the power-off call, and a Pi set up this way is an appliance, not a workstation.
install_service() {
    log "Installing the systemd service"

    run install -m 0755 "$SCRIPT_DIR/system/autobleem-session.sh" /usr/local/bin/autobleem-session

    sed "s|@DATA_MOUNT@|$DATA_MOUNT|g" "$SCRIPT_DIR/system/autobleem.service" \
        | write_file /etc/systemd/system/autobleem.service

    # tty1 is the launcher's screen; a getty there would fight it for the terminal and the keyboard.
    # tty2..tty6 are untouched, so Alt+F2 still gives a login prompt if the launcher ever fails to start.
    # (it may already be disabled, or never have been enabled - neither is a reason to stop)
    run systemctl disable --now getty@tty1.service || true
    run systemctl set-default multi-user.target
    run systemctl daemon-reload
    run systemctl enable autobleem.service
}

#*******************************
# configure_boot
#*******************************
configure_boot() {
    [ "$DO_BOOT_CONFIG" -eq 1 ] || { log "skipping boot config (--no-boot-config)"; return 0; }

    log "Configuring $BOOT_DIR/cmdline.txt"
    run cp -n "$BOOT_DIR/cmdline.txt" "$BOOT_DIR/cmdline.txt.autobleem-backup"

    local cmdline extra
    cmdline="$(tr -d '\n' < "$BOOT_DIR/cmdline.txt")"

    # consoleblank=0: without it the framebuffer blanks after 10 minutes and a game looks like a crash.
    extra="consoleblank=0"
    if [ "$QUIET_BOOT" -eq 1 ]; then
        extra="$extra quiet loglevel=3 logo.nologo vt.global_cursor_default=0"
    fi

    local word
    for word in $extra; do
        case " $cmdline " in
            *" ${word%%=*}="*|*" $word "*) ;;                 # already set, leave whatever value is there
            *) cmdline="$cmdline $word" ;;
        esac
    done

    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would write to %s: %s\n' "$BOOT_DIR/cmdline.txt" "$cmdline"
    else
        printf '%s\n' "$cmdline" | write_file "$BOOT_DIR/cmdline.txt"
    fi
}

#*******************************
# summary
#*******************************
summary() {
    log "Done."
    cat <<EOF

  Games go in      $DATA_MOUNT/Games/<game name>/      (one folder per game, .cue+.bin / .pbp / .chd)
  BIOS goes in     $DATA_MOUNT/System/Bios/            (romw.bin, plus romJP.bin for Japanese games - a copy
                                                       of romw.bin will do. Without them pcsx-ab uses HLE.)
  RetroArch        $DATA_MOUNT/RetroArch/    roms/<system>/ for its games (a folder per system is there),
                                              system/ for the cores' BIOS files, then cores/ info/ saves/
                                              states/ playlists/ ... the standard layout. After copying games
                                              in: RetroArch -> Import Content -> Scan Directory -> roms
  Logs             $DATA_MOUNT/System/Logs/
  Themes           $DATA_MOUNT/themes/

  The $DATA_LABEL partition is exFAT, so you can pull the card and drop games on it from Windows, macOS or
  Linux. Windows shows it as a second drive next to the small boot partition (Windows 10 1903 and newer).

  Start it now without rebooting:   sudo systemctl start autobleem
  Watch what it does:               sudo journalctl -u autobleem -f
  Stop it owning the screen:        sudo systemctl disable --now autobleem && sudo systemctl enable --now getty@tty1

  Alt+F2 gives you a login prompt if the launcher ever fails to come up. Enabling SSH before you reboot is a
  good idea: sudo raspi-config -> Interface Options -> SSH.

  Power the Pi off from the launcher's L2+R2 menu (Power Off) or with "sudo poweroff", not by pulling the
  plug: an unclean shutdown can leave freshly written files empty.

EOF
    if [ "$DRY_RUN" -eq 1 ]; then
        warn "this was a --dry-run: nothing above was actually changed"
    fi
}

#*******************************
# main
#*******************************
main() {
    parse_args "$@"
    preflight
    install_packages
    ensure_data_partition       # may arm --shrink-root and reboot: everything slow comes after it
    mount_data
    create_tree
    install_retroarch
    download_retroarch_content
    install_payload
    install_service
    configure_boot
    sync
    summary
}

main "$@"
