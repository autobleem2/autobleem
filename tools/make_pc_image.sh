#!/usr/bin/env bash
#
# Build the flashable AutoBleem image for a PC USB stick: a 32-bit Debian 12 (Bookworm, i386 - the last Debian
# with a 32-bit x86 kernel, so older CPUs boot it too) built from packages with mmdebstrap, GRUB for BIOS and
# UEFI, and the AutoBleem package plus the first-boot service injected the way tools/make_rpi_image.sh
# injects them into a Pi image. Written to a stick with Rufus (DD mode), Etcher or dd, it boots on any PC
# and its first boot (payload_linux/system/autobleem-firstboot.sh) installs AutoBleem onto the exFAT data
# partition install.sh makes out of the rest of the stick - exactly the Pi flow.
#
# Debian publishes no raw disk image for i386, hence "from packages": mmdebstrap makes the root filesystem
# (no qemu - i386 runs natively on the amd64 build host), mke2fs -d turns it into the ext4 root, the
# partition table and the ESP are written on the image file (sfdisk, mtools), GRUB's images come from the
# host's grub-*-bin packages (grub-mkimage) and boot.img/core.img are written into the MBR by hand: no
# loop device is ever needed. Two modes differ only in how mmdebstrap gets its root: --rootless
# (--mode=unshare, a user namespace; docker/run.sh --userns on a host whose kernel lets a container map
# one) and --mount (--mode=root; docker/run.sh --privileged - what the build server runs, its kernel
# refusing newuidmap in a container - or sudo on a Linux machine).
#
# Three kernels, GRUB picks with cpuid: linux-image-686 for a CPU without PAE, linux-image-686-pae for the
# rest of the 32-bit world, and linux-image-amd64 for a 64-bit CPU - GRUB's x86_64-efi loader refuses to
# start a 32-bit kernel ("kernel doesn't support 64-bit CPUs"), so a 64-bit UEFI machine needs it. Bookworm's
# i386 archive has no amd64 kernel any more; it is installed the way Debian's release notes describe for an
# i386 system that wants one - amd64 added as a foreign architecture, linux-image-amd64:amd64 - a 32-bit
# userland under a 64-bit kernel. The userland is i386 whichever kernel runs. Secure Boot must be off:
# nothing here is signed.
#
#   docker/run.sh --privileged tools/make_pc_image.sh --package dist/pcusb/autobleem-pcusb-i386.tar.gz \
#       --work build_pc_image --out build_pc_image/out            # the server, after ci/build.sh pcusb
#
# Output: <out>/autobleem-<version>-pcusb-i386.img.xz + .sha256 (the version is the package's VERSION
# file; --version overrides it). Layout: p1 the ESP (FAT32, label ABBOOT: EFI/BOOT/BOOTIA32.EFI and
# BOOTX64.EFI, autobleem.txt), p2 the root (ext4, label AUTOBLEEM_ROOT, --root-size), nothing after it -
# install.sh --grow-root grows the root to autobleem.txt's root_gib on the first boot and makes the data
# partition of the rest.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

#*******************************
# defaults
#*******************************
PACKAGE=""               # --package: autobleem-pcusb-i386.tar.gz (required)
WORK_DIR=""              # --work: scratch space (the root tarball, the raw image)
OUT_DIR=""               # --out: where the finished .img.xz lands (default: same as WORK_DIR)
MODE=""                  # --rootless | --mount (default: mount as root, rootless otherwise)
RELEASE="${AB_PC_DEBIAN_RELEASE:-bookworm}"   # --release: the Debian release (bookworm is the last with an i386 kernel)
MIRROR="${AB_PC_DEBIAN_MIRROR:-http://deb.debian.org/debian}"   # --mirror
ROOT_SIZE="${AB_PC_ROOT_SIZE:-4G}"   # --root-size: the root partition in the image (grown on the first boot)
ESP_SIZE="${AB_PC_ESP_SIZE:-256M}"   # --esp-size
XZ_LEVEL="${AB_XZ_LEVEL:-4}"         # --xz-level
KEEP_RAW=0               # --keep-raw
VERSION=""               # --version
DRY_RUN=0
NO_AMD64_KERNEL=0        # --no-amd64-kernel: leave the 64-bit kernel out (BIOS and 32-bit UEFI machines only)

