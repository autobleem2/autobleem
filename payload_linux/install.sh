#!/usr/bin/env bash
#
# AutoBleem for a Linux appliance - the installer for a Raspberry Pi (Raspberry Pi OS Lite, 32-bit armhf or
# 64-bit arm64) and for the 32-bit PC USB stick (Debian 12 Bookworm i386).
#
# Turns a plain minimal install into an AutoBleem appliance: an exFAT data partition that behaves like the
# PlayStation Classic's USB stick (drop games onto it from Windows/macOS/Linux), the launcher started at boot
# on tty1 with no desktop, and RetroArch behind it. The package this unpacks from is built for one
# architecture (tools/make_rpi_package.sh --arch armhf|arm64|i386); this installer detects which at runtime
# and picks the matching RetroArch builds, and detects the platform - a Pi (its firmware boot partition,
# cmdline.txt/config.txt) or a PC (GRUB) - for the boot setup. The platform-specific parts are the
# *_rpi / *_pcusb functions; everything else is shared.
#
# Run it from the unpacked release package:   sudo bash install.sh
# See README.md in this directory for the whole story, including what has and has not run on real hardware.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

#*******************************
# defaults
#*******************************
PLATFORM=""                     # rpi | pcusb, set by detect_platform() (--platform overrides)
PLATFORM_DIR=""                 # the download repository's folder for this platform's RetroArch builds and
                                # cores: rpi (armhf, arm64) or pc (i386)
ARCH=""                         # armhf | arm64 | i386, set by preflight() from dpkg --print-architecture
RA_ARCH=""                      # same as buildbot.libretro.com spells it (aarch64, x86), for download_retroarch_content()
BOOT_DIR=""                     # a Pi's firmware partition (/boot/firmware) or a PC's ESP (/boot/efi), set by preflight()
MACHINE="the machine"           # how the messages name this computer: "the Pi" / "the PC" (detect_platform)
MEDIUM="the disk"               # ...and what it boots from: "the card" / "the stick" - a PC user must never read "Pi"
DATA_LABEL="AUTOBLEEM"          # the exFAT partition's label - also how the README tells users to find it
DATA_MOUNT="/media/autobleem"
MIN_DATA_MIB=2048               # refuse to make a data partition smaller than this - it holds every game
SHRINK_ROOT_GIB=""              # --shrink-root: repartition, see shrink_root() (opt-in, it is destructive)
GROW_ROOT_GIB=""                # --grow-root: grow a still-small root to this size first, see grow_root()
GROW_ONLY=0                     # --grow-only: stop right after the root is grown (the first-boot script's use)
STAGE_DIR="$SCRIPT_DIR"         # the payload tree to install (Autobleem/, Themes/, Games/, Apps/)
DISK=""                         # --disk: the SD card / USB stick. autodetected from where the boot files or the root live
DRY_RUN=0
ASSUME_YES=0                    # --yes: answer every confirmation, for unattended runs over ssh
DO_PACKAGES=1
DO_BOOT_CONFIG=1
QUIET_BOOT=1                    # strip the kernel log/rainbow splash so the launcher is the only thing seen
BOOT_SPLASH=1                   # plymouth with system/plymouth/ (the AutoBleem logo) from the initramfs on; needs QUIET_BOOT
HDMI_MODE="1920x1080@60"        # --hdmi-mode: the KMS mode for the whole boot, so plymouth and the launcher share it (a Pi;
                                # a PC's KMS driver takes the screen's native mode and the option is ignored)
RETROARCH_MODE=prebuilt         # --retroarch: prebuilt (from the download repository) | source | apt | none
REPO_URL="${AB_REPO_URL:-https://autobleem.retromenele.pl}"   # --repo: the download repository (CLAUDE.md, "The download repository")
DO_DOWNLOADS=1                  # --no-downloads: skip the RetroArch cores/assets from buildbot.libretro.com
DO_BIOS=1                       # --no-bios: skip the BIOS pack (system/biospack*.txt, from github.com/Abdess/retrobios)
DO_SAMPLES=1                    # --no-samples: skip the sample games (samples/ on the download repository)
UPDATE_MODE=0                   # --update: a re-run over an installed Pi from a newer package (the launcher's
                                # online update, system/autobleem-update.sh): unattended, RetroArch replaced from
                                # the repository only when it is installed, nothing repartitioned
RETROARCH_TARBALL=""            # --retroarch-tarball: a prebuilt RetroArch already downloaded (the online update)
THUMBNAILS=none                 # --thumbnails: none (the launcher fetches each game's cover itself) | boxarts (the whole
                                # PS1 set, ~350 MB, for covers offline) | all (+ title screens, snaps)
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

  --platform P         rpi (a Raspberry Pi) or pcusb (the 32-bit PC stick); detected when not given
  --disk DEVICE        SD card / USB stick to put the data partition on (default: the disk the boot
                       files - or, on a PC, the root filesystem - are on)
  --mount PATH         where to mount it (default: /media/autobleem)
  --stage DIR          the payload tree to install from (default: this directory)
  --shrink-root GIB    shrink the root filesystem to GIB and use the freed space for the data partition.
                       REPARTITIONS THE CARD. Needs a reboot to do the work offline. Back up first.
  --grow-root GIB      the opposite case - a root filesystem that was never expanded (an image built by
                       tools/make_rpi_image.sh, or a card whose cmdline.txt lost the word "resize" before
                       the first boot): grow it to GIB, online, and leave the rest for the data partition.
                       Capped so at least 2 GiB stay free for the data partition; a no-op when the root is
                       already that big.
  --grow-only          do only that and stop - for a root too small to even unpack the package on
                       (autobleem-firstboot.sh runs this before extracting the tarball, then the full install)
  --no-packages        skip apt - assume SDL2/RetroArch/exfatprogs are already installed
  --no-boot-config     do not touch cmdline.txt/config.txt
  --no-quiet-boot      keep the kernel messages and rainbow splash on screen while booting (no boot splash then)
  --no-boot-splash     boot quietly but without the AutoBleem logo (no plymouth)
  --hdmi-mode MODE     a Pi: the HDMI mode set on the kernel command line for the whole boot (default:
                       1920x1080@60; the launcher draws its 1280x720 UI at 1.5x on a 1080p screen, covers and
                       text sharp; 1280x720@60 for a 720p screen; "none" leaves the screen's preferred mode).
                       A PC always boots in the screen's native mode - the option is ignored there
  --retroarch MODE     prebuilt (default): the newest RetroArch release built for this architecture by
                       AutoBleem's download repository (--repo), unpacked in seconds - falls back to source
                       when the repository cannot be reached; source: build the latest release here, 10-40
                       min on a Pi; apt: the distribution's package; none: leave RetroArch alone
  --repo URL           the download repository (default: $AB_REPO_URL or AutoBleem's)
  --thumbnails WHAT    none (default): the launcher fetches a game's cover from thumbnails.libretro.com when
                       it scans the game, so nothing is mirrored here; boxarts: the whole PS1 set (~9000
                       files, ~350 MB, resumable, four streams) for covers offline; all: title screens and
                       snaps too (~3x)
  --no-downloads       do not download the RetroArch cores, core info, assets, databases from
                       buildbot.libretro.com (a few hundred MB; RetroArch's Online Updater can do it later)
  --no-bios            do not download the BIOS pack (system/biospack.txt, biospack-arm64.txt or -i386.txt: ~190-230 MB
                       of console, computer, arcade and ScummVM files from github.com/Abdess/retrobios into RetroArch/system, and
                       the PS1 BIOS for pcsx-ab into System/Bios)
  --no-samples         do not install the sample games (a 1 MB pack from the download repository: a homebrew
                       PS1 game in Games/ and, with RetroArch, NES/SNES/Mega Drive homebrew in RetroArch/roms/,
                       each with its box art - so the shelf is not empty on the first start; SAMPLES.md on the
                       data partition says what they are and under which licences)
  --update             the online update's re-run over an installed Pi (autobleem-update): --yes, RetroArch
                       from the repository when one is installed here (--retroarch none otherwise), no
                       repartitioning; everything already there is kept (config.ini, cores, BIOS, samples)
  --retroarch-tarball FILE  install this prebuilt RetroArch tarball (already downloaded and checked) instead
                       of fetching one - what the online update hands over
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
            --platform)       PLATFORM="${2:?--platform needs rpi or pcusb}"; shift 2 ;;
            --disk)           DISK="${2:?--disk needs a device}"; shift 2 ;;
            --mount)          DATA_MOUNT="${2:?--mount needs a path}"; shift 2 ;;
            --stage)          STAGE_DIR="${2:?--stage needs a directory}"; shift 2 ;;
            --shrink-root)    SHRINK_ROOT_GIB="${2:?--shrink-root needs a size in GiB}"; shift 2 ;;
            --grow-root)      GROW_ROOT_GIB="${2:?--grow-root needs a size in GiB}"; shift 2 ;;
            --grow-only)      GROW_ONLY=1; shift ;;
            --no-packages)    DO_PACKAGES=0; shift ;;
            --no-boot-config) DO_BOOT_CONFIG=0; shift ;;
            --no-quiet-boot)  QUIET_BOOT=0; BOOT_SPLASH=0; shift ;;
            --no-boot-splash) BOOT_SPLASH=0; shift ;;
            --hdmi-mode)      HDMI_MODE="${2:?--hdmi-mode needs a mode such as 1280x720@60, or none}"; shift 2 ;;
            --retroarch)      RETROARCH_MODE="${2:?--retroarch needs prebuilt, source, apt or none}"; shift 2 ;;
            --repo)           REPO_URL="${2:?--repo needs a URL}"; shift 2 ;;
            --no-downloads)   DO_DOWNLOADS=0; shift ;;
            --no-bios)        DO_BIOS=0; shift ;;
            --no-samples)     DO_SAMPLES=0; shift ;;
            --update)         UPDATE_MODE=1; ASSUME_YES=1; shift ;;
            --retroarch-tarball) RETROARCH_TARBALL="${2:?--retroarch-tarball needs a file}"; shift 2 ;;
            --thumbnails)     THUMBNAILS="${2:?--thumbnails needs boxarts, all or none}"; shift 2 ;;
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

    if [ "$UPDATE_MODE" -eq 1 ]; then
        # the online update: over what is here. RetroArch is replaced from the repository only where the
        # prebuilt/source stamp says one is installed (an apt one stays apt's, a PS1-only Pi stays that)
        SHRINK_ROOT_GIB=""
        GROW_ROOT_GIB=""
        if [ -n "$RETROARCH_TARBALL" ] || { [ -f /usr/local/share/autobleem/retroarch.version ] && [ -x /usr/local/bin/retroarch ]; }; then
            RETROARCH_MODE=prebuilt
        else
            RETROARCH_MODE=none
        fi
        log "Update mode: RetroArch $RETROARCH_MODE, nothing repartitioned"
    fi

    # The package is built for one architecture (armhf: toolchains/rpi/RPitoolchain.cmake, arm64:
    # toolchains/rpi64/RPi64toolchain.cmake, i386: toolchains/pcusb/PcUsbToolchain.cmake) and the binaries
    # it stages only run on a matching userland. RA_ARCH feeds download_retroarch_content()'s buildbot path -
    # buildbot.libretro.com calls them "aarch64" and "x86", not Debian's "arm64" and "i386"
    # (dpkg --print-architecture), so the two are not the same string.
    ARCH="$(dpkg --print-architecture)"
    case "$ARCH" in
        armhf) RA_ARCH=armhf ;;
        arm64) RA_ARCH=aarch64 ;;
        i386)  RA_ARCH=x86 ;;
        *) die "unsupported architecture '$ARCH' - AutoBleem is built for armhf (32-bit) or arm64 (64-bit)
    Raspberry Pi OS Lite, and for i386 Debian (the PC stick)." ;;
    esac
    log "Architecture: $ARCH"

    detect_platform
    case "$PLATFORM" in
        rpi)   PLATFORM_DIR=rpi; preflight_rpi ;;
        pcusb) PLATFORM_DIR=pc;  preflight_pcusb ;;
    esac

    if [ -r /etc/os-release ]; then
        # shellcheck disable=SC1091
        . /etc/os-release
        log "OS: ${PRETTY_NAME:-unknown}"
    fi

    if systemctl list-unit-files 2>/dev/null | grep -qE '^(lightdm|gdm3|sddm)\.service'; then
        warn "a display manager is installed - this is not the Lite image. AutoBleem will still be set up to
    own tty1, but the desktop may fight it for the screen. 'sudo systemctl disable lightdm' if it does."
    fi

    [ -b "$DISK" ] || die "not a block device: $DISK"
    log "Disk: $DISK"
}

