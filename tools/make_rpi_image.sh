#!/usr/bin/env bash
#
# Build a Raspberry Pi Imager-flashable AutoBleem image: an official Raspberry Pi OS Lite image (armhf or
# arm64) with an AutoBleem package tarball and a first-boot service injected - nothing inside the base image
# is modified beyond that (no chroot, no package pre-install: see docs/rpi-image-and-update-plan.md, "what
# stays out of scope for this round"). Raspberry Pi Imager's own OS customisation (hostname, user, WiFi,
# SSH, locale) keeps working unmodified, because the image's own first-boot mechanism (cloud-init or
# firstrun.sh, whichever the base image ships) is never touched; autobleem-firstboot.service runs after it,
# on the first or second boot (see payload_rpi/system/autobleem-firstboot.sh), and installs AutoBleem onto
# the exFAT data partition install.sh creates, exactly as a manual "tar xzf ... && sudo bash install.sh"
# would.
#
# Two ways to edit the image, same result (2026-09-20): --rootless writes the ext4 root with debugfs
# (e2fsprogs) and the FAT boot partition with mcopy (mtools), nothing mounted, no root - what the build
# server runs inside the Docker image; --mount loop-mounts it and needs root (the Pi 400 way, the
# original). Nothing here is Pi-specific or cross-compiles: it is plain image manipulation.
#
#   docker/run.sh tools/make_rpi_image.sh --arch armhf --package dist/rpi/autobleem-rpi.tar.gz \
#       --work build_rpi_image --out build_rpi_image/out            # the server, after ci/build.sh rpi
#   sudo ./tools/make_rpi_image.sh --arch arm64 --package /path/to/autobleem-rpi-arm64.tar.gz   # a Pi
#
# Build the package first (ci/build.sh rpi rpi64 in the Docker image, or on the PC: ./make_rpi.sh &&
# ./tools/make_rpi_package.sh --arch armhf, likewise make_rpi64.sh / --arch arm64) and point --package at
# it. About 7 minutes per image on the 2-core server, most of it xz.
#
# Output: <out>/autobleem-<version>-rpi-<arch>.img.xz (the version is the package's VERSION file, written by
# tools/make_rpi_package.sh from the build's version.h; --version overrides it), plus <out>/rpi_imager_repo.json (a copy of
# tools/rpi_imager_repo.json with this run's size/hash fields, url and icon filled in - what
# tools/repo_publish.sh image turns into the site's rpi-imager/os_list.json).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

#*******************************
# defaults
#*******************************
ARCH=""                 # armhf | arm64 (required)
PACKAGE=""               # path to autobleem-rpi(.tar.gz|-arm64.tar.gz) (required)
BASE_IMG=""              # --base: a URL or local .img.xz; default is the architecture's official "latest"
WORK_DIR=""              # --work: scratch space (downloaded/decompressed image, loop mount point)
OUT_DIR=""               # --out: where the finished .img.xz and repo.json land (default: same as WORK_DIR)
MODE=""                  # --mount | --rootless: how the image is edited (default: mount as root, rootless otherwise)
XZ_LEVEL="${AB_XZ_LEVEL:-4}"   # xz preset for the output: 4 took 7 min where 6 took 12 on the server, for ~2% more size
KEEP_RAW=0               # --keep-raw: don't delete the decompressed .img after recompressing
VERSION=""               # --version: what to name the image after; default: the package's VERSION file
REPO_URL="${AB_REPO_URL:-https://autobleem.retromenele.pl}"   # --repo: where tools/repo_publish.sh image puts it
DRY_RUN=0