# what the root gets. The launcher's own runtime (SDL2, Mesa's GL/EGL/GLES + GBM for kmsdrm, libpng),
# plymouth for the splash, exfatprogs/parted/e2fsprogs for install.sh's partitioning, NetworkManager +
# wpasupplicant + iw/rfkill for the first boot's WiFi question, python3 for the first-boot screen, the
# non-free firmware for WiFi chips, sound and GPUs (nouveau needs it for anything Maxwell or newer), and
# the *-bin GRUB packages - never grub-pc/grub-efi-*, whose postinst would try to grub-install on a debconf
# device. What install.sh apt-gets on the first boot is here already, so that boot is short.
PACKAGES_COMMON="initramfs-tools systemd systemd-sysv systemd-timesyncd udev dbus kmod sudo locales
  console-setup kbd less nano python3 network-manager wpasupplicant iw wireless-regdb rfkill iproute2
  ca-certificates curl wget unzip xz-utils parted exfatprogs e2fsprogs dosfstools util-linux alsa-utils
  plymouth libsdl2-2.0-0 libsdl2-image-2.0-0 libsdl2-mixer-2.0-0 libsdl2-ttf-2.0-0 libpng16-16 zlib1g
  libgl1 libgl1-mesa-dri libegl1 libgles2 libgbm1 mesa-va-drivers grub2-common grub-pc-bin grub-efi-ia32-bin
  grub-efi-amd64-bin openssh-server pciutils usbutils firmware-linux-free firmware-misc-nonfree
  firmware-amd-graphics firmware-iwlwifi firmware-atheros firmware-realtek firmware-brcm80211
  firmware-intel-sound firmware-sof-signed fontconfig-config fonts-dejavu-core zstd"
# (fontconfig-config: plymouth's initramfs hook copies /etc/fonts/fonts.conf and fails without it; zstd:
# the initramfs compressor initramfs-tools prefers, gzip otherwise)
PACKAGES_KERNELS="linux-image-686-pae linux-image-686"
PACKAGES_AMD64_KERNEL="linux-image-amd64:amd64"   # a foreign-architecture package (see the header)

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
Usage: tools/make_pc_image.sh --package PATH [options]

  --package PATH       autobleem-pcusb-i386.tar.gz, built by ci/build.sh pcusb / tools/make_rpi_package.sh --arch i386 (required)
  --work DIR           scratch directory: the root tarball and the raw image are made here (default: ./build_pc_image)
  --out DIR            where the finished .img.xz and .sha256 land (default: same as --work)
  --rootless           no root, no loop device: mmdebstrap --mode=unshare, the root filesystem made from
                       inside its user namespace, the partition table and ESP written on the image file, GRUB
                       by grub-mkimage. The default when not root (docker/run.sh --userns, where the kernel lets a container map one)
  --mount              with root: mmdebstrap --mode=root - the default when run as root (docker/run.sh
                       --privileged, what the build server uses, or sudo on a Linux machine)
  --release NAME       the Debian release (default: bookworm - the last with an i386 kernel)
  --mirror URL         the Debian mirror (default: http://deb.debian.org/debian)
  --root-size SIZE     the root partition in the image, parted/truncate syntax (default: 4G; the first boot
                       grows it to autobleem.txt's root_gib and makes the data partition of the rest)
  --esp-size SIZE      the EFI system partition (default: 256M)
  --no-amd64-kernel    no 64-bit kernel: BIOS and 32-bit-UEFI machines only, ~80 MB less (see the header)
  --xz-level N         xz preset for the output, 0-9 (default 4, or AB_XZ_LEVEL)
  --keep-raw           keep the decompressed .img after recompressing
  --version V          name the image autobleem-V-pcusb-i386.img.xz (default: the package's VERSION file)
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
            --package)   PACKAGE="${2:?--package needs a path}"; shift 2 ;;
            --work)      WORK_DIR="${2:?--work needs a directory}"; shift 2 ;;
            --out)       OUT_DIR="${2:?--out needs a directory}"; shift 2 ;;
            --rootless)  MODE=rootless; shift ;;
            --mount)     MODE=mount; shift ;;
            --release)   RELEASE="${2:?--release needs a name}"; shift 2 ;;
            --mirror)    MIRROR="${2:?--mirror needs a URL}"; shift 2 ;;
            --root-size) ROOT_SIZE="${2:?--root-size needs a size}"; shift 2 ;;
            --esp-size)  ESP_SIZE="${2:?--esp-size needs a size}"; shift 2 ;;
            --no-amd64-kernel) NO_AMD64_KERNEL=1; shift ;;
            --xz-level)  XZ_LEVEL="${2:?--xz-level needs 0-9}"; shift 2 ;;
            --keep-raw)  KEEP_RAW=1; shift ;;
            --version)   VERSION="${2:?--version needs a value}"; shift 2 ;;
            --dry-run)   DRY_RUN=1; shift ;;
            -h|--help)   usage; exit 0 ;;
            *)           usage; die "unknown option: $1" ;;
        esac
    done
}