#*******************************
# detect_platform
#*******************************
# A Raspberry Pi has a device tree with its model in it; the PC stick is an i386 Debian. --platform
# overrides (a Pi-shaped board that is not a Pi, a PC install to try the Pi paths on).
detect_platform() {
    if [ -n "$PLATFORM" ]; then
        case "$PLATFORM" in
            rpi|pcusb) log "Platform: $PLATFORM (--platform)" ;;
            *) die "--platform takes rpi or pcusb (got '$PLATFORM')" ;;
        esac
        name_platform
        return 0
    fi
    if [ -r /proc/device-tree/model ] && grep -qi 'raspberry' /proc/device-tree/model; then
        PLATFORM=rpi
        log "Platform: Raspberry Pi - $(tr -d '\0' < /proc/device-tree/model)"
    elif [ "$ARCH" = i386 ] || [ "$ARCH" = amd64 ]; then
        PLATFORM=pcusb
        log "Platform: PC ($ARCH)"
    elif [ -r /proc/device-tree/model ]; then
        PLATFORM=rpi
        warn "not a Raspberry Pi by its model ($(tr -d '\0' < /proc/device-tree/model)) - taking the Pi's paths anyway"
    else
        die "cannot tell what this machine is (no /proc/device-tree/model, not x86) - say --platform rpi|pcusb"
    fi
    name_platform
}

# the words the messages use for this computer and what it boots from
name_platform() {
    case "$PLATFORM" in
        rpi)   MACHINE="the Pi"; MEDIUM="the card" ;;
        pcusb) MACHINE="the PC"; MEDIUM="the stick" ;;
    esac
}

# a Pi: the firmware partition holds cmdline.txt/config.txt, and the card is whatever it is on
preflight_rpi() {
    BOOT_DIR=/boot/firmware
    [ -d "$BOOT_DIR" ] || BOOT_DIR=/boot     # pre-bookworm images keep the firmware files in /boot
    [ -f "$BOOT_DIR/cmdline.txt" ] || die "no cmdline.txt under $BOOT_DIR - is this Raspberry Pi OS?"
    log "Boot files: $BOOT_DIR"
    if [ -z "$DISK" ]; then
        local bootpart
        bootpart="$(findmnt -no SOURCE "$BOOT_DIR")" || die "cannot tell which device $BOOT_DIR is on"
        DISK="/dev/$(lsblk -no PKNAME "$bootpart")"
    fi
}

# the PC stick: GRUB boots it (BIOS or UEFI), the ESP is /boot/efi when there is one (it is the image's
# first partition, mounted by label whichever way the machine booted), and the stick is the disk the root
# filesystem is on
preflight_pcusb() {
    command -v update-grub >/dev/null 2>&1 || warn "no update-grub - the boot setup (quiet boot, splash) will be skipped"
    if findmnt -no SOURCE /boot/efi >/dev/null 2>&1; then
        BOOT_DIR=/boot/efi
    else
        BOOT_DIR=/boot
    fi
    log "Boot files: $BOOT_DIR (GRUB)"
    if [ -z "$DISK" ]; then
        local rootpart
        rootpart="$(findmnt -no SOURCE /)" || die "cannot tell which device / is on"
        rootpart="${rootpart%%\[*}"   # a bind/subvol mount shows as /dev/sdX2[/path]
        DISK="/dev/$(lsblk -no PKNAME "$rootpart")"
    fi
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
        # a package apt can install - "apt-cache show" alone is not the test: it succeeds, silently, for a
        # name that only exists as something another package Provides (libasound2 on Trixie, libasound2t64's
        # virtual name), which apt-get then refuses to install
        if apt-cache policy "$name" 2>/dev/null | grep -q '^  Candidate: [^(]'; then
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

    # SDL2 is what autobleem-gui draws with (pcsx-ab too, plus libpng16 for its screenshots and skin).
    # libgl1 + libgl1-mesa-dri (with libegl1/libgles2/libgbm1) are the GL SDL's "opengl" renderer dlopens on
    # KMS: SDL2 does not depend on them, and without libGL.so.1 it silently falls back to a context with no
    # shaders and no render targets - the launcher runs and shows a black screen (the first 64-bit image,
    # 2026-09-20; on the 32-bit card they had come in with RetroArch's source-build packages). exfatprogs
    # formats the data partition; parted creates it; wget/unzip fetch the RetroArch cores.
    run apt-get install -y \
        libsdl2-2.0-0 libsdl2-image-2.0-0 libsdl2-mixer-2.0-0 libsdl2-ttf-2.0-0 \
        libgl1 libgl1-mesa-dri libegl1 libgles2 libgbm1 \
        "$(pkg_first_available libpng16-16t64 libpng16-16)" zlib1g \
        exfatprogs parted alsa-utils wget unzip ca-certificates

    # the boot splash. Lite ships without plymouth; the "script" plugin the theme uses is in the core package.
    if [ "$BOOT_SPLASH" -eq 1 ]; then
        run apt-get install -y plymouth || warn "plymouth did not install - there will be no boot splash"
    fi
}