#*******************************
# output helpers
#*******************************
log()  { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[!]\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31m[x]\033[0m %s\n' "$*" >&2; exit 1; }

run() {
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would run: %s\n' "$*"
    else
        "$@"
    fi
}

usage() {
    cat <<'EOF'
Usage: sudo ./tools/make_rpi_image.sh --arch armhf|arm64 --package PATH [options]

  --arch armhf|arm64   which AutoBleem package this is (required)
  --package PATH       the autobleem-rpi(.tar.gz|-arm64.tar.gz) built by tools/make_rpi_package.sh (required)
  --base URL|PATH      the base Raspberry Pi OS Lite image (.img.xz). Default: the architecture's official
                       "latest" Lite image (downloaded fresh, sha256-verified against its published .sha256).
                       A local path is used as-is, not verified.
  --work DIR           scratch directory: download, decompress and loop-mount happen here
                       (default: ./build_rpi_image)
  --out DIR            where the finished .img.xz and repo.json land (default: same as --work)
  --keep-raw           keep the decompressed .img after recompressing (default: deleted to save space)
  --rootless           edit the image without mounting it: debugfs (e2fsprogs) writes the ext4 root, mcopy
                       (mtools) the FAT boot partition - no root, no loop device, works in a container
                       (the build server: docker/run.sh tools/make_rpi_image.sh ...). The default when not
                       root and both tools are there.
  --mount              the loop-mount way (needs root); the default when run with sudo
  --xz-level N         xz preset for the output, 0-9 (default 4, or AB_XZ_LEVEL: 7 min against 12 for 6 on the
                       build server, for about 2% more size)
  --repo URL           the download repository the image will be published to - what the Imager JSON's
                       url and icon fields point at (default: AB_REPO_URL or AutoBleem's; the JSON is
                       laid out as tools/repo_publish.sh image puts things: rpi-imager/images/<version>/)
  --version V          name the image autobleem-V-rpi-<arch>.img.xz (default: the VERSION file inside the
                       package, which tools/make_rpi_package.sh writes from the build's version.h)
  --dry-run            print what would happen and change nothing (no download, no mount, no root needed)
  -h, --help           this text

Needs root (sudo) for the loop-mount way; the rootless way needs only debugfs and mcopy.
EOF
}

#*******************************
# parse_args
#*******************************
parse_args() {
    while [ $# -gt 0 ]; do
        case "$1" in
            --arch)     ARCH="${2:?--arch needs armhf or arm64}"; shift 2 ;;
            --package)  PACKAGE="${2:?--package needs a path}"; shift 2 ;;
            --base)     BASE_IMG="${2:?--base needs a URL or path}"; shift 2 ;;
            --work)     WORK_DIR="${2:?--work needs a directory}"; shift 2 ;;
            --out)      OUT_DIR="${2:?--out needs a directory}"; shift 2 ;;
            --mount)    MODE=mount; shift ;;
            --rootless) MODE=rootless; shift ;;
            --xz-level) XZ_LEVEL="${2:?--xz-level needs 0-9}"; shift 2 ;;
            --keep-raw) KEEP_RAW=1; shift ;;
            --version)  VERSION="${2:?--version needs a value}"; shift 2 ;;
            --repo)     REPO_URL="${2:?--repo needs a URL}"; shift 2 ;;
            --dry-run)  DRY_RUN=1; shift ;;
            -h|--help)  usage; exit 0 ;;
            *)          usage; die "unknown option: $1" ;;
        esac
    done
}