#*******************************
# preflight
#*******************************
preflight() {
    [ -n "$PACKAGE" ] || die "--package is required - the tarball ci/build.sh pcusb built"
    [ -f "$PACKAGE" ] || [ "$DRY_RUN" -eq 1 ] || die "no such package: $PACKAGE"
    PACKAGE="$(cd "$(dirname "$PACKAGE")" && pwd)/$(basename "$PACKAGE")"
    case "$(basename "$PACKAGE")" in
        autobleem-pcusb*.tar.gz) ;;
        *) die "the package should be autobleem-pcusb-i386.tar.gz (got $(basename "$PACKAGE"))" ;;
    esac

    if [ -z "$MODE" ]; then
        if [ "$(id -u)" -eq 0 ]; then MODE=mount; else MODE=rootless; fi
    fi
    if [ "$MODE" = mount ] && [ "$(id -u)" -ne 0 ]; then
        die "--mount needs root (sudo, or docker/run.sh --privileged); --rootless does not"
    fi

    local tool
    for tool in mmdebstrap sfdisk mkfs.vfat mformat mcopy mmd grub-mkimage xz sha256sum python3 tar; do
        command -v "$tool" >/dev/null 2>&1 || die "$tool is needed (Debian: mmdebstrap fdisk dosfstools mtools grub-common xz-utils)"
    done
    for tool in mke2fs resize2fs e2fsck; do
        command -v "$tool" >/dev/null 2>&1 || die "$tool is needed (e2fsprogs)"
    done
    local d
    for d in /usr/lib/grub/i386-pc /usr/lib/grub/i386-efi /usr/lib/grub/x86_64-efi; do
        [ -d "$d" ] || die "no $d - the host needs grub-pc-bin, grub-efi-ia32-bin and grub-efi-amd64-bin"
    done
    if [ "$MODE" = mount ]; then
        for tool in losetup mount umount; do
            command -v "$tool" >/dev/null 2>&1 || die "$tool is needed for --mount"
        done
        # grub-bios-setup is in grub-pc (which would grub-install on a real disk at install), not the -bin
        # packages: without it boot.img/core.img are written by hand, which is what grub-bios-setup does anyway
    fi

    WORK_DIR="${WORK_DIR:-$REPO_DIR/build_pc_image}"
    OUT_DIR="${OUT_DIR:-$WORK_DIR}"
    if [ "$DRY_RUN" -eq 0 ]; then
        mkdir -p "$WORK_DIR" "$OUT_DIR"
    fi
    WORK_DIR="$(cd "$WORK_DIR" 2>/dev/null && pwd || echo "$WORK_DIR")"
    OUT_DIR="$(cd "$OUT_DIR" 2>/dev/null && pwd || echo "$OUT_DIR")"

    if [ -z "$VERSION" ] && [ -f "$PACKAGE" ]; then
        VERSION="$(tar -xzOf "$PACKAGE" autobleem-pcusb/VERSION 2>/dev/null | head -1 || true)"
    fi
    [ -n "$VERSION" ] || { warn "no VERSION in the package - naming the image 'dev'"; VERSION=dev; }
    OUT_IMG="$OUT_DIR/autobleem-$VERSION-pcusb-i386.img.xz"
    RAW_IMG="$WORK_DIR/autobleem-$VERSION-pcusb-i386.img"
    ROOT_FS="$WORK_DIR/root.ext4"
    ROOT_TAR="$WORK_DIR/root.tar"
    ESP_IMG="$WORK_DIR/esp.fat"
    GRUB_DIR="$WORK_DIR/grub"

    log "Package: $PACKAGE (version $VERSION)"
    log "Mode: $MODE, Debian $RELEASE i386 from $MIRROR, root $ROOT_SIZE, ESP $ESP_SIZE"
    log "Work: $WORK_DIR  Out: $OUT_IMG"
}