#*******************************
# install_retroarch
#*******************************
# RetroArch is the second half of the launcher: the RetroArch set and playlists, "RetroArch" in the L2+R2
# system menu, and a PS1 game's "Play using RA" option. libretro's buildbot has every core for armhf and
# arm64 but no frontend build, and the distribution's package trails the releases, so the default is the
# newest release AutoBleem's download repository has built for this architecture (ci/build_retroarch.sh -
# the same configure as the source build here, seconds instead of 10-40 minutes), else that source build.
# --retroarch apt takes the distribution's instead; --retroarch none leaves whatever is installed alone.
install_retroarch() {
    case "$RETROARCH_MODE" in
        none)
            log "leaving RetroArch alone (--retroarch none)"
            return 0 ;;
        apt)
            install_retroarch_apt
            return 0 ;;
        prebuilt)
            install_retroarch_prebuilt && return 0
            warn "no prebuilt RetroArch - building it from source instead"
            ;&
        source)
            install_retroarch_source || {
                if [ "$PLATFORM" = pcusb ]; then
                    # Debian's retroarch package is the desktop build - Qt and GTK behind it, ~400 MB the
                    # appliance never uses (the owner's call, 2026-09-20): a PC stick that can reach neither
                    # the site nor github.com goes without, and gets RetroArch on a later run
                    warn "building RetroArch failed - going on without it (run the installer again to retry)"
                    RETROARCH_MODE=none
                else
                    warn "building RetroArch failed - installing the distribution's package instead"
                    install_retroarch_apt
                fi
            }
            return 0 ;;
        *)
            die "--retroarch takes prebuilt, source, apt or none (got '$RETROARCH_MODE')" ;;
    esac
}

#*******************************
# install_retroarch_prebuilt
#*******************************
# <repo>/rpi/retroarch/latest.json names the newest build per architecture; its tarball is `make install`ed
# RetroArch as files under usr/local (plus etc/retroarch.cfg), unpacked over /, with the runtime packages it
# needs listed in usr/local/share/autobleem/retroarch.depends (Bookworm names - the t64 spelling is tried
# too, for Trixie) and its version in retroarch.version, the same stamp the source build writes.
install_retroarch_prebuilt() {
    local stamp=/usr/local/share/autobleem/retroarch.version
    local latest=/tmp/autobleem-retroarch-latest.json
    local version url sha
    if [ -n "$RETROARCH_TARBALL" ]; then
        # the online update downloaded and checked it already (UpdateService): unpack it as it is
        [ -f "$RETROARCH_TARBALL" ] || { warn "no such file: $RETROARCH_TARBALL"; return 1; }
        log "RetroArch: installing $RETROARCH_TARBALL"
        run tar -xzf "$RETROARCH_TARBALL" -C / --no-same-owner || return 1
        install_retroarch_depends
        hash -r
        log "RetroArch: installed $(cat "$stamp" 2>/dev/null || echo "?") as /usr/local/bin/retroarch"
        return 0
    fi
    command -v python3 >/dev/null 2>&1 || { warn "python3 is needed to read the repository's index"; return 1; }
    log "RetroArch: asking $REPO_URL for a build for $ARCH"
    if ! wget -q -O "$latest" "$REPO_URL/$PLATFORM_DIR/retroarch/latest.json"; then
        warn "cannot reach $REPO_URL/$PLATFORM_DIR/retroarch/latest.json"
        return 1
    fi
    read -r version url sha < <(python3 - "$latest" "$ARCH" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
a = d.get(sys.argv[2]) or {}
print(d.get("version", ""), a.get("url", ""), a.get("sha256", ""))
PY
    )
    [ -n "$url" ] || { warn "the repository has no RetroArch build for $ARCH"; return 1; }
    if [ -f "$stamp" ] && [ "$(cat "$stamp")" = "$version" ] && [ -x /usr/local/bin/retroarch ]; then
        log "RetroArch $version is already installed"
        return 0
    fi
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would download %s, check its sha256, unpack it over / and apt-get install what it needs\n' "$url"
        return 0
    fi
    local tarball=/tmp/autobleem-retroarch.tar.gz
    log "RetroArch: downloading $version ($url)"
    if ! wget -q -O "$tarball.part" "$url" || [ "$(sha256sum "$tarball.part" | cut -d' ' -f1)" != "$sha" ]; then
        warn "the download failed or its sha256 does not match"
        rm -f "$tarball.part"
        return 1
    fi
    mv -f "$tarball.part" "$tarball"
    run tar -xzf "$tarball" -C / --no-same-owner || { rm -f "$tarball"; return 1; }
    rm -f "$tarball"
    install_retroarch_depends
    hash -r
    log "RetroArch: installed $version as /usr/local/bin/retroarch"
}

