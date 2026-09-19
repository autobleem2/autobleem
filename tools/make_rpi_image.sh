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
# Run this ON a Linux host with root (losetup/mount) - in practice the Pi 400 build host this project
# already uses over ssh (see CLAUDE.md's "Pi test setup"), though nothing here is Pi-specific: it is plain
# image manipulation (loop-mount, copy files, recompress), not cross-compiling.
#
#   ./tools/make_rpi_image.sh --arch armhf --package /path/to/autobleem-rpi.tar.gz
#   ./tools/make_rpi_image.sh --arch arm64 --package /path/to/autobleem-rpi-arm64.tar.gz
#
# Build the package first, on the PC, the usual way (see payload_rpi/README.md):
#   ./make_rpi.sh   && ./tools/make_rpi_package.sh --arch armhf   # -> build_rpi/autobleem-rpi.tar.gz
#   ./make_rpi64.sh && ./tools/make_rpi_package.sh --arch arm64   # -> build_rpi64/autobleem-rpi-arm64.tar.gz
# then copy the tarball to this host and point --package at it.
#
# Output: <out>/autobleem-rpi-image-<arch>.img.xz, plus <out>/rpi_imager_repo.json (a copy of
# tools/rpi_imager_repo.json with this run's size/hash fields filled in - see that file and
# payload_rpi/README.md's "Flashing with Raspberry Pi Imager" section for what still needs filling in by
# hand: where you host the .img.xz, an icon, and the device-tag list).
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
KEEP_RAW=0               # --keep-raw: don't delete the decompressed .img after recompressing
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
  --dry-run            print what would happen and change nothing (no download, no mount, no root needed)
  -h, --help           this text

Must be run as root (sudo) for losetup/mount, except --dry-run.
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
            --keep-raw) KEEP_RAW=1; shift ;;
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

    if [ "$DRY_RUN" -eq 0 ] && [ "$(id -u)" -ne 0 ]; then
        die "run this with sudo (losetup/mount need root) - or pass --dry-run"
    fi

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
    if [ "$DRY_RUN" -eq 0 ]; then
        for tool in losetup udevadm mount umount mountpoint; do
            command -v "$tool" >/dev/null 2>&1 || die "missing required tool: $tool"
        done
    fi
    if [ -z "$BASE_IMG" ] || [[ "$BASE_IMG" == http://* || "$BASE_IMG" == https://* ]]; then
        command -v wget >/dev/null 2>&1 || die "missing required tool: wget (downloading the base image)"
    fi

    log "Architecture: $ARCH"
    log "Package: $PACKAGE"
    log "Work directory: $WORK_DIR"
    log "Output directory: $OUT_DIR"
    run mkdir -p "$WORK_DIR" "$OUT_DIR"
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
# only the root (ext4) partition is mounted - the payload goes under /opt on the root filesystem, not the
# FAT boot partition (see docs/rpi-image-and-update-plan.md: boot is small and shared with the
# kernel/firmware, and the injected tree is tens of MB, not worth the risk of crowding it).
LOOP_DEV=""
ROOT_MNT=""

mount_image() {
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would losetup -fP %s and mount its root (2nd) partition\n' "$RAW_IMG"
        return 0
    fi
    log "Mounting $RAW_IMG"
    LOOP_DEV="$(losetup -fP --show "$RAW_IMG")"
    udevadm settle
    [ -b "${LOOP_DEV}p2" ] || die "no ${LOOP_DEV}p2 - is this really a Raspberry Pi OS image (boot + root)?"
    ROOT_MNT="$WORK_DIR/root-mnt"
    mkdir -p "$ROOT_MNT"
    mount "${LOOP_DEV}p2" "$ROOT_MNT"
}

unmount_image() {
    [ "$DRY_RUN" -eq 1 ] && return 0
    if [ -n "$ROOT_MNT" ] && mountpoint -q "$ROOT_MNT" 2>/dev/null; then
        sync
        umount "$ROOT_MNT" || warn "umount $ROOT_MNT failed"
    fi
    if [ -n "$LOOP_DEV" ]; then
        losetup -d "$LOOP_DEV" 2>/dev/null || warn "losetup -d $LOOP_DEV failed"
    fi
    LOOP_DEV=""
    ROOT_MNT=""
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
    local image_dir="$ROOT_MNT/opt/autobleem-image"
    mkdir -p "$image_dir"
    cp "$PACKAGE" "$image_dir/autobleem-rpi.tar.gz"
    install -m 0755 "$SCRIPT_DIR/../payload_rpi/system/autobleem-firstboot.sh" "$image_dir/autobleem-firstboot.sh"
    install -m 0644 "$SCRIPT_DIR/../payload_rpi/system/autobleem-firstboot.service" \
        "$ROOT_MNT/etc/systemd/system/autobleem-firstboot.service"

    # "systemctl enable" for a plain WantedBy=multi-user.target unit is just this symlink - done by hand
    # rather than chrooting to run systemctl itself, which is exactly the "no qemu/chroot" simplification
    # injection-only buys: this works identically for an armhf image built on an aarch64 host or vice versa,
    # because nothing from the image is ever executed at build time.
    mkdir -p "$ROOT_MNT/etc/systemd/system/multi-user.target.wants"
    ln -sf ../autobleem-firstboot.service \
        "$ROOT_MNT/etc/systemd/system/multi-user.target.wants/autobleem-firstboot.service"
}

#*******************************
# finalize_image
#*******************************
finalize_image() {
    local out_img="$OUT_DIR/autobleem-rpi-image-$ARCH.img.xz"
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would recompress %s -> %s and hash both\n' "$RAW_IMG" "$out_img"
        EXTRACT_SIZE=0; EXTRACT_SHA256="0"; DOWNLOAD_SIZE=0; DOWNLOAD_SHA256="0"
        OUT_IMG="$out_img"
        return 0
    fi

    log "Hashing the raw image (this is 'extract_size'/'extract_sha256')"
    EXTRACT_SIZE="$(stat -c%s "$RAW_IMG")"
    EXTRACT_SHA256="$(sha256sum "$RAW_IMG" | cut -d' ' -f1)"

    log "Recompressing to $out_img (xz -T0, this can take a while)"
    rm -f "$out_img"
    xz -T0 -6 -c "$RAW_IMG" >"$out_img"
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
    RELEASE_DATE="$BASE_RELEASE_DATE" OUT_JSON="$out_json" python3 - <<'PYEOF'
import json, os

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
        if os.environ["RELEASE_DATE"]:
            entry["release_date"] = os.environ["RELEASE_DATE"]
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
  Repo JSON:  $OUT_DIR/rpi_imager_repo.json  (still needs 'url', 'icon' and, if you want one, a 'devices'
              filter filled in by hand before it's usable with Raspberry Pi Imager - see
              payload_rpi/README.md's "Flashing with Raspberry Pi Imager" section)

  Test it with Raspberry Pi Imager: "Use custom", point at the .img.xz directly (no customisation offered
  that way), or at the filled-in JSON via a local file / --repo for hostname/WiFi/SSH/user setup too.

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
    unmount_image
    finalize_image
    update_repo_json
    summary
}

main "$@"