#*******************************
# the files that go into the root: written to a staging dir, copied in by a customize hook
#*******************************
# Everything the base system needs on top of the packages: fstab by label (the stick may be sda on one
# machine and sdb on the next), the hostname, GRUB's defaults and our own menu script, the plymouth theme,
# the AutoBleem package with the first-boot pieces under /opt/autobleem-image (the same five files
# tools/make_rpi_image.sh injects), and the units enabled by their symlinks.
stage_root_files() {
    local s="$WORK_DIR/stage"
    rm -rf "$s"
    mkdir -p "$s/etc/default" "$s/etc/grub.d" "$s/etc/systemd/system/multi-user.target.wants" \
             "$s/opt/autobleem-image" "$s/usr/share/plymouth/themes/autobleem" "$s/etc/modprobe.d"

    cat >"$s/etc/fstab" <<'EOF'
# AutoBleem PC stick: by label - the stick is whatever disk the machine makes it
LABEL=AUTOBLEEM_ROOT  /          ext4  defaults,noatime,errors=remount-ro  0  1
LABEL=ABBOOT          /boot/efi  vfat  defaults,umask=0077                  0  2
EOF
    echo autobleem >"$s/etc/hostname"
    printf '127.0.0.1\tlocalhost\n127.0.1.1\tautobleem\n::1\tlocalhost ip6-localhost ip6-loopback\n' >"$s/etc/hosts"

    # GRUB: what install.sh's configure_boot_pcusb writes on the first boot, so the very first boot already
    # looks right (plymouth, no kernel log), and our menu script in place of 10_linux (below)
    cat >"$s/etc/default/grub" <<'EOF'
# AutoBleem PC stick - install.sh (configure_boot_pcusb) rewrites the keys below on every run
GRUB_DEFAULT=0
GRUB_TIMEOUT=2
GRUB_TIMEOUT_STYLE=hidden
GRUB_DISTRIBUTOR=AutoBleem
GRUB_CMDLINE_LINUX_DEFAULT="consoleblank=0 quiet loglevel=3 logo.nologo vt.global_cursor_default=0 splash plymouth.ignore-serial-consoles"
GRUB_CMDLINE_LINUX=""
GRUB_GFXMODE=auto
GRUB_GFXPAYLOAD_LINUX=keep
GRUB_DISABLE_OS_PROBER=true
EOF
    install -m 0755 "$SCRIPT_DIR/pc_image/10_autobleem" "$s/etc/grub.d/10_autobleem"

    install -m 0644 "$REPO_DIR/payload_linux/system/plymouth/autobleem.plymouth" \
                    "$REPO_DIR/payload_linux/system/plymouth/autobleem.script" \
                    "$REPO_DIR/payload_linux/system/plymouth/splash.png" "$s/usr/share/plymouth/themes/autobleem/"

    install -m 0644 "$PACKAGE" "$s/opt/autobleem-image/$(basename "$PACKAGE")"
    install -m 0755 "$REPO_DIR/payload_linux/system/autobleem-firstboot.sh" "$s/opt/autobleem-image/autobleem-firstboot.sh"
    install -m 0644 "$REPO_DIR/payload_linux/system/autobleem-install-ui.py" "$s/opt/autobleem-image/autobleem-install-ui.py"
    install -m 0644 "$REPO_DIR/payload_linux/system/plymouth/splash.png" "$s/opt/autobleem-image/splash.png"
    install -m 0644 "$REPO_DIR/payload_linux/system/autobleem-firstboot.service" "$s/etc/systemd/system/autobleem-firstboot.service"
    ln -sf ../autobleem-firstboot.service "$s/etc/systemd/system/multi-user.target.wants/autobleem-firstboot.service"
    # ssh from the first boot (the owner's rule: once installed the launcher owns tty1 and the keyboard, so
    # there is no console to enable it from); the host keys are made on the first boot by the first-boot
    # script - the image ships none, every stick would share them otherwise
    ln -sf /lib/systemd/system/ssh.service "$s/etc/systemd/system/multi-user.target.wants/ssh.service"
    # no WiFi country yet: the first boot asks, and keeps its answer here
    : >"$s/etc/modprobe.d/cfg80211.conf"

    # initramfs-tools' own fsck hook reads the type of the *mounted* root, which in the build chroot is the
    # host's - so the ext4 fsck goes in by a hook of ours, and the root is checked at boot as on any Debian
    mkdir -p "$s/etc/initramfs-tools/hooks"
    cat >"$s/etc/initramfs-tools/hooks/autobleem-fsck" <<'EOF'
#!/bin/sh
# AutoBleem PC stick: fsck for the ext4 root in the initramfs (the stock hook could not tell the root's type
# when the image was built in a chroot)
PREREQ=""
prereqs() { echo "$PREREQ"; }
case "$1" in prereqs) prereqs; exit 0 ;; esac
. /usr/share/initramfs-tools/hook-functions
copy_exec /sbin/fsck /sbin
copy_exec /sbin/fsck.ext4 /sbin
copy_exec /sbin/e2fsck /sbin
copy_exec /sbin/logsave /sbin
exit 0
EOF
    chmod 0755 "$s/etc/initramfs-tools/hooks/autobleem-fsck"
}