# the runtime packages a prebuilt RetroArch lists in retroarch.depends (Bookworm names; the t64 spelling
# is tried too, for Trixie)
install_retroarch_depends() {
    [ "$DO_PACKAGES" -eq 1 ] && [ -f /usr/local/share/autobleem/retroarch.depends ] || return 0
    local pkgs=() p
    while read -r p; do
        [ -n "$p" ] && pkgs+=("$(pkg_first_available "$p" "${p}t64")")
    done < /usr/local/share/autobleem/retroarch.depends
    run apt-get install -y "${pkgs[@]}" \
        || warn "not every library RetroArch needs could be installed (${pkgs[*]}) - it may not start"
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
    log "RetroArch: building $tag from source (this takes a while on $MACHINE)"

    if [ "$DO_PACKAGES" -eq 1 ]; then
        # KMS/EGL/GLES output, udev pads, ALSA sound. No X11, no Wayland, no Qt, no ffmpeg recording. A PC's
        # Mesa drivers speak desktop OpenGL too, which more cores and shaders expect than GLES.
        local gl_dev=""
        [ "$PLATFORM" = pcusb ] && gl_dev="$(pkg_first_available libgl-dev libgl1-mesa-dev)"
        # shellcheck disable=SC2086
        run apt-get install -y build-essential git pkg-config \
            libasound2-dev libudev-dev libusb-1.0-0-dev libgbm-dev libdrm-dev \
            "$(pkg_first_available libegl-dev libegl1-mesa-dev)" \
            "$(pkg_first_available libgles-dev libgles2-mesa-dev)" $gl_dev \
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
    # desktop OpenGL on a PC, GLES on a Pi - never both (see ci/build_retroarch.sh)
    local gl_flags="--enable-opengles --enable-opengles3"
    [ "$PLATFORM" = pcusb ] && gl_flags="--enable-opengl --disable-opengles --disable-opengles3"
    (
        cd "$src" || exit 1
        exec </dev/null
        # shellcheck disable=SC2086
        ./configure --prefix=/usr/local \
            --disable-x11 --disable-wayland --disable-videocore --disable-vulkan --disable-qt \
            --disable-ffmpeg --disable-jack --disable-oss --disable-pulse --disable-sdl \
            --enable-sdl2 --enable-kms --enable-egl $gl_flags \
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
# scanner's job obvious, and tell the user where a NES or a Mega Drive game goes. Every system here has a
# core on the armhf buildbot, and the BIOS pack (tools/biospack.py) carries what those cores load.
RA_ROM_SYSTEMS="Arcade
SNK - Neo Geo
SNK - Neo Geo CD
The 3DO Company - 3DO
Atari - 2600
Atari - 5200
Atari - 7800
Atari - 8-bit Family
Atari - Lynx
Bandai - WonderSwan
Bandai - WonderSwan Color
Coleco - ColecoVision
Magnavox - Odyssey2
Philips - Videopac+
Mattel - Intellivision
GCE - Vectrex
Fairchild - Channel F
NEC - PC Engine - TurboGrafx 16
NEC - PC Engine CD - TurboGrafx-CD
NEC - PC Engine SuperGrafx
Nintendo - Game Boy
Nintendo - Game Boy Color
Nintendo - Game Boy Advance
Nintendo - Nintendo Entertainment System
Nintendo - Family Computer Disk System
Nintendo - Super Nintendo Entertainment System
Nintendo - Virtual Boy
Nintendo - Pokemon Mini
Sega - SG-1000
Sega - Master System - Mark III
Sega - Mega Drive - Genesis
Sega - Mega-CD - Sega CD
Sega - 32X
Sega - Game Gear
SNK - Neo Geo Pocket
SNK - Neo Geo Pocket Color
Commodore - Amiga
Commodore - CD32
Commodore - 64
Commodore - VIC-20
Commodore - Plus-4
Amstrad - CPC
Sinclair - ZX Spectrum
Sinclair - ZX 81
Microsoft - MSX
Microsoft - MSX2
NEC - PC-88
NEC - PC-98
Sharp - X1
Sharp - X68000
DOS
ScummVM
DOOM
Wolfenstein 3D"

create_retroarch_tree() {
    log "Creating the RetroArch tree under $RA_ROOT"
    local d
    for d in $RA_SUBDIRS; do
        run mkdir -p "$RA_ROOT/$d"
    done
    log "Creating the roms/ folders (one per system, named as RetroArch's playlists are)"
    # the shared list the Windows installer also reads (src/resources/platform/roms_systems.cfg, staged into
    # the app's resources); the inline RA_ROM_SYSTEMS is the fallback if the package predates it
    local systems_file="$STAGE_DIR/Autobleem/bin/autobleem/platform/roms_systems.cfg"
    local systems="$RA_ROM_SYSTEMS"
    if [ -f "$systems_file" ]; then
        systems="$(grep -v '^[[:space:]]*#' "$systems_file" | grep -v '^[[:space:]]*$')"
    fi
    printf '%s
' "$systems" | while IFS= read -r d; do
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
global_core_options = "true"
log_dir = "$RA_ROOT/logs"
video_fullscreen = "true"
input_autodetect_enable = "true"
menu_show_core_updater = "true"
EOF
}

#*******************************
# set_cfg_key
#*******************************
# sets `key = "value"` in a RetroArch-style cfg: replaces the line when the key is there (RetroArch writes
# every key back on exit, defaults included), appends it otherwise. Everything else in the file - the user's
# settings - is kept.
set_cfg_key() {
    local file="$1" key="$2" value="$3"
    [ -f "$file" ] || return 0
    grep -q "^$key = \"$value\"" "$file" && return 0
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would set %s = "%s" in %s
' "$key" "$value" "$file"
        return 0
    fi
    if grep -q "^$key = " "$file"; then
        sed -i "s|^$key = .*|$key = \"$value\"|" "$file"
    else
        printf '%s = "%s"
' "$key" "$value" >> "$file"
    fi
    log "Set $key = \"$value\" in $(basename "$file")"
}

#*******************************
# write_retroarch_core_options
#*******************************
# The core options AutoBleem edits around each launch (LaunchService::transferRaConfig - the game editor's
# High-res and Speed hack for pcsx_rearmed) live in this one global file, so retroarch.cfg says
# global_core_options = true: with RetroArch's default of per-core .opt files the global one would be
# ignored the moment RetroArch saved its own. Only written when missing; RetroArch keeps it up to date.
# pad types: pcsx_rearmed defaults to a digital PS1 pad, which makes every analog stick a d-pad.
write_retroarch_core_options() {
    set_cfg_key "$RA_ROOT/retroarch.cfg" global_core_options true

    local opt="$RA_ROOT/config/retroarch-core-options.cfg"
    if [ -s "$opt" ]; then   # -s: an empty one (AutoBleem's config transfer touches it) counts as missing
        log "Keeping the existing $opt"
        return 0
    fi
    log "Writing $opt"
    write_file "$opt" <<EOF
pcsx_rearmed_pad1type = "dualshock"
pcsx_rearmed_pad2type = "dualshock"
pcsx_rearmed_neon_enhancement_enable = "disabled"
pcsx_rearmed_neon_enhancement_no_main = "disabled"
EOF
}

#*******************************
# download_retroarch_content
#*******************************
# What RetroArch's own Online Updater would fetch, done here so it is complete before the first start: every
# core the buildbot has for this architecture (its .index-extended is the list RetroArch itself uses), the core info
# files AutoBleem's RetroArch set is built from, and the menu assets, pad autoconfigs, scanner databases,
# cheats, overlays and GLSL shaders. A few hundred MB; --no-downloads skips all of it.
download_retroarch_content() {
    [ "$DO_DOWNLOADS" -eq 1 ] || { log "skipping the RetroArch cores and assets (--no-downloads)"; return 0; }
    # nothing would run them (--retroarch none, or a build that failed and was given up on)
    [ "$RETROARCH_MODE" != none ] || { log "no RetroArch here - skipping its cores and assets"; return 0; }

    # one tarball from AutoBleem's download repository first (ci/build_cores.sh packs the same cores and
    # bundles); the per-core download from buildbot below is the fallback, and what a re-run uses to fill in
    if download_cores_tarball; then
        return 0
    fi

    local base=https://buildbot.libretro.com
    local cores_url="$base/nightly/linux/$RA_ARCH/latest"
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
            printf '\r    [%3d/%3d] %3d%% %-50s' "$count" "$total" "$((count * 100 / total))" "$zip"
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
# download_cores_tarball
#*******************************
# <repo>/rpi/cores/latest.json names, per architecture, a tarball of every core plus the info/assets/
# autoconfig/database/cheats/overlays/shaders bundles, laid out as the RetroArch tree - one download and one
# unpack into RetroArch/ instead of ~130 requests. Only for a RetroArch/ that has no cores yet: a Pi that
# already has some keeps them (RetroArch's Online Updater is the way to refresh), and the buildbot loop fills
# in what is missing. Returns 1 whenever the buildbot path should run instead.
download_cores_tarball() {
    local latest="$DATA_MOUNT/.autobleem-tmp/cores-latest.json"
    local url sha date
    if [ -n "$(ls -A "$RA_ROOT/cores" 2>/dev/null)" ]; then
        return 1
    fi
    command -v python3 >/dev/null 2>&1 || return 1
    mkdir -p "$(dirname "$latest")"
    log "RetroArch: asking $REPO_URL for the cores of $ARCH"
    if ! wget -q -O "$latest" "$REPO_URL/$PLATFORM_DIR/cores/latest.json"; then
        warn "cannot reach $REPO_URL/$PLATFORM_DIR/cores/latest.json - downloading the cores one by one instead"
        return 1
    fi
    read -r url sha date < <(python3 - "$latest" "$ARCH" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
a = d.get(sys.argv[2]) or {}
print(a.get("url", ""), a.get("sha256", ""), a.get("date", ""))
PY
    )
    [ -n "$url" ] || { warn "the repository has no cores tarball for $ARCH"; return 1; }
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would download %s, check its sha256 and unpack it into %s\n' "$url" "$RA_ROOT"
        return 0
    fi
    # on the data partition, not /tmp: the tarball is several hundred MB and the root is small
    local tarball="$DATA_MOUNT/.autobleem-tmp/cores.tar.gz"
    log "RetroArch: downloading the cores and assets of $date ($url)"
    if ! wget -q --show-progress -O "$tarball.part" "$url" \
       || [ "$(sha256sum "$tarball.part" | cut -d' ' -f1)" != "$sha" ]; then
        warn "the download failed or its sha256 does not match - downloading the cores one by one instead"
        rm -f "$tarball.part"
        return 1
    fi
    mv -f "$tarball.part" "$tarball"
    log "RetroArch: unpacking into $RA_ROOT"
    if ! tar -xzf "$tarball" -C "$RA_ROOT" --no-same-owner --no-same-permissions; then
        warn "could not unpack the cores tarball - downloading the cores one by one instead"
        rm -f "$tarball"
        return 1
    fi
    rm -rf "$DATA_MOUNT/.autobleem-tmp"
    log "RetroArch: $(ls "$RA_ROOT/cores" | grep -c '_libretro\.so$') cores installed"
}

#*******************************
# download_thumbnails
#*******************************
# The launcher's covers: libretro-thumbnails' "Sony - PlayStation" pack, named after the games in the
# rdb downloaded above, so a game the scanner identifies gets its box art from here (the covers*.db in
# Autobleem/bin/db, when present, is the fallback). thumbnailpacks.libretro.com's zips are gone, and the
# GitHub mirror is one 1.5 GB archive of all three folders, so this mirrors the per-file listing at
# thumbnails.libretro.com instead: ~9000 files, only the ones not already there (re-runs resume), four
# connections at a time, with a running [n/total] percentage on the screen.
download_thumbnails() {
    case "$THUMBNAILS" in
        none) log "skipping the thumbnails (--thumbnails none)"; return 0 ;;
        boxarts) local dirs="Named_Boxarts" ;;
        all) local dirs="Named_Boxarts Named_Titles Named_Snaps" ;;
        *) die "--thumbnails takes boxarts, all or none (got '$THUMBNAILS')" ;;
    esac
    local system="Sony - PlayStation"
    local base="https://thumbnails.libretro.com/${system// /%20}"
    local tmp=/tmp/autobleem-thumbs
    run mkdir -p "$tmp"
    local dir
    for dir in $dirs; do
        local dest="$RA_ROOT/thumbnails/$system/$dir"
        run mkdir -p "$dest"
        if [ "$DRY_RUN" -eq 1 ]; then
            printf '    would list %s/%s/ and download the files not yet in %s, 4 at a time, with a counter\n' "$base" "$dir" "$dest"
            continue
        fi
        # The directory listing first, so the total is known and the screen can show a percentage - this
        # is the longest download of the install (~9000 files for the box arts) and used to be a silent
        # recursive wget. The hrefs are percent-encoded; the local file names are the decoded form, which
        # is what wget -r wrote before and what the launcher's ThumbnailLookup matches against.
        log "RetroArch: thumbnails.libretro.com/$system/$dir - listing"
        if ! wget -q -O "$tmp/index.html" "$base/$dir/"; then
            warn "thumbnails: cannot fetch the $dir listing - run the installer again later to get them"
            continue
        fi
        grep -oE 'href="[^"]+\.(png|jpg)"' "$tmp/index.html" | sed 's/^href="//; s/"$//' | sort -u >"$tmp/all.txt"
        local total present=0
        total="$(grep -c . "$tmp/all.txt")"
        if [ "$total" -eq 0 ]; then
            warn "thumbnails: the $dir listing has no images - the site's layout may have changed"
            continue
        fi
        : >"$tmp/todo.txt"
        local href name
        while IFS= read -r href; do
            name="$(printf '%b' "${href//%/\\x}")"
            if [ -s "$dest/$name" ]; then
                present=$((present + 1))
            else
                printf '%s/%s/%s\n' "$base" "$dir" "$href" >>"$tmp/todo.txt"
            fi
        done <"$tmp/all.txt"
        local todo=$((total - present))
        if [ "$todo" -eq 0 ]; then
            log "RetroArch: $dir - all $total files already there"
            continue
        fi
        log "RetroArch: $dir - $todo of $total files to download into $dest ($present already there)"
        # four wget streams, each on one kept-alive connection over its share of the list; every finished
        # file is one -nv line, which the awk turns into the running counter (a 404 counts as done too -
        # it is reported at the end, not retried)
        rm -f "$tmp"/todo.part.*
        split -n l/4 -d "$tmp/todo.txt" "$tmp/todo.part."
        local part
        {
            for part in "$tmp"/todo.part.*; do
                [ -s "$part" ] || continue
                wget -nv -nc -P "$dest" -i "$part" 2>&1 &
            done
            wait
        } | awk -v total="$total" -v done0="$present" '
            # one result line per file: "<date> URL:... [size] -> file" on success, "<date> ERROR 404: ..."
            # on failure (preceded by a bare URL line, not counted), "already there" for a -nc skip; each
            # wget also ends with a FINISHED/Total/Downloaded summary, not counted either
            function show() { printf "\r    [%5d/%5d] %3d%%", done0 + n, total, (done0 + n) * 100 / total; fflush() }
            / URL:/ || /already there/ { n++; show() }
            / ERROR / { n++; err++; show() }
            END { printf "\n"; if (err) printf "    %d files did not download - run the installer again to retry them\n", err }'
    done
    run rm -rf "$tmp"
}