#*******************************
# preflight
#*******************************
preflight() {
    case "$ARCH" in
        armhf|arm64) ;;
        "") die "--arch is required (armhf or arm64)" ;;
        *)  die "--arch takes armhf or arm64 (got '$ARCH')" ;;
    esac
    [ -n "$PACKAGE" ] || die "--package is required - the tarball tools/make_rpi_package.sh built"
    [ -f "$PACKAGE" ] || die "no such file: $PACKAGE"

    [ -n "$WORK_DIR" ] || WORK_DIR="$REPO_DIR/build_rpi_image"
    [ -n "$OUT_DIR" ] || OUT_DIR="$WORK_DIR"

    if [ -z "$MODE" ]; then
        if [ "$(id -u)" -eq 0 ]; then
            MODE=mount
        elif command -v debugfs >/dev/null 2>&1 && command -v mcopy >/dev/null 2>&1; then
            MODE=rootless
        elif [ "$DRY_RUN" -eq 0 ]; then
            die "run this with sudo (losetup/mount need root), or install e2fsprogs + mtools for --rootless"
        fi
    fi
    if [ "$MODE" = mount ] && [ "$DRY_RUN" -eq 0 ] && [ "$(id -u)" -ne 0 ]; then
        die "--mount needs root (losetup/mount) - sudo, or --rootless"
    fi
    log "Editing the image by: ${MODE:-mount} (--mount / --rootless)"

    # xz/sha256sum/tar/python3 are used even in a dry run (well, would be - the dry-run path skips calling
    # them too, but they're ordinary PATH tools for any user, unlike the mount family below, so checking
    # them always is harmless and catches a genuinely missing tool early either way. losetup/mount/umount/
    # mountpoint/udevadm typically live in /sbin, which is on root's PATH (sudo's secure_path) but not a
    # plain user's - checking for them under a plain --dry-run would fail even though dry-run never calls
    # them, so they're only required for a real run.
    local tool
    for tool in xz sha256sum tar python3; do
        command -v "$tool" >/dev/null 2>&1 || die "missing required tool: $tool"
    done
    if [ "$DRY_RUN" -eq 0 ] && [ "$MODE" = mount ]; then
        for tool in losetup udevadm mount umount mountpoint; do
            command -v "$tool" >/dev/null 2>&1 || die "missing required tool: $tool"
        done
    elif [ "$DRY_RUN" -eq 0 ]; then
        for tool in debugfs mcopy; do
            command -v "$tool" >/dev/null 2>&1 || die "missing required tool: $tool (e2fsprogs / mtools, for --rootless)"
        done
    fi
    if [ -z "$BASE_IMG" ] || [[ "$BASE_IMG" == http://* || "$BASE_IMG" == https://* ]]; then
        command -v wget >/dev/null 2>&1 || die "missing required tool: wget (downloading the base image)"
    fi

    log "Architecture: $ARCH"
    log "Package: $PACKAGE"
    if [ -z "$VERSION" ]; then
        # the top-level entry of the tarball is autobleem-rpi/ (tools/make_rpi_package.sh)
        VERSION="$(tar -xzOf "$PACKAGE" autobleem-rpi/VERSION 2>/dev/null | head -1 | tr -d '[:space:]')"
    fi
    if [ -n "$VERSION" ]; then
        log "Version: $VERSION"
    else
        warn "the package has no VERSION file and no --version was given - the image will be named without one"
    fi
    OUT_IMG="$OUT_DIR/autobleem-${VERSION:+$VERSION-}rpi-$ARCH.img.xz"
    log "Work directory: $WORK_DIR"
    log "Output directory: $OUT_DIR"
    run mkdir -p "$WORK_DIR" "$OUT_DIR"

    # Room to work in, checked now rather than found out an hour later: the base image compressed (~0.6 GB)
    # and decompressed (~3.1 GB) sit in the work directory together while the output (~0.7 GB) is written -
    # an 8 GB Pi root with RetroArch's source tree and the apt cache on it ran out at the very last step.
    # On a Pi, --work/--out on the data partition is the natural place.
    if [ "$DRY_RUN" -eq 0 ]; then
        local need_work_mib=5000 need_out_mib=800 free_mib
        free_mib="$(df -Pm "$WORK_DIR" | awk 'NR == 2 { print $4 }')"
        if [ "$(df -P "$WORK_DIR" | awk 'NR == 2 { print $1 }')" = "$(df -P "$OUT_DIR" | awk 'NR == 2 { print $1 }')" ]; then
            need_work_mib=$((need_work_mib + need_out_mib))
        else
            local free_out_mib
            free_out_mib="$(df -Pm "$OUT_DIR" | awk 'NR == 2 { print $4 }')"
            [ "$free_out_mib" -ge "$need_out_mib" ] \
                || die "only ${free_out_mib} MiB free under $OUT_DIR - the image needs about ${need_out_mib} MiB there (--out elsewhere?)"
        fi
        [ "$free_mib" -ge "$need_work_mib" ] \
            || die "only ${free_mib} MiB free under $WORK_DIR - this needs about ${need_work_mib} MiB (the base image, its
    decompressed copy and the output). --work somewhere roomier, e.g. the data partition on a Pi."
    fi
}