#*******************************
# build_root
#*******************************
# mmdebstrap makes the Debian root and, in its hooks, finishes it from inside: our staged files copied in,
# the user made, the plymouth theme set and the initramfs of every kernel rebuilt with it (i386 runs on
# this host, so these run natively in the chroot - the one thing the Pi's foreign-arch injection could not
# do), the ssh host keys and the machine-id removed. The result is a tar (rootless: mke2fs turns it into
# the ext4 root below without ever mounting anything) or, with root, the loop-mounted root partition directly.
build_root() {
    local packages="$PACKAGES_COMMON $PACKAGES_KERNELS" arches=i386
    if [ "$NO_AMD64_KERNEL" -eq 0 ]; then
        packages="$packages $PACKAGES_AMD64_KERNEL"
        arches=i386,amd64   # the first is the root's own; amd64 is foreign, for its kernel alone
    fi
    local include
    include="$(echo $packages | tr ' ' ',')"
    local mmmode=unshare
    [ "$MODE" = mount ] && mmmode=root

    stage_root_files
    local finish="$WORK_DIR/finish-root.sh"
    cat >"$finish" <<'EOF'
#!/bin/sh
# runs outside the chroot with its path as $1 (mmdebstrap --customize-hook), as root inside the namespace
set -e
root="$1"; stage="$2"
cp -a "$stage/." "$root/"
chroot "$root" sh -c '
set -e
chmod -x /etc/grub.d/10_linux /etc/grub.d/20_linux_xen /etc/grub.d/30_os-prober /etc/grub.d/30_uefi-firmware 2>/dev/null || true
echo "en_US.UTF-8 UTF-8" > /etc/locale.gen && locale-gen >/dev/null 2>&1 || true
update-locale LANG=en_US.UTF-8 >/dev/null 2>&1 || true
useradd -m -s /bin/bash -G sudo,audio,video,input,plugdev autobleem 2>/dev/null || true
echo "autobleem:autobleem" | chpasswd
echo "root:autobleem" | chpasswd
plymouth-set-default-theme autobleem
update-initramfs -u -k all
# the menu GRUB boots from until the first boot runs update-grub (grub-mkconfig cannot probe a device here)
mkdir -p /boot/grub
{ printf "set timeout=2\nset timeout_style=hidden\nset gfxmode=auto\nset gfxpayload=keep\nif loadfont unicode; then insmod gfxterm; insmod all_video; terminal_output gfxterm; fi\n"
  GRUB_CMDLINE_LINUX_DEFAULT="$(sed -n "s/^GRUB_CMDLINE_LINUX_DEFAULT=\"\(.*\)\"$/\1/p" /etc/default/grub)" \
  GRUB_CMDLINE_LINUX="" sh /etc/grub.d/10_autobleem; } > /boot/grub/grub.cfg
# GRUB modules for the images grub-mkimage makes (they embed what they need; these are for a rescue shell)
for p in i386-pc i386-efi x86_64-efi; do [ -d /usr/lib/grub/$p ] && mkdir -p /boot/grub/$p && cp /usr/lib/grub/$p/*.mod /usr/lib/grub/$p/*.lst /boot/grub/$p/ 2>/dev/null || true; done
cp /usr/share/grub/unicode.pf2 /boot/grub/ 2>/dev/null || true
rm -f /etc/ssh/ssh_host_*
: > /etc/machine-id
rm -f /var/lib/dbus/machine-id
dpkg-divert --local --rename --remove /etc/kernel/postinst.d/zz-update-grub
apt-get clean
'
EOF
    chmod +x "$finish"

    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would run: mmdebstrap --mode=%s --architectures=%s --variant=minbase --include=... %s %s\n' "$mmmode" "$arches" "$RELEASE" "$ROOT_TAR"
        return 0
    fi
    log "Building the Debian $RELEASE i386 root with mmdebstrap (--mode=$mmmode, $(echo $packages | wc -w) packages)"
    rm -f "$ROOT_TAR"
    # the kernel packages' postinst runs update-grub, which cannot probe a device inside a chroot and would
    # fail the whole build: grub2-common's hook is diverted away before the packages go in, and the
    # diversion removed again at the end (finish-root.sh) so a kernel upgrade on the stick does update GRUB
    mmdebstrap --mode="$mmmode" --architectures="$arches" --variant=minbase \
        --components=main,contrib,non-free,non-free-firmware \
        --include="$include" \
        --skip=check/qemu \
        --essential-hook='chroot "$1" dpkg-divert --local --rename --add /etc/kernel/postinst.d/zz-update-grub' \
        --aptopt='Acquire::Languages "none"' \
        --dpkgopt='path-exclude=/usr/share/man/*' --dpkgopt='path-exclude=/usr/share/doc/*/*.gz' \
        --customize-hook="$finish \"\$1\" \"$WORK_DIR/stage\"" \
        --format=tar \
        "$RELEASE" "$ROOT_TAR" "$MIRROR" \
        || die "mmdebstrap failed"
    log "Root tarball: $(du -h "$ROOT_TAR" | cut -f1)"
}