#*******************************
# download_bios_pack
#*******************************
# The BIOS files the cores need, into RetroArch/system/. system/biospack.txt (armhf) or
# system/biospack-arm64.txt (arm64) - built per architecture by tools/biospack.py in the source tree, from
# the cores that architecture's buildbot actually has - names every file with its SHA-256, size and URL: a
# pinned commit of RetroBIOS (github.com/Abdess/retrobios), cut down to the systems with a roms/ folder here
# plus arcade and ScummVM - the full RetroArch pack is 5.8 GB, this is ~190-230 MB depending on architecture.
# A file already there with the right hash is kept, so a re-run only fetches what is missing or damaged; the
# download lands in a .part next to the target and is renamed once its hash checks out, so a power cut
# cannot leave a half file that looks whole.
download_bios_pack() {
    [ "$DO_BIOS" -eq 1 ] || { log "skipping the BIOS pack (--no-bios)"; return 0; }
    local manifest_name=biospack.txt
    case "$ARCH" in
        arm64) manifest_name=biospack-arm64.txt ;;
        i386)  manifest_name=biospack-i386.txt ;;
    esac
    local manifest="$SCRIPT_DIR/system/$manifest_name"
    [ -f "$manifest" ] || { warn "no system/$manifest_name in the package - no BIOS files installed"; return 0; }

    # --retroarch none is a PS1-only AutoBleem: nothing reads RetroArch/system but pcsx-ab's two files
    # (install_ps1_bios), so the rest of the ~200 MB pack is not fetched. Same manifest, filtered by name.
    local entries
    if [ "$RETROARCH_MODE" = none ]; then
        # the last field is the destination under RetroArch/system - the PS1 files sit at its top level
        entries="$(grep '^[0-9a-f]' "$manifest" | awk '$4 == "scph5501.bin" || $4 == "scph5500.bin"')"
        log "BIOS pack: PS1-only (--retroarch none) - just the two files pcsx-ab needs"
    else
        entries="$(grep '^[0-9a-f]' "$manifest")"
    fi
    local total
    total="$(echo "$entries" | grep -c .)"
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would download the %s BIOS files of %s into %s/system\n' "$total" "$(basename "$manifest")" "$RA_ROOT"
        printf '    would copy scph5501.bin/scph5500.bin to %s/System/Bios as romw.bin/romJP.bin if not there\n' "$DATA_MOUNT"
        return 0
    fi
    log "BIOS pack: $total files into $RA_ROOT/system (only what is missing)"
    local sha size url dest target count=0 fetched=0 failed=0
    while read -r sha size url dest; do
        [ -n "$dest" ] || continue
        count=$((count + 1))
        printf '\r    [%3d/%3d] %3d%% %-50.50s' "$count" "$total" "$((count * 100 / total))" "$dest"
        target="$RA_ROOT/system/$dest"
        if [ -f "$target" ] && [ "$(sha256sum "$target" | cut -d' ' -f1)" = "$sha" ]; then
            continue
        fi
        mkdir -p "$(dirname "$target")"
        if wget -q -O "$target.part" "$url" \
           && [ "$(sha256sum "$target.part" | cut -d' ' -f1)" = "$sha" ]; then
            mv -f "$target.part" "$target"
            fetched=$((fetched + 1))
        else
            rm -f "$target.part"
            failed=$((failed + 1))
        fi
    done < <(echo "$entries")
    printf '\n'
    log "BIOS pack: $fetched downloaded, $((count - fetched - failed)) already there"
    [ "$failed" -eq 0 ] || warn "$failed BIOS files did not download or did not match their hash - run the installer again"
    sync
    install_ps1_bios
}