#*******************************
# default_base_url
#*******************************
# Raspberry Pi Foundation's stable "latest" redirect for each architecture's Lite image - verified live
# 2026-09-19 (resolves to a dated raspios_lite_<arch>/images/... .img.xz, each with a same-named .sha256
# sidecar next to it). If this ever stops resolving, pass --base with a direct URL or local file instead.
default_base_url() {
    case "$ARCH" in
        armhf) echo "https://downloads.raspberrypi.com/raspios_lite_armhf_latest" ;;
        arm64) echo "https://downloads.raspberrypi.com/raspios_lite_arm64_latest" ;;
    esac
}

#*******************************
# resolve_base_image
#*******************************
# sets BASE_IMG_XZ to a local, verified (when downloaded) .img.xz path, and BASE_RELEASE_DATE to whatever
# date the file name carries (Raspberry Pi OS image file names are "<YYYY-MM-DD>-raspios-<codename>-<arch>-lite.img.xz").
resolve_base_image() {
    local src="$BASE_IMG"
    [ -n "$src" ] || src="$(default_base_url)"

    if [[ "$src" != http://* && "$src" != https://* ]]; then
        [ -f "$src" ] || die "no such file: $src"
        log "Using local base image: $src (not sha256-verified - only downloaded images are)"
        BASE_IMG_XZ="$src"
        BASE_RELEASE_DATE="$(basename "$src" | grep -oE '^[0-9]{4}-[0-9]{2}-[0-9]{2}' || true)"
        return 0
    fi

    log "Resolving $src"
    local final_url
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would resolve the redirect and download the base image + its .sha256\n'
        BASE_IMG_XZ="$WORK_DIR/base-$ARCH.img.xz"
        BASE_RELEASE_DATE="__unknown_in_dry_run__"
        return 0
    fi
    # NOT -q: some wget builds suppress --server-response's header dump under -q too, silently breaking
    # this (found live on the Pi 400's wget 1.25 - -q ate the header lines, final_url came back empty, and
    # the fallback below then downloaded and named the file after the alias URL instead of the real dated
    # one, which made the .sha256 check fail with a filename mismatch: the sidecar names the dated file,
    # not the alias). The extra chatter this prints is captured into a variable, never shown.
    #
    # Two different lines can carry the redirect target, and neither is what an early version of this
    # script assumed (also found live, the hard way): --server-response echoes the raw HTTP header, which
    # can be sent by the server in any case ("  location: <url>", two-space indented, lowercase, on this
    # server) - and wget's own "I'm following this" message is a separate line ("Location: <url>
    # [following]", capitalised, flush left, no indent). Match either, case-insensitively, regardless of
    # leading whitespace; $2 is the URL in both formats since awk's default field splitting ignores
    # leading whitespace anyway.
    final_url="$(wget --max-redirect=5 --server-response "$src" -O /dev/null 2>&1 \
        | awk 'tolower($0) ~ /^[[:space:]]*location:/ {u=$2} END{print u}')"
    # the loop above keeps the last one, which is the final, dated URL we actually want to name the file after
    if [ -z "$final_url" ]; then
        final_url="$src" # not a redirect after all - use it as given
    fi
    local fname
    fname="$(basename "$final_url")"
    BASE_IMG_XZ="$WORK_DIR/$fname"
    BASE_RELEASE_DATE="$(echo "$fname" | grep -oE '^[0-9]{4}-[0-9]{2}-[0-9]{2}' || true)"

    log "Downloading $final_url"
    wget -c -O "$BASE_IMG_XZ" "$final_url" || die "download failed"

    log "Downloading and checking $fname.sha256"
    if wget -q -O "$WORK_DIR/$fname.sha256" "$final_url.sha256"; then
        ( cd "$WORK_DIR" && sha256sum -c "$fname.sha256" ) || die "sha256 mismatch on $fname - re-download or check --base"
        log "sha256 OK"
    else
        warn "no .sha256 published next to $fname - continuing unverified"
    fi
}

#*******************************
# decompress_base_image
#*******************************
decompress_base_image() {
    RAW_IMG="$WORK_DIR/${ARCH}.img"
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would decompress %s -> %s\n' "$BASE_IMG_XZ" "$RAW_IMG"
        return 0
    fi
    log "Decompressing $(basename "$BASE_IMG_XZ")"
    rm -f "$RAW_IMG"
    xz -T0 -dk -c "$BASE_IMG_XZ" >"$RAW_IMG"
}

#*******************************
# mount_image / unmount_image
#*******************************
# Both partitions are mounted: the payload goes under /opt on the root filesystem (the FAT boot partition
# is small and shared with the kernel/firmware - see docs/rpi-image-and-update-plan.md), and the boot
# partition gets two small edits: cmdline.txt loses the word "resize" (see inject_boot_files) and gains
# autobleem.txt (the first-boot options, editable from any PC).
LOOP_DEV=""
ROOT_MNT=""
BOOT_MNT=""

BOOT_OFF=""   # rootless mode: byte offsets of the two partitions inside the raw image
ROOT_OFF=""

# the MBR's partition table, read by hand (16-byte entries at 0x1be: the LBA start is bytes 8-11): no
# sfdisk, no root - a Raspberry Pi OS image is always MBR with the boot partition first and the root second
partition_offsets() {
    python3 - "$RAW_IMG" <<'PY'
import struct, sys
with open(sys.argv[1], "rb") as f:
    f.seek(0x1be)
    table = f.read(64)
starts = [struct.unpack_from("<I", table, i * 16 + 8)[0] * 512 for i in range(2)]
print(starts[0], starts[1])
PY
}

mount_image() {
    if [ "$DRY_RUN" -eq 1 ]; then
        if [ "$MODE" = rootless ]; then
            printf '    would read the partition table of %s and edit its two partitions in place\n' "$RAW_IMG"
        else
            printf '    would losetup -fP %s and mount its root (2nd) partition\n' "$RAW_IMG"
        fi
        return 0
    fi
    if [ "$MODE" = rootless ]; then
        read -r BOOT_OFF ROOT_OFF < <(partition_offsets)
        [ "${ROOT_OFF:-0}" -gt 0 ] || die "no second partition in $RAW_IMG - is this a Raspberry Pi OS image?"
        log "Partitions of $RAW_IMG: boot at $BOOT_OFF, root at $ROOT_OFF (rootless - not mounted)"
        debugfs -R "ls -l /etc" "$RAW_IMG?offset=$ROOT_OFF" >/dev/null 2>&1 \
            || die "debugfs cannot read the root filesystem at offset $ROOT_OFF"
        return 0
    fi
    log "Mounting $RAW_IMG"
    LOOP_DEV="$(losetup -fP --show "$RAW_IMG")"
    udevadm settle
    [ -b "${LOOP_DEV}p2" ] || die "no ${LOOP_DEV}p2 - is this really a Raspberry Pi OS image (boot + root)?"
    ROOT_MNT="$WORK_DIR/root-mnt"
    BOOT_MNT="$WORK_DIR/boot-mnt"
    mkdir -p "$ROOT_MNT" "$BOOT_MNT"
    mount "${LOOP_DEV}p2" "$ROOT_MNT"
    mount "${LOOP_DEV}p1" "$BOOT_MNT"
}

unmount_image() {
    [ "$DRY_RUN" -eq 1 ] && return 0
    [ "$MODE" = rootless ] && return 0
    local m
    for m in "$BOOT_MNT" "$ROOT_MNT"; do
        if [ -n "$m" ] && mountpoint -q "$m" 2>/dev/null; then
            sync
            umount "$m" || warn "umount $m failed"
        fi
    done
    if [ -n "$LOOP_DEV" ]; then
        losetup -d "$LOOP_DEV" 2>/dev/null || warn "losetup -d $LOOP_DEV failed"
    fi
    LOOP_DEV=""
    ROOT_MNT=""
    BOOT_MNT=""
}

# unmount/detach on any exit (success, error, or an interrupted run) so a failed build never leaves a loop
# device or a stray mount behind for the next run to trip over
trap unmount_image EXIT

#*******************************
# inject_payload
#*******************************
inject_payload() {
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would copy %s and the firstboot service/script into the image, and enable the service\n' "$PACKAGE"
        return 0
    fi
    log "Injecting the AutoBleem package and first-boot service"
    if [ "$MODE" = rootless ]; then
        inject_payload_rootless
        return 0
    fi
    local image_dir="$ROOT_MNT/opt/autobleem-image"
    mkdir -p "$image_dir"
    cp "$PACKAGE" "$image_dir/autobleem-rpi.tar.gz"
    install -m 0755 "$REPO_DIR/payload_rpi/system/autobleem-firstboot.sh" "$image_dir/autobleem-firstboot.sh"
    install -m 0644 "$REPO_DIR/payload_rpi/system/autobleem-firstboot.service" \
        "$ROOT_MNT/etc/systemd/system/autobleem-firstboot.service"

    # "systemctl enable" for a plain WantedBy=multi-user.target unit is just this symlink - done by hand
    # rather than chrooting to run systemctl itself, which is exactly the "no qemu/chroot" simplification
    # injection-only buys: this works identically for an armhf image built on an aarch64 host or vice versa,
    # because nothing from the image is ever executed at build time.
    mkdir -p "$ROOT_MNT/etc/systemd/system/multi-user.target.wants"
    ln -sf ../autobleem-firstboot.service \
        "$ROOT_MNT/etc/systemd/system/multi-user.target.wants/autobleem-firstboot.service"
}

# the same five writes through debugfs, on the unmounted root filesystem. `write` gives the new file the
# local file's uid/gid/mode - ours, not root's - so every one is set explicitly (mode is the full st_mode).
inject_payload_rootless() {
    local fs="$RAW_IMG?offset=$ROOT_OFF"
    # (the banner, "Allocated inode" chatter and "already exists" on a mkdir of a directory the base image
    # has are noise; anything else debugfs says is shown)
    dfs() {
        debugfs -w -R "$1" "$fs" 2>&1 \
            | grep -vE '^debugfs 1\.|^Allocated inode|directory already exists|^$' || true
    }
    put() { # put LOCAL IMAGEPATH MODE
        dfs "rm $2" >/dev/null   # a rerun over the same raw image would fail on an existing file
        dfs "write $1 $2"
        dfs "sif $2 uid 0"
        dfs "sif $2 gid 0"
        dfs "sif $2 mode $3"
    }
    dfs "mkdir /opt/autobleem-image"
    put "$PACKAGE" /opt/autobleem-image/autobleem-rpi.tar.gz 0100644
    put "$REPO_DIR/payload_rpi/system/autobleem-firstboot.sh" /opt/autobleem-image/autobleem-firstboot.sh 0100755
    put "$REPO_DIR/payload_rpi/system/autobleem-firstboot.service" /etc/systemd/system/autobleem-firstboot.service 0100644
    dfs "mkdir /etc/systemd/system/multi-user.target.wants"
    dfs "rm /etc/systemd/system/multi-user.target.wants/autobleem-firstboot.service" >/dev/null
    dfs "symlink /etc/systemd/system/multi-user.target.wants/autobleem-firstboot.service ../autobleem-firstboot.service"
    # what went in, as the image will see it
    debugfs -R "ls -l /opt/autobleem-image" "$fs" 2>/dev/null | grep -v '^debugfs' | sed 's/^/    /'
    debugfs -R "ls -l /etc/systemd/system/multi-user.target.wants" "$fs" 2>/dev/null | grep autobleem | sed 's/^/    /'
    # every file went in whole
    local want got
    want="$(stat -c %s "$PACKAGE")"
    got="$(debugfs -R "stat /opt/autobleem-image/autobleem-rpi.tar.gz" "$fs" 2>/dev/null | sed -n 's/.*Size: \([0-9]*\).*/\1/p' | head -1)"
    [ "$want" = "$got" ] || die "the package inside the image is $got bytes, the file is $want"
}

#*******************************
# inject_boot_files
#*******************************
# Two edits on the FAT boot partition. cmdline.txt loses the word "resize": that is what the base image's
# initramfs keys on to grow the root partition over the whole card on the first boot
# (/usr/share/initramfs-tools/scripts/local-premount/resize_early, and set_partuuid next to it), and a
# root that fills the card leaves install.sh no room for the exFAT data partition - instead install.sh
# grows the root to a bounded size itself (--grow-root, from autobleem.txt's root_gib) and takes the rest.
# Nothing else on the partition is touched: cloud-init's user-data/network-config/meta-data stay exactly as
# the base image ships them, so Raspberry Pi Imager's OS customisation still lands on top of them.
inject_boot_files() {
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would drop "resize" from cmdline.txt and add autobleem.txt on the boot partition\n'
        return 0
    fi
    log "Editing the boot partition: cmdline.txt without 'resize', plus autobleem.txt"
    local cmdline kept="" word
    if [ "$MODE" = rootless ]; then
        # mtools reads and writes the FAT partition in place: <image>@@<byte offset>
        cmdline="$WORK_DIR/cmdline.txt"
        mcopy -o -i "$RAW_IMG@@$BOOT_OFF" ::cmdline.txt "$cmdline" \
            || die "no cmdline.txt on the boot partition - not a Raspberry Pi OS image?"
    else
        cmdline="$BOOT_MNT/cmdline.txt"
        [ -f "$cmdline" ] || die "no cmdline.txt on the boot partition - not a Raspberry Pi OS image?"
    fi
    for word in $(tr -d '\n' <"$cmdline"); do
        [ "$word" = resize ] && continue
        kept="$kept${kept:+ }$word"
    done
    printf '%s\n' "$kept" >"$cmdline"
    if [ "$MODE" = rootless ]; then
        mcopy -o -i "$RAW_IMG@@$BOOT_OFF" "$cmdline" ::cmdline.txt || die "writing cmdline.txt failed"
        mcopy -o -i "$RAW_IMG@@$BOOT_OFF" "$REPO_DIR/payload_rpi/system/autobleem.txt" ::autobleem.txt \
            || die "writing autobleem.txt failed"
        rm -f "$cmdline"
        mdir -i "$RAW_IMG@@$BOOT_OFF" ::autobleem.txt ::cmdline.txt | grep -iE "autobleem|cmdline" | sed 's/^/    /'
    else
        install -m 0644 "$REPO_DIR/payload_rpi/system/autobleem.txt" "$BOOT_MNT/autobleem.txt"
    fi
}

#*******************************
# finalize_image
#*******************************
finalize_image() {
    local out_img="$OUT_IMG"
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would recompress %s -> %s and hash both\n' "$RAW_IMG" "$out_img"
        EXTRACT_SIZE=0; EXTRACT_SHA256="0"; DOWNLOAD_SIZE=0; DOWNLOAD_SHA256="0"
        OUT_IMG="$out_img"
        return 0
    fi

    log "Hashing the raw image (this is 'extract_size'/'extract_sha256')"
    EXTRACT_SIZE="$(stat -c%s "$RAW_IMG")"
    EXTRACT_SHA256="$(sha256sum "$RAW_IMG" | cut -d' ' -f1)"

    log "Recompressing to $out_img (xz -T0 -$XZ_LEVEL, this can take a while)"
    rm -f "$out_img"
    if ! xz -T0 "-$XZ_LEVEL" -c "$RAW_IMG" >"$out_img"; then
        rm -f "$out_img" # never leave a truncated image that looks like a real one
        die "xz failed writing $out_img (out of space?) - the decompressed image is kept in $WORK_DIR"
    fi
    [ "$KEEP_RAW" -eq 1 ] || rm -f "$RAW_IMG"

    DOWNLOAD_SIZE="$(stat -c%s "$out_img")"
    DOWNLOAD_SHA256="$(sha256sum "$out_img" | cut -d' ' -f1)"
    OUT_IMG="$out_img"
}

#*******************************
# update_repo_json
#*******************************
# merges this run's real size/hash fields into <out>/rpi_imager_repo.json, starting from a copy of the
# checked-in template on the first run so a second `make_rpi_image.sh --arch <other>` invocation fills in
# the other architecture's entry alongside, not over, this one's.
update_repo_json() {
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would update %s/rpi_imager_repo.json for %s\n' "$OUT_DIR" "$ARCH"
        return 0
    fi
    local out_json="$OUT_DIR/rpi_imager_repo.json"
    [ -f "$out_json" ] || cp "$SCRIPT_DIR/rpi_imager_repo.json" "$out_json"

    ARCH="$ARCH" EXTRACT_SIZE="$EXTRACT_SIZE" EXTRACT_SHA256="$EXTRACT_SHA256" \
    DOWNLOAD_SIZE="$DOWNLOAD_SIZE" DOWNLOAD_SHA256="$DOWNLOAD_SHA256" \
    RELEASE_DATE="$BASE_RELEASE_DATE" VERSION="$VERSION" OUT_JSON="$out_json" \
    IMAGE_URL="$REPO_URL/rpi-imager/images/${VERSION:-unversioned}/$(basename "$OUT_IMG")" \
    ICON_URL="$REPO_URL/rpi-imager/icon.png" python3 - <<'PYEOF'
import json, os, re

path = os.environ["OUT_JSON"]
arch = os.environ["ARCH"]
name = "AutoBleem (32-bit)" if arch == "armhf" else "AutoBleem (64-bit)"

with open(path, encoding="utf-8") as f:
    data = json.load(f)

for entry in data["os_list"]:
    if entry["name"] == name:
        entry["extract_size"] = int(os.environ["EXTRACT_SIZE"])
        entry["extract_sha256"] = os.environ["EXTRACT_SHA256"]
        entry["image_download_size"] = int(os.environ["DOWNLOAD_SIZE"])
        entry["image_download_sha256"] = os.environ["DOWNLOAD_SHA256"]
        # where tools/repo_publish.sh image will put it (repo_index.py fills these in again from what it
        # finds, so a different --repo at publish time still ends up right)
        entry["url"] = os.environ["IMAGE_URL"]
        entry["icon"] = os.environ["ICON_URL"]
        if os.environ["RELEASE_DATE"]:
            entry["release_date"] = os.environ["RELEASE_DATE"]
        if os.environ["VERSION"]:
            # the version goes in the description Imager shows under the name; a rerun replaces, not appends
            base = re.sub(r" AutoBleem [^ ]+\.$", "", entry.get("description", "").rstrip())
            entry["description"] = f"{base} AutoBleem {os.environ['VERSION']}."
        break
else:
    raise SystemExit(f"no os_list entry named {name!r} in {path}")

with open(path, "w", encoding="utf-8") as f:
    json.dump(data, f, indent=2)
    f.write("\n")
PYEOF
}

#*******************************
# summary
#*******************************
summary() {
    log "Done."
    if [ "$DRY_RUN" -eq 1 ]; then
        warn "this was a --dry-run: nothing above actually happened"
        return 0
    fi
    cat <<EOF

  Image:      $OUT_IMG
  Repo JSON:  $OUT_DIR/rpi_imager_repo.json  (url/icon point at $REPO_URL; tools/repo_publish.sh image
              puts the image and this file on the site, where it becomes rpi-imager/os_list.json)

  Test it with Raspberry Pi Imager: "Use custom", point at the .img.xz directly (no customisation offered
  that way), or at the JSON via tools/rpi_imager_local_manifest.py / --repo for hostname/WiFi/SSH/user
  setup too.

EOF
}

#*******************************
# main
#*******************************
main() {
    parse_args "$@"
    preflight
    resolve_base_image
    decompress_base_image
    mount_image
    inject_payload
    inject_boot_files
    unmount_image
    finalize_image
    update_repo_json
    summary
}

main "$@"