#*******************************
# make_root_fs (rootless)
#*******************************
# ext4 from the tar without mounting: mke2fs -d takes a tar since e2fsprogs 1.47.1 (the Docker image
# builds that; Bookworm's own 1.47.0 takes only a directory - hence the tar is unpacked first where the
# host's e2fsprogs is older, as root inside a fresh user namespace so the owners survive).
make_root_fs() {
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would make %s (ext4, %s) from %s\n' "$ROOT_FS" "$ROOT_SIZE" "$ROOT_TAR"
        return 0
    fi
    log "Making the ext4 root ($ROOT_SIZE) from the tarball"
    rm -f "$ROOT_FS"
    if mke2fs -V 2>&1 | grep -qE 'mke2fs 1\.(4[7-9]\.[1-9]|4[8-9]|[5-9])'; then
        run mke2fs -q -F -t ext4 -L AUTOBLEEM_ROOT -E root_owner=0:0 -d "$ROOT_TAR" "$ROOT_FS" "$ROOT_SIZE"
    else
        # unpack (as root, or inside a user namespace where we are root - the tar's owners are kept either
        # way), then -d the directory
        local dir="$WORK_DIR/rootdir"
        rm -rf "$dir"; mkdir -p "$dir"
        local cmd="tar -xf '$ROOT_TAR' -C '$dir' && mke2fs -q -F -t ext4 -L AUTOBLEEM_ROOT -d '$dir' '$ROOT_FS' '$ROOT_SIZE'"
        if [ "$(id -u)" -eq 0 ]; then
            run sh -c "$cmd" || die "mke2fs -d failed"
        else
            run unshare --map-root-user --map-users=auto --map-groups=auto sh -c "$cmd" \
                || die "mke2fs -d failed (the host's e2fsprogs is older than 1.47.1 and the unpack needed a user namespace)"
        fi
        rm -rf "$dir"
    fi
    run e2fsck -fy "$ROOT_FS" >/dev/null || true
}

#*******************************
# make_esp
#*******************************
# GRUB's EFI images for both firmware widths, each with an early config that finds the root by label and
# hands over to /boot/grub/grub.cfg there, and autobleem.txt - the first boot's options, editable from any PC.
make_grub_images() {
    mkdir -p "$GRUB_DIR"
    cat >"$GRUB_DIR/early.cfg" <<'EOF'
search --no-floppy --label AUTOBLEEM_ROOT --set=root
set prefix=($root)/boot/grub
configfile ($root)/boot/grub/grub.cfg
EOF
    local mods_common="part_msdos part_gpt ext2 fat normal linux search search_label search_fs_uuid cpuid test echo configfile gzio loadenv minicmd sleep"
    log "Making the GRUB images (i386-pc, i386-efi, x86_64-efi)"
    run grub-mkimage -O i386-efi   -p /boot/grub -c "$GRUB_DIR/early.cfg" -o "$GRUB_DIR/BOOTIA32.EFI" \
        $mods_common efi_gop efi_uga all_video gfxterm font
    run grub-mkimage -O x86_64-efi -p /boot/grub -c "$GRUB_DIR/early.cfg" -o "$GRUB_DIR/BOOTX64.EFI" \
        $mods_common efi_gop efi_uga all_video gfxterm font
    # the BIOS core: the prefix is the root partition (the second one), where grub.cfg is
    run grub-mkimage -O i386-pc -p '(hd0,msdos2)/boot/grub' -o "$GRUB_DIR/core.img" \
        biosdisk $mods_common vbe vga all_video gfxterm font
}