#*******************************
# install_ps1_bios
#*******************************
# pcsx-ab does not look in RetroArch's system/: rc/launch.sh gives it System/Bios, where it wants romw.bin
# for every game and romJP.bin for a Japanese one (the console's names - see the launch script). The pack's
# SCPH-5501 (NTSC-U, v3.0) and SCPH-5500 (NTSC-J, v3.0) fill the two names when the user has not put their
# own there. Only copies: the originals stay in system/ for pcsx_rearmed.
install_ps1_bios() {
    local bios_dir="$DATA_MOUNT/System/Bios" pair name src
    for pair in romw.bin:scph5501.bin romJP.bin:scph5500.bin; do
        name="${pair%%:*}"; src="$RA_ROOT/system/${pair#*:}"
        if [ -f "$bios_dir/$name" ]; then
            log "Keeping the existing $bios_dir/$name"
        elif [ -f "$src" ]; then
            log "PS1 BIOS for pcsx-ab: $(basename "$src") -> $bios_dir/$name"
            run cp "$src" "$bios_dir/$name"
        else
            warn "no $src - pcsx-ab has no $name and will use its HLE BIOS"
        fi
    done
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

    # parted prints the disk's size rounded to whole MiB, so a free span that runs to the end of the disk
    # ends at a value that can be a fraction past the last sector - and "mkpart ... 120580MiB" is then
    # "outside of the device" (seen on the first image's first boot, a 117.8 GB card). 100% is parted's own
    # way of saying "to the end", so a span that reaches the disk's end asks for that instead.
    local disk_end end_arg="${end}MiB"
    disk_end="$(parted -ms "$DISK" unit MiB print 2>/dev/null | awk -F: -v d="$DISK" '$1 == d { print $2 + 0 }')"
    [ -n "$disk_end" ] && [ "$end" -ge "$disk_end" ] && end_arg="100%"
    run parted -s "$DISK" mkpart primary "${start}MiB" "$end_arg"
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
    [ "$PLATFORM" = rpi ] || die "--shrink-root is a Raspberry Pi mechanism (it rides on cmdline.txt); the PC
    stick's image ships with a small root that --grow-root grows, and a hand-made PC install should be
    partitioned with room left for the data partition"
    rootdev="$(findmnt -no SOURCE /)"

    log "Staging an offline shrink of $rootdev to ${gib}GiB"
    warn "This repartitions $DISK on the next boot. If it goes wrong $MEDIUM may not boot."
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
    log "Run this installer again when $MACHINE comes back up."
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
# grow_root
#*******************************
# The image tools/make_rpi_image.sh builds keeps the root filesystem at the base image's size (its
# cmdline.txt has no "resize", so the initramfs never grows the partition over the whole card) - that is
# what leaves room for the data partition. But a 3 GB root is too small for what install.sh puts on it
# (RetroArch built from source alone wants a couple of GB), so the root is grown here first, online, to a
# bounded size, and ensure_data_partition then takes the rest. sfdisk rewrites the partition's size in
# place (--no-reread: the disk is in use, the kernel is told separately with partx), and resize2fs grows
# ext4 while mounted - both are what growpart does, minus the "to the very end" it insists on.
grow_root() {
    local gib="$1"
    local rootdev rootname diskname partnum start_s size_s disk_s target_s max_s target_mib
    rootdev="$(findmnt -no SOURCE /)"
    rootname="$(basename "$rootdev")"
    diskname="$(basename "$DISK")"
    [ -r "/sys/class/block/$rootname/partition" ] || die "cannot tell which partition $rootdev is"
    partnum="$(cat "/sys/class/block/$rootname/partition")"
    # sysfs sizes are in 512-byte sectors whatever the device says
    start_s="$(cat "/sys/class/block/$rootname/start")"
    size_s="$(cat "/sys/class/block/$rootname/size")"
    disk_s="$(cat "/sys/block/$diskname/size")"

    target_s=$((gib * 1024 * 1024 * 2))
    max_s=$((disk_s - start_s - MIN_DATA_MIB * 2048))
    if [ "$target_s" -gt "$max_s" ]; then
        warn "$MEDIUM is too small for a ${gib} GiB root plus a ${MIN_DATA_MIB} MiB data partition - growing the root to $((max_s / 2048)) MiB instead"
        target_s=$max_s
    fi
    if [ "$target_s" -le "$size_s" ]; then
        log "Root partition is already $((size_s / 2048)) MiB - not growing it"
        return 0
    fi
    target_mib=$((target_s / 2048))
    log "Growing $rootdev from $((size_s / 2048)) MiB to ${target_mib} MiB (the rest of $DISK is for the data partition)"
    confirm "Grow partition $partnum of $DISK to ${target_mib} MiB?"

    # ",SIZE" keeps the start and sets the new size; the disk is in use, so no re-read, and --force past
    # sfdisk's own "device is mounted" refusal
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would run: echo ",%sMiB" | sfdisk --no-reread --force -N %s %s\n' "$target_mib" "$partnum" "$DISK"
    else
        echo ",${target_mib}MiB" | sfdisk --no-reread --force -N "$partnum" "$DISK" \
            || die "sfdisk could not resize partition $partnum of $DISK"
    fi
    # tell the kernel about the new size of a partition that is mounted (BLKPG_RESIZE_PARTITION)
    run partx -u --nr "$partnum" "$DISK" || run partprobe "$DISK" || true
    run udevadm settle
    run resize2fs "$rootdev" || die "resize2fs $rootdev failed - the partition is grown but the filesystem is not"
    log "Root filesystem grown"
}

# --grow-root, before apt starts filling a root that is still the base image's size (a fresh Lite root has
# a few hundred MB free - not enough for the packages, let alone the RetroArch build). Pointless once the
# data partition exists: it sits right after the root, there is nothing to grow into.
maybe_grow_root() {
    [ -n "$GROW_ROOT_GIB" ] || return 0
    if [ -n "$(existing_data_partition)" ]; then
        log "A $DATA_LABEL partition already exists - ignoring --grow-root"
        return 0
    fi
    grow_root "$GROW_ROOT_GIB"
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

    if [ "$PLATFORM" = pcusb ]; then
        die "no $DATA_LABEL partition and no room to make one.

    The root filesystem fills the disk. Shrink it from another machine (GParted, or a live USB) so at least
    ${MIN_DATA_MIB}MiB are free after it, then re-run this installer - or install Debian again with a smaller
    root partition. The AutoBleem image (tools/make_pc_image.sh) is laid out for this: its root is small and
    --grow-root grows it, leaving the rest of the stick for the data partition."
    fi
    die "no $DATA_LABEL partition and no room to make one.

    The root filesystem was grown over the whole card on first boot, which is normal. Pick one:

      * sudo bash install.sh --shrink-root 8     shrink the root filesystem to 8GiB and use the rest here.
                                              Repartitions the card on the next boot - back up first.
      * shrink partition 2 from another machine (GParted), then re-run this installer.
      * re-flash, and before the first boot delete the word 'resize' from cmdline.txt on the FAT partition
        so the root filesystem is never expanded (Raspberry Pi OS Trixie; older images used init=...firstboot).
        Then run this installer with --grow-root 8: the root grows to 8 GiB and the rest is the data partition."
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
# derives every path from this root (Autobleem/bin/autobleem, Games, System/Databases, Themes, ...).
create_tree() {
    log "Creating the AutoBleem tree under $DATA_MOUNT"
    local d
    for d in Autobleem/bin/autobleem Autobleem/bin/db Autobleem/bin/emu Autobleem/bin/emunxt Autobleem/rc \
             Games System/Bios System/Databases System/Logs System/Processors Themes Apps Extensions; do
        run mkdir -p "$DATA_MOUNT/$d"
    done
    RA_ROOT="$DATA_MOUNT/RetroArch"
    create_retroarch_tree
    write_retroarch_config
    write_retroarch_core_options
}

#*******************************
# install_payload
#*******************************
# payload_linux/ is the tree that goes on the data partition, laid out exactly like the console's payload/:
# Autobleem/bin/autobleem (the app and its resources), Autobleem/bin/db (covers), Autobleem/rc (the launch
# scripts), Themes/, Games/, Apps/. Installing is copying it across - tools/make_rpi_package.sh is what fills
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
    for d in Autobleem Themes Games Apps RetroArch; do
        [ -d "$STAGE_DIR/$d" ] || continue
        run cp -r "$STAGE_DIR/$d/." "$DATA_MOUNT/$d/"
    done
    # the scanner processors' folder (the launcher's docs/scanner-processors-plan.md): its README once - the
    # user may have edited it, and the processors in there are theirs, never the package's
    # (kept under system/, the installer's own files: payload_linux cannot hold both System/ and system/ on a
    # case-insensitive filesystem)
    if [ -f "$SCRIPT_DIR/system/processors-README.txt" ] && [ ! -f "$DATA_MOUNT/System/Processors/README.txt" ]; then
        run cp "$SCRIPT_DIR/system/processors-README.txt" "$DATA_MOUNT/System/Processors/README.txt"
    fi
    # the bundled scanner processors (processors/<name>/ in the package - proc_unzip): their files replace the
    # data partition's copies, so an update brings the new program; System/Processors/sequence.ini (the user's
    # order, on/off) is never touched
    if [ -d "$STAGE_DIR/processors" ]; then
        run cp -r "$STAGE_DIR/processors/." "$DATA_MOUNT/System/Processors/"
        run chmod +x "$DATA_MOUNT"/System/Processors/*/bin/*/* || true
    fi
    # the extensions' folder (docs/extensions-plan.md): installed by hand, so only its README, once
    if [ -f "$SCRIPT_DIR/system/extensions-README.txt" ] && [ ! -f "$DATA_MOUNT/Extensions/README.txt" ]; then
        run cp "$SCRIPT_DIR/system/extensions-README.txt" "$DATA_MOUNT/Extensions/README.txt"
    fi

    # exFAT has no permission bits of its own - the mount's umask=000 already makes everything 0777 - so a
    # chmod there is either a no-op or refused by the driver. Neither is a reason to stop.
    run chmod +x "$app_dest/autobleem-gui" "$DATA_MOUNT/Autobleem/bin/emu/pcsx-ab" "$DATA_MOUNT/Autobleem/bin/emunxt/pcsx-ab" || true
    run chmod +x "$DATA_MOUNT/Autobleem/rc/"*.sh || true
    # the package's version, which the launcher's update check compares with the site's (Env::productVersion
    # reads <data>/VERSION) - on every install and update, so it always names what is installed
    if [ -f "$STAGE_DIR/VERSION" ]; then
        run cp "$STAGE_DIR/VERSION" "$DATA_MOUNT/VERSION"
    fi

    if [ -f "$app_dest/config.ini.keep" ]; then
        run mv -f "$app_dest/config.ini.keep" "$app_dest/config.ini"
    fi
    # the PS1 emulator every install lands on (the owner's rule, 2026-09-21): pcsx-abnxt, whatever the kept
    # config.ini said - the key replaced in place, or added when the file has none
    if [ -f "$app_dest/config.ini" ]; then
        log "PS1 emulator set to pcsx-abnxt"
        if grep -qi '^emulator=' "$app_dest/config.ini"; then
            run sed -i 's/^[Ee]mulator=.*/Emulator=pcsx-abnxt/' "$app_dest/config.ini"
        else
            run sh -c "printf '\nEmulator=pcsx-abnxt\\n' >> '$app_dest/config.ini'"
        fi
    fi

    # a new launcher's first start rescans everything (the owner's rule, 2026-09-20): a scanner change
    # (new Game.ini keys, a merge rule, box art) must reach the games already there, and the fingerprints
    # are what makes the launcher skip the scan when nothing on disk changed - so they go
    log "Removing the scan fingerprints - the launcher rescans on its next start"
    run rm -f "$app_dest/games.fingerprint" "$app_dest/roms.fingerprint" "$app_dest/roms.scanstate"

    [ -f "$DATA_MOUNT/Autobleem/bin/emu/pcsx-ab" ] || warn "no pcsx-ab in the package - PS1 games will fall back
    to RetroArch's pcsx_rearmed core (no AutoBleem save states). Build it with pcsx-rearmed-develop/make_rpi.sh."

    install_cover_databases
}

#*******************************
# install_cover_databases
#*******************************
# The three cover databases are 290 MB the package leaves out since 2026-09-19 (tools/make_rpi_package.sh
# --with-covers puts them back); they come from the download repository's db/ instead, each checked against
# its .sha256 there. Ones already on the data partition (a re-install, or a package that had them) are
# kept. Without them the launcher still works: a game's title comes from the rdb and its cover from
# thumbnails.libretro.com when online - the databases are the offline fallback.
install_cover_databases() {
    local db_dir="$DATA_MOUNT/Autobleem/bin/db" name url sha missing=0
    mkdir -p "$db_dir"
    for name in coversU.db coversP.db coversJ.db; do
        [ -f "$db_dir/$name" ] && continue
        # not behind --no-downloads: that flag is about RetroArch's cores, these are the launcher's own
        url="$REPO_URL/db/$name"
        if [ "$DRY_RUN" -eq 1 ]; then
            printf '    would download %s into %s\n' "$url" "$db_dir"
            continue
        fi
        log "Cover database: downloading $name"
        sha="$(wget -q -O - "$url.sha256" 2>/dev/null | cut -d' ' -f1)"
        if wget -q --show-progress -O "$db_dir/$name.part" "$url" \
           && { [ -z "$sha" ] || [ "$(sha256sum "$db_dir/$name.part" | cut -d' ' -f1)" = "$sha" ]; }; then
            mv -f "$db_dir/$name.part" "$db_dir/$name"
        else
            rm -f "$db_dir/$name.part"
            missing=$((missing + 1))
        fi
    done
    [ "$missing" -eq 0 ] || warn "$missing cover database(s) missing - titles and covers still come from the rdb and
    thumbnails.libretro.com while online; re-run the installer for the offline fallback, or copy covers*.db into $db_dir/"
}

#*******************************
# install_sample_games
#*******************************
# So the shelf is not empty on the first start: the sample pack from the download repository (samples/,
# built by tools/build_samples.py from tools/samples/samples.json - homebrew whose licence allows
# redistribution) laid out as it is on the data partition: Games/<game>/ with a locked Game.ini and a
# cover for the PS1 game, RetroArch/roms/<system>/ + thumbnails for the others (those only when RetroArch
# is installed - nothing plays them otherwise). Once: System/samples.txt remembers the pack installed, so a
# re-run does not put back a sample the user deleted. Not behind --no-downloads (that is RetroArch's
# content); --no-samples skips it. A site that cannot be reached is a warning, not a failure.
install_sample_games() {
    [ "$DO_SAMPLES" -eq 1 ] || { log "skipping the sample games (--no-samples)"; return 0; }
    local marker="$DATA_MOUNT/System/samples.txt" latest="$DATA_MOUNT/.autobleem-tmp/samples-latest.json"
    local url sha date tarball
    if [ -f "$marker" ]; then
        log "Sample games: already installed ($(head -1 "$marker")) - not again"
        return 0
    fi
    command -v python3 >/dev/null 2>&1 || { warn "no python3 - skipping the sample games"; return 0; }
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would download %s/samples/latest.json and unpack the pack into %s\n' "$REPO_URL" "$DATA_MOUNT"
        return 0
    fi
    mkdir -p "$(dirname "$latest")"
    log "Sample games: asking $REPO_URL for the pack"
    if ! wget -q -O "$latest" "$REPO_URL/samples/latest.json"; then
        warn "cannot reach $REPO_URL/samples/latest.json - no sample games this time (re-run the installer to get them)"
        return 0
    fi
    read -r url sha date < <(python3 - "$latest" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
print(d.get("url", ""), d.get("sha256", ""), d.get("date", ""))
PY
    )
    [ -n "$url" ] || { warn "the repository has no sample pack"; return 0; }
    tarball="$DATA_MOUNT/.autobleem-tmp/samples.tar.gz"
    log "Sample games: downloading the pack of $date ($url)"
    if ! wget -q --show-progress -O "$tarball.part" "$url" \
       || [ "$(sha256sum "$tarball.part" | cut -d' ' -f1)" != "$sha" ]; then
        warn "the download failed or its sha256 does not match - no sample games this time"
        rm -f "$tarball.part"
        return 0
    fi
    mv -f "$tarball.part" "$tarball"
    # only the top-level members the pack actually has: naming one it lacks makes tar fail outright
    # (the pack has had no PS1 game - no Games/ - since 2026-09-20, and every install lost all of its samples
    # to "Games: Not found in archive"); the RetroArch part only where RetroArch is installed
    local -a members=()
    local top
    while read -r top; do
        case "$top" in
            Games|SAMPLES.md) members+=("$top") ;;
            RetroArch) [ "$RETROARCH_MODE" = none ] || members+=("$top") ;;
        esac
    done < <(tar -tzf "$tarball" | sed 's|/.*||' | sort -u)
    if [ ${#members[@]} -eq 0 ]; then
        warn "the sample pack has nothing for this install - no sample games this time"
        rm -f "$tarball"
        return 0
    fi
    if ! tar -xzf "$tarball" -C "$DATA_MOUNT" --no-same-owner --no-same-permissions "${members[@]}"; then
        warn "could not unpack the sample pack - no sample games this time"
        rm -f "$tarball"
        return 0
    fi
    rm -rf "$DATA_MOUNT/.autobleem-tmp"
    mkdir -p "$DATA_MOUNT/System"
    printf 'pack %s from %s\n' "$date" "$url" > "$marker"
    log "Sample games: installed (see SAMPLES.md on the games partition)"
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

    install_update_helper
}

#*******************************
# install_update_helper
#*******************************
# The launcher's online update (its Options -> Updates, CLAUDE.md "The online update") ends with the
# session loop running autobleem-update over what the launcher downloaded: that script, the first boot's
# progress screen and its logo, and a copy of this installer (with system/, for a RetroArch-only update that
# has no new package to take install.sh from) go under /usr/local.
install_update_helper() {
    local share=/usr/local/share/autobleem
    run install -d -m 0755 "$share" "$share/installer/system"
    run install -m 0755 "$SCRIPT_DIR/system/autobleem-update.sh" /usr/local/bin/autobleem-update
    run install -m 0644 "$SCRIPT_DIR/system/autobleem-install-ui.py" "$share/autobleem-install-ui.py"
    run install -m 0644 "$SCRIPT_DIR/system/plymouth/splash.png" "$share/splash.png"
    run install -m 0755 "$SCRIPT_DIR/install.sh" "$share/installer/install.sh"
    run cp -r "$SCRIPT_DIR/system/." "$share/installer/system/"
}

#*******************************
# install_boot_splash
#*******************************
# The AutoBleem logo instead of a black screen (or the kernel log) between the firmware and the launcher:
# a plymouth theme (system/plymouth/, the "script" plugin - the one Raspberry Pi OS's own "pix" theme uses)
# started from the initramfs. autobleem-session quits it, keeping the last frame on screen, right before it
# starts the launcher - see the service file for why plymouth-quit.service is kept out of the way. The same
# theme shows on the way down, when Power Off in the L2+R2 menu halts the Pi.
PLYMOUTH_THEME_DIR=/usr/share/plymouth/themes/autobleem

install_boot_splash() {
    [ "$DO_BOOT_CONFIG" -eq 1 ] || return 0
    [ "$BOOT_SPLASH" -eq 1 ] || { log "skipping the boot splash (--no-boot-splash / --no-quiet-boot)"; return 0; }

    local src="$SCRIPT_DIR/system/plymouth"
    if [ ! -f "$src/splash.png" ]; then
        warn "no system/plymouth/splash.png in the package - no boot splash"
        BOOT_SPLASH=0
        return 0
    fi
    # the same theme files already in place and selected - an update, or an image that was built with them
    # (the PC stick's) - keep the initramfs they are in (rebuilding every kernel's takes minutes)
    if [ -f /etc/plymouth/plymouthd.conf ] && grep -q '^Theme=autobleem' /etc/plymouth/plymouthd.conf \
       && cmp -s "$src/splash.png" "$PLYMOUTH_THEME_DIR/splash.png" \
       && cmp -s "$src/autobleem.script" "$PLYMOUTH_THEME_DIR/autobleem.script" \
       && cmp -s "$src/autobleem.plymouth" "$PLYMOUTH_THEME_DIR/autobleem.plymouth"; then
        log "Boot splash unchanged - keeping the initramfs"
        return 0
    fi
    if ! command -v plymouth-set-default-theme >/dev/null 2>&1; then
        warn "plymouth is not installed - no boot splash (re-run without --no-packages, or apt-get install plymouth)"
        BOOT_SPLASH=0
        return 0
    fi

    log "Installing the boot splash"
    run install -d -m 0755 "$PLYMOUTH_THEME_DIR"
    run install -m 0644 "$src/autobleem.plymouth" "$src/autobleem.script" "$src/splash.png" "$PLYMOUTH_THEME_DIR/"
    run plymouth-set-default-theme autobleem || {
        warn "plymouth-set-default-theme failed - no boot splash"
        BOOT_SPLASH=0
        return 0
    }

    # plymouth starts from the initramfs, so the theme has to be packed into it - into every kernel's, not
    # just the running one's: the 32-bit image carries one per board (v6 for a Zero/1, v7 for a 2/3, v7l
    # for a 4, v8 for the 64-bit kernel), each with its own /boot/firmware/initramfs<N>, and a card set up
    # on one board gets moved to another (a card done on a Pi 400 showed the stock theme on a Pi 3 until
    # this). A bare "-u" would pick the newest version only. The shrink hook stays on the running kernel on
    # purpose - it runs at the next boot of this board.
    if [ -d /etc/initramfs-tools ]; then
        log "Rebuilding the initramfs of every installed kernel with the boot splash in it"
        run update-initramfs -u -k all \
            || warn "update-initramfs failed - the splash will only show once the root is mounted"
    fi
}

#*******************************
# configure_boot
#*******************************
# The kernel command line for a quiet boot with the splash, per platform: a Pi's firmware reads cmdline.txt
# (and config.txt for its own rainbow square); a PC's GRUB takes it from /etc/default/grub.
configure_boot() {
    [ "$DO_BOOT_CONFIG" -eq 1 ] || { log "skipping boot config (--no-boot-config)"; return 0; }
    case "$PLATFORM" in
        rpi)   configure_boot_rpi ;;
        pcusb) configure_boot_pcusb ;;
    esac
}

