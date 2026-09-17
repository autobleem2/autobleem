#!/usr/bin/env bash
#
# AutoBleem for Raspberry Pi - installer for Raspberry Pi OS Lite (32-bit).
#
# Turns a plain Lite install into an AutoBleem appliance: an exFAT data partition that behaves like the
# PlayStation Classic's USB stick (drop games onto it from Windows/macOS/Linux), the launcher started at boot
# on tty1 with no desktop, and RetroArch behind it.
#
# Run it from the unpacked release package:   sudo bash install.sh
# See README.md in this directory for the whole story, including what is not ported yet.
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
DO_PACKAGES=1
DO_BOOT_CONFIG=1
QUIET_BOOT=1                    # strip the kernel log/rainbow splash so the launcher is the only thing seen
FETCH_CORES=0                   # download libretro cores from buildbot.libretro.com when apt has none

#*******************************
# output helpers
#*******************************
log()  { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[!]\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31m[x]\033[0m %s\n' "$*" >&2; exit 1; }

# every command that changes the machine goes through this, so --dry-run is honest
run() {
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would run: %s\n' "$*"
    else
        "$@"
    fi
}

confirm() {
    local prompt="$1"
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would ask: %s\n' "$prompt"
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
  --fetch-cores        download missing libretro cores from buildbot.libretro.com (third-party binaries)
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
            --fetch-cores)    FETCH_CORES=1; shift ;;
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
# install_packages
#*******************************
install_packages() {
    [ "$DO_PACKAGES" -eq 1 ] || { log "skipping apt (--no-packages)"; return 0; }

    log "Installing packages"
    # not fatal: a Pi with no network but a warm apt cache can still have everything that is needed
    if ! run apt-get update; then
        warn "apt-get update failed - carrying on with whatever is already cached"
    fi

    # SDL2 is what autobleem-gui draws with; exfatprogs formats the data partition; parted creates it.
    run apt-get install -y \
        libsdl2-2.0-0 libsdl2-image-2.0-0 libsdl2-mixer-2.0-0 libsdl2-ttf-2.0-0 \
        exfatprogs parted alsa-utils

    # RetroArch is the second half of the launcher (the RetroArch set, and the fallback PS1 emulator until
    # pcsx-ab is ported). Not fatal if the repo has no package: the launcher just has nothing to hand off to.
    if ! run apt-get install -y retroarch; then
        warn "could not install retroarch from apt - the RetroArch set and the PS1 fallback will not work"
    fi

    install_ps1_core
}

#*******************************
# install_ps1_core
#*******************************
# pcsx_rearmed is what actually runs PS1 games until pcsx-ab is ported to the Pi (see rc/launch.sh).
install_ps1_core() {
    if run apt-get install -y libretro-pcsx-rearmed; then
        log "PS1 core: libretro-pcsx-rearmed (apt)"
        return 0
    fi

    if [ "$FETCH_CORES" -eq 0 ]; then
        warn "apt has no libretro-pcsx-rearmed. PS1 games will not launch until a core is installed.
    Re-run with --fetch-cores to download one from buildbot.libretro.com, or install a core by hand into
    /usr/lib/arm-linux-gnueabihf/libretro/."
        return 0
    fi

    local url=https://buildbot.libretro.com/nightly/linux/armhf/latest/pcsx_rearmed_libretro.so.zip
    local dest=/usr/lib/arm-linux-gnueabihf/libretro
    log "Downloading pcsx_rearmed core from buildbot.libretro.com"
    run apt-get install -y wget unzip
    run mkdir -p "$dest"
    if run wget -q -O /tmp/pcsx_rearmed.zip "$url" && run unzip -o /tmp/pcsx_rearmed.zip -d "$dest"; then
        log "PS1 core: $dest/pcsx_rearmed_libretro.so"
    else
        warn "download failed - PS1 games will not launch until a core is installed"
    fi
    run rm -f /tmp/pcsx_rearmed.zip
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
# no free space left to put the data partition in. ext4 cannot be shrunk while it is mounted, so the work is
# done the same way Raspberry Pi OS does its own resize: a script runs as init= before the root filesystem is
# mounted read-write, then reboots. system/shrink-root-init.sh is that script.
shrink_root() {
    local gib="$1"
    local rootdev
    rootdev="$(findmnt -no SOURCE /)"

    log "Staging an offline shrink of $rootdev to ${gib}GiB"
    warn "This repartitions $DISK on the next boot. If it goes wrong the card may not boot."
    warn "Back up anything you care about first."
    confirm "Shrink $rootdev to ${gib}GiB and reboot now?"

    run install -m 0755 "$SCRIPT_DIR/system/shrink-root-init.sh" "$BOOT_DIR/autobleem-shrink.sh"
    run cp -n "$BOOT_DIR/cmdline.txt" "$BOOT_DIR/cmdline.txt.autobleem-backup"

    # the init script reads these off the kernel command line, restores cmdline.txt from the backup above and
    # reboots - whether it succeeded or not, so a failure never leaves an unbootable card
    # init= is a path the *kernel* resolves inside the root filesystem, but /boot(/firmware) is the FAT
    # partition mounted there, so the script has to be named by the path it has once the system is up
    local cmdline
    cmdline="$(tr -d '\n' < "$BOOT_DIR/cmdline.txt")"
    cmdline="$cmdline ab_shrink_gib=$gib ab_shrink_label=$DATA_LABEL init=$BOOT_DIR/autobleem-shrink.sh"

    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would write to %s: %s\n' "$BOOT_DIR/cmdline.txt" "$cmdline"
    else
        printf '%s\n' "$cmdline" > "$BOOT_DIR/cmdline.txt"
    fi

    log "Rebooting to do the shrink. Run this installer again when the Pi comes back up."
    run sync
    run reboot
    exit 0
}

#*******************************
# ensure_data_partition
#*******************************
# sets DATA_DEV to the exFAT partition to use, creating it if that is possible without destroying anything.
ensure_data_partition() {
    DATA_DEV="$(existing_data_partition)"
    if [ -n "$DATA_DEV" ]; then
        log "Using the existing $DATA_LABEL partition: $DATA_DEV"
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
             Games System/Databases System/Logs themes Apps retroarch/saves retroarch/config; do
        run mkdir -p "$DATA_MOUNT/$d"
    done
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
    local d
    for d in Autobleem themes Games Apps; do
        [ -d "$STAGE_DIR/$d" ] || continue
        run cp -a "$STAGE_DIR/$d/." "$DATA_MOUNT/$d/"
    done

    # exFAT has no permission bits of its own - the mount's umask=000 already makes everything 0777 - so a
    # chmod there is either a no-op or refused by the driver. Neither is a reason to stop.
    run chmod +x "$app_dest/autobleem-gui" || true
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

    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would write /etc/systemd/system/autobleem.service (root=%s)\n' "$DATA_MOUNT"
    else
        sed "s|@DATA_MOUNT@|$DATA_MOUNT|g" "$SCRIPT_DIR/system/autobleem.service" \
            > /etc/systemd/system/autobleem.service
    fi

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
        printf '%s\n' "$cmdline" > "$BOOT_DIR/cmdline.txt"
    fi
}

#*******************************
# summary
#*******************************
summary() {
    log "Done."
    cat <<EOF

  Games go in      $DATA_MOUNT/Games/<game name>/      (one folder per game, .cue+.bin / .pbp / .chd)
  Logs             $DATA_MOUNT/System/Logs/
  Themes           $DATA_MOUNT/themes/

  The $DATA_LABEL partition is exFAT, so you can pull the card and drop games on it from Windows, macOS or
  Linux. Windows shows it as a second drive next to the small boot partition (Windows 10 1903 and newer).

  Start it now without rebooting:   sudo systemctl start autobleem
  Watch what it does:               sudo journalctl -u autobleem -f
  Stop it owning the screen:        sudo systemctl disable --now autobleem && sudo systemctl enable --now getty@tty1

  Alt+F2 gives you a login prompt if the launcher ever fails to come up. Enabling SSH before you reboot is a
  good idea: sudo raspi-config -> Interface Options -> SSH.

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
    ensure_data_partition
    mount_data
    create_tree
    install_payload
    install_service
    configure_boot
    summary
}

main "$@"