make_esp() {
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would make %s (FAT32, %s) with EFI/BOOT/BOOTIA32.EFI, BOOTX64.EFI and autobleem.txt\n' "$ESP_IMG" "$ESP_SIZE"
        return 0
    fi
    make_grub_images
    log "Making the ESP ($ESP_SIZE)"
    rm -f "$ESP_IMG"
    run truncate -s "$ESP_SIZE" "$ESP_IMG"
    run mkfs.vfat -F 32 -n ABBOOT "$ESP_IMG" >/dev/null
    run mmd -i "$ESP_IMG" ::/EFI ::/EFI/BOOT
    run mcopy -i "$ESP_IMG" "$GRUB_DIR/BOOTIA32.EFI" ::/EFI/BOOT/BOOTIA32.EFI
    run mcopy -i "$ESP_IMG" "$GRUB_DIR/BOOTX64.EFI" ::/EFI/BOOT/BOOTX64.EFI
    run mcopy -i "$ESP_IMG" "$REPO_DIR/payload_linux/system/autobleem.txt" ::/autobleem.txt
}

#*******************************
# assemble_image
#*******************************
# The raw image: an MBR (the widest-booting table - every BIOS and UEFI reads it), p1 the ESP at 1 MiB, p2
# the root right after, GRUB's boot.img in the MBR's code area and core.img in the gap before p1.
assemble_image() {
    local esp_bytes root_bytes
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would assemble %s: MBR + ESP + root, GRUB in the MBR gap\n' "$RAW_IMG"
        return 0
    fi
    esp_bytes="$(stat -c%s "$ESP_IMG")"
    root_bytes="$(stat -c%s "$ROOT_FS")"
    local mib=$((1024 * 1024))
    local esp_start=$mib
    local root_start=$(( (esp_start + esp_bytes + mib - 1) / mib * mib ))
    local total=$(( root_start + root_bytes + mib ))
    log "Assembling $RAW_IMG ($(( total / mib )) MiB)"
    rm -f "$RAW_IMG"
    run truncate -s "$total" "$RAW_IMG"
    # sfdisk: type ef (EFI system) and 83 (Linux), sectors of 512
    printf 'label: dos\nunit: sectors\n%s : start=%s, size=%s, type=ef, bootable\n%s : start=%s, size=%s, type=83\n' \
        "${RAW_IMG}1" $((esp_start / 512)) $((esp_bytes / 512)) \
        "${RAW_IMG}2" $((root_start / 512)) $((root_bytes / 512)) \
        | sfdisk -q "$RAW_IMG" >/dev/null
    run dd if="$ESP_IMG" of="$RAW_IMG" bs=1M seek=$((esp_start / mib)) conv=notrunc status=none
    run dd if="$ROOT_FS" of="$RAW_IMG" bs=1M seek=$((root_start / mib)) conv=notrunc status=none
    ESP_OFF=$esp_start
    ROOT_OFF=$root_start
    install_bios_grub
}