# the same words both platforms boot with: no console blanking (a game would look like a crash after 10
# minutes), no kernel log, no cursor, plymouth for "splash" (ignore-serial-consoles keeps it on the screen
# when a serial console is also named)
boot_cmdline_words() {
    local extra="consoleblank=0"
    if [ "$QUIET_BOOT" -eq 1 ]; then
        extra="$extra quiet loglevel=3 logo.nologo vt.global_cursor_default=0"
    fi
    if [ "$BOOT_SPLASH" -eq 1 ]; then
        extra="$extra splash plymouth.ignore-serial-consoles"
    fi
    echo "$extra"
}

configure_boot_pcusb() {
    local grub_default=/etc/default/grub
    if ! command -v update-grub >/dev/null 2>&1 || [ ! -f "$grub_default" ]; then
        warn "no GRUB configuration to edit ($grub_default) - the boot stays as it is"
        return 0
    fi
    log "Configuring $grub_default"
    run cp -n "$grub_default" "$grub_default.autobleem-backup"

    # GRUB_CMDLINE_LINUX_DEFAULT gets our words (any earlier value's other words kept, ours replacing their
    # namesakes); the menu is hidden with a 2 s Shift window, the framebuffer mode GRUB set is kept for the
    # kernel so the handover to plymouth is not a modeset. Every key is written whole - a missing one is added.
    local current cmdline word kept
    current="$(sed -n 's/^GRUB_CMDLINE_LINUX_DEFAULT="\(.*\)"$/\1/p' "$grub_default" | tail -1)"
    kept=""
    for word in $current; do
        case " $(boot_cmdline_words) " in
            *" ${word%%=*}="*|*" $word "*) ;;   # ours replaces it
            *) kept="$kept${kept:+ }$word" ;;
        esac
    done
    cmdline="$kept${kept:+ }$(boot_cmdline_words)"

    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would set GRUB_CMDLINE_LINUX_DEFAULT="%s", GRUB_TIMEOUT=2, GRUB_TIMEOUT_STYLE=hidden and run update-grub\n' "$cmdline"
        return 0
    fi
    {
        grep -vE '^(GRUB_CMDLINE_LINUX_DEFAULT|GRUB_TIMEOUT|GRUB_TIMEOUT_STYLE|GRUB_GFXMODE|GRUB_GFXPAYLOAD_LINUX)=' "$grub_default"
        printf 'GRUB_CMDLINE_LINUX_DEFAULT="%s"\n' "$cmdline"
        printf 'GRUB_TIMEOUT=2\nGRUB_TIMEOUT_STYLE=hidden\nGRUB_GFXMODE=auto\nGRUB_GFXPAYLOAD_LINUX=keep\n'
    } | write_file "$grub_default"
    run update-grub || warn "update-grub failed - the boot options were written to $grub_default but not applied"
    [ "$HDMI_MODE" = "1920x1080@60" ] || [ "$HDMI_MODE" = none ] \
        || log "(--hdmi-mode is a Raspberry Pi option: a PC boots in the screen's native mode)"
}