# boot.img's 440 code bytes into the MBR (the partition table and the disk signature after them are
# kept), core.img from sector 1. boot.img as shipped already says "the kernel is at sector 1" and "the
# boot drive is whatever the BIOS says" (kernel_sector = 1, boot_drive = 0xff), and grub-mkimage's
# diskboot blocklist already says "the rest starts at sector 2", so nothing needs patching - what
# grub-bios-setup would do for us on a loop device, and does with --mount.
install_bios_grub() {
    local boot_img=/usr/lib/grub/i386-pc/boot.img
    if [ "$MODE" = mount ] && command -v grub-bios-setup >/dev/null 2>&1; then
        log "Installing the BIOS GRUB with grub-bios-setup on a loop device"
        local loop
        loop="$(losetup -fP --show "$RAW_IMG")"
        printf '(hd0) %s\n' "$loop" >"$GRUB_DIR/device.map"
        if grub-bios-setup -d "$GRUB_DIR" -b "$boot_img" -c core.img -m "$GRUB_DIR/device.map" "$loop"; then
            losetup -d "$loop"
            return 0
        fi
        warn "grub-bios-setup failed - writing boot.img and core.img by hand"
        losetup -d "$loop"
    fi
    log "Installing the BIOS GRUB (boot.img into the MBR, core.img at sector 1)"
    local core_sectors=$(( ( $(stat -c%s "$GRUB_DIR/core.img") + 511 ) / 512 ))
    [ "$core_sectors" -lt 2047 ] || die "core.img is $core_sectors sectors - it does not fit the 1 MiB gap"
    run dd if="$boot_img" of="$RAW_IMG" bs=1 count=440 conv=notrunc status=none
    run dd if="$GRUB_DIR/core.img" of="$RAW_IMG" bs=512 seek=1 conv=notrunc status=none
}

#*******************************
# verify_image
#*******************************
# what a boot will find, without booting: the partition table, the ESP's files, the root's kernels and the
# five injected files
verify_image() {
    [ "$DRY_RUN" -eq 0 ] || return 0
    log "Checking the image"
    sfdisk -l "$RAW_IMG" | sed 's/^/    /'
    mdir -i "$RAW_IMG@@$ESP_OFF" ::/EFI/BOOT | grep -E 'BOOT' | sed 's/^/    ESP: /'
    local fs="$RAW_IMG?offset=$ROOT_OFF"
    debugfs -R "ls -l /boot" "$fs" 2>/dev/null | grep -E 'vmlinuz|initrd' | sed 's/^/    /'
    debugfs -R "ls -l /opt/autobleem-image" "$fs" 2>/dev/null | grep -v '^debugfs' | sed 's/^/    /'
    local want got
    want="$(stat -c %s "$PACKAGE")"
    got="$(debugfs -R "stat /opt/autobleem-image/$(basename "$PACKAGE")" "$fs" 2>/dev/null | sed -n 's/.*Size: \([0-9]*\).*/\1/p' | head -1)"
    [ "$want" = "$got" ] || die "the package inside the image is ${got:-missing} bytes, the file is $want"
    debugfs -R "cat /boot/grub/grub.cfg" "$fs" 2>/dev/null | grep -c menuentry | sed 's/^/    menu entries: /'
}

#*******************************
# finalize_image
#*******************************
finalize_image() {
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    would compress %s -> %s and hash it\n' "$RAW_IMG" "$OUT_IMG"
        return 0
    fi
    log "Compressing to $OUT_IMG (xz -T0 -$XZ_LEVEL)"
    rm -f "$OUT_IMG"
    if ! xz -T0 "-$XZ_LEVEL" -c "$RAW_IMG" >"$OUT_IMG"; then
        rm -f "$OUT_IMG"
        die "xz failed writing $OUT_IMG (out of space?) - the raw image is kept in $WORK_DIR"
    fi
    (cd "$OUT_DIR" && sha256sum "$(basename "$OUT_IMG")" >"$(basename "$OUT_IMG").sha256")
    [ "$KEEP_RAW" -eq 1 ] || rm -f "$RAW_IMG"
    rm -f "$ROOT_FS" "$ESP_IMG"
}

summary() {
    log "Done."
    if [ "$DRY_RUN" -eq 1 ]; then
        warn "this was a --dry-run: nothing above actually happened"
        return 0
    fi
    cat <<EOF

  Image:  $OUT_IMG ($(du -h "$OUT_IMG" | cut -f1))
  Write it to a stick of 8 GB or more with Rufus ("DD Image" mode when asked), balenaEtcher, or
      xzcat $(basename "$OUT_IMG") | sudo dd of=/dev/sdX bs=4M status=progress
  Boot the PC from the stick (BIOS or UEFI, Secure Boot off). The first boot sets AutoBleem up on the
  screen; autobleem.txt on the stick's first partition holds its options.
  Publish: tools/repo_publish.sh pc-image $VERSION $OUT_IMG

EOF
}

#*******************************
# main
#*******************************
main() {
    parse_args "$@"
    preflight
    build_root
    make_root_fs
    make_esp
    assemble_image
    verify_image
    finalize_image
    summary
}

main "$@"