configure_boot_rpi() {
    log "Configuring $BOOT_DIR/cmdline.txt"
    run cp -n "$BOOT_DIR/cmdline.txt" "$BOOT_DIR/cmdline.txt.autobleem-backup"

    local cmdline extra
    cmdline="$(tr -d '\n' < "$BOOT_DIR/cmdline.txt")"
    extra="$(boot_cmdline_words)"

    local word
    for word in $extra; do
        case " $cmdline " in
            *" ${word%%=*}="*|*" $word "*) ;;                 # already set, leave whatever value is there
            *) cmdline="$cmdline $word" ;;
        esac
    done

    # The HDMI mode for the whole boot. config.txt's hdmi_group/hdmi_mode mean nothing to the KMS driver;
    # video= on the kernel command line does. The launcher opens its window at the display's mode (its
    # 1280x720 UI drawn at 1.5x on a 1080p one), so booting in that mode means plymouth draws at the
    # launcher's resolution and the handover is not a modeset the TV has to re-sync to. Both ports get it - which one the screen is on is only known at boot - and any earlier
    # video=HDMI-A-n: words are replaced, so a re-run with another --hdmi-mode takes effect.
    if [ "$HDMI_MODE" != none ]; then
        local kept=""
        for word in $cmdline; do
            case "$word" in
                video=HDMI-A-[12]:*) ;;
                *) kept="$kept${kept:+ }$word" ;;
            esac
        done
        cmdline="$kept video=HDMI-A-1:$HDMI_MODE video=HDMI-A-2:$HDMI_MODE"
    fi

    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would write to %s: %s\n' "$BOOT_DIR/cmdline.txt" "$cmdline"
    else
        printf '%s\n' "$cmdline" | write_file "$BOOT_DIR/cmdline.txt"
    fi

    # The firmware's rainbow square is config.txt's business, not the kernel's. Appended in its own [all]
    # section, so it applies whatever [model] filter the file happens to end under.
    if [ "$QUIET_BOOT" -eq 1 ] && [ -f "$BOOT_DIR/config.txt" ] && ! grep -q '^disable_splash=' "$BOOT_DIR/config.txt"; then
        log "Configuring $BOOT_DIR/config.txt (disable_splash=1)"
        run cp -n "$BOOT_DIR/config.txt" "$BOOT_DIR/config.txt.autobleem-backup"
        { cat "$BOOT_DIR/config.txt"; printf '\n[all]\n# AutoBleem: no rainbow square from the firmware\ndisable_splash=1\n'; } \
            | write_file "$BOOT_DIR/config.txt"
    fi
}

#*******************************
# summary
#*******************************
ssh_hint() {
    if [ "$PLATFORM" = rpi ]; then
        echo "sudo raspi-config -> Interface Options -> SSH."
    elif systemctl is-enabled ssh >/dev/null 2>&1; then
        echo "it is (ssh <user>@<this machine>)."
    else
        echo "sudo apt-get install openssh-server."
    fi
}

summary() {
    log "Done."
    cat <<EOF

  Games go in      $DATA_MOUNT/Games/<game name>/      (one folder per game, .cue+.bin / .pbp / .chd)
  BIOS             $DATA_MOUNT/System/Bios/            romw.bin (+ romJP.bin for Japanese games) for pcsx-ab -
                                                       filled from the BIOS pack unless you put your own there
  RetroArch        $DATA_MOUNT/RetroArch/    roms/<system>/ for its games (a folder per system is there),
                                              system/ the cores' BIOS files (the pack is there), cores/ info/ saves/
                                              states/ playlists/ ... the standard layout. After copying games
                                              in: RetroArch -> Import Content -> Scan Directory -> roms
  Logs             $DATA_MOUNT/System/Logs/
  Themes           $DATA_MOUNT/Themes/

  The $DATA_LABEL partition is exFAT, so you can pull $MEDIUM and drop games on it from Windows, macOS or
  Linux. Windows shows it as a second drive next to the small boot partition (Windows 10 1903 and newer).

  Start it now without rebooting:   sudo systemctl start autobleem
  Watch what it does:               sudo journalctl -u autobleem -f
  Stop it owning the screen:        sudo systemctl disable --now autobleem && sudo systemctl enable --now getty@tty1

  Alt+F2 gives you a login prompt if the launcher ever fails to come up. Enabling SSH before you reboot is a
  good idea: $(ssh_hint)

  Power the machine off from the launcher's L2+R2 menu (Power Off) or with "sudo poweroff", not by pulling
  the plug: an unclean shutdown can leave freshly written files empty.

EOF
    if [ "$DRY_RUN" -eq 1 ]; then
        warn "this was a --dry-run: nothing above was actually changed"
    fi
}

#*******************************
# main
#*******************************
# "@@phase N/M text" lines for autobleem-firstboot.sh's screen (system/autobleem-install-ui.py), which
# turns them into its first progress bar; only with AB_UI_MARKERS=1, a terminal never sees them
PHASES=9
phase() { [ "${AB_UI_MARKERS:-0}" = 1 ] && printf '@@phase %s/%s %s\n' "$1" "$PHASES" "$2"; return 0; }

main() {
    parse_args "$@"
    phase 1 "Preparing"
    preflight
    phase 2 "The system partition"
    maybe_grow_root             # --grow-root: the root must have room before apt fills it
    if [ "$GROW_ONLY" -eq 1 ]; then
        [ -n "$GROW_ROOT_GIB" ] || die "--grow-only needs --grow-root GIB"
        log "--grow-only: stopping here"
        exit 0
    fi
    phase 3 "Installing packages"
    install_packages
    phase 4 "The games partition"
    ensure_data_partition       # may arm --shrink-root and reboot: everything slow comes after it
    mount_data
    create_tree
    phase 5 "RetroArch"
    install_retroarch
    phase 6 "RetroArch cores and assets"
    download_retroarch_content
    download_thumbnails         # the launcher's PS1 box art - wanted with or without RetroArch
    phase 7 "BIOS files"
    download_bios_pack
    phase 8 "AutoBleem"
    install_payload
    install_sample_games
    phase 9 "Boot setup"
    install_service
    install_boot_splash
    configure_boot
    sync
    summary
}

main "$@"
