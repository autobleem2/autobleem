# Pi: prebuilt flashable image (Raspberry Pi Imager) + fast in-place updates

Parked here from `docs/IDEAS.md` once it had enough research to write down as a real plan, but before being
picked up for implementation - same role `docs/retroarch-scanner-plan.md` served for that feature. Remove
this file (per CLAUDE.md's "finished plans leave docs/" rule) once implemented, or once superseded by an
updated plan.

## Context

Today the Raspberry Pi port ships only as `payload_rpi/` + `install.sh`, run by hand over ssh on top of a
stock Raspberry Pi OS Lite install (`tools/make_rpi_package.sh` -> `autobleem-rpi(.tar.gz|-arm64.tar.gz)`).
That is a good dev/test path but a poor "give this to someone else" path: they still need to flash Lite
themselves, get ssh working, copy the tarball up and run `sudo bash install.sh`. The owner wants a second
distribution form - a prebuilt disk image that Raspberry Pi Imager can flash directly, with Imager doing what
it's good at (hostname, user/password, WiFi, SSH, locale) and AutoBleem's own setup completing itself
afterwards on first or second boot. The tarball+install.sh path stays as-is for anyone who prefers it, and
gets one more capability while we're in this code: `install.sh` needs to recognize a Pi it already installed
and refresh it in place, fast, without repeating slow steps (RetroArch source build, core/thumbnail/BIOS
downloads, repartitioning) it already did. A second, simpler update path - drop a tarball onto the exFAT
partition from a PC - covers a user with no ssh access.

Per the owner's decisions: the image is built by injection only for this round (stock Lite + the AutoBleem
package + a first-boot service that runs `install.sh`; no pre-installed packages/RetroArch inside the image
yet - that can be a later round once the injection path is proven). The image is built natively on the Pi 400
(root Linux with real ARM, both armhf and arm64 loop-mountable - no qemu/chroot needed for injection-only).
Updating an already-installed Pi has two entry points: (1) `install.sh` itself, run again, detects the
existing install and takes a fast path; (2) a tarball dropped into a well-known folder on the exFAT partition
from Windows/macOS/Linux, applied automatically by `autobleem-session.sh` at the next boot before the
launcher starts, with progress shown on tty1.

## Part 1 - Fast update, both entry points

### Shared piece: a diff-copy helper in `install.sh`

Add `apply_payload_diff(src_tree, dest_tree)` to `payload_rpi/install.sh`: walks the files under `src_tree`
(the staged/extracted package - `Autobleem/`, `themes/`, plus `Autobleem/rc/*.sh`), and for each one compares
`sha256sum` against the same relative path under `dest_tree`; copies only when missing or different. This
replaces the current unconditional `cp -r "$STAGE_DIR/$d/." "$DATA_MOUNT/$d/"` loop in `install_payload()`
for an update run - full copy on a fresh install (nothing exists yet, so the hash compare is a no-op that
falls through to "copy everything" the first time), incremental after that. `config.ini` keeps its existing
special-case (copy-aside/restore) untouched. `Games/`/`Apps/`/`RetroArch/roms` etc. are never touched by
this - the diff only ever runs over the AutoBleem/themes/rc payload, never anything the user's own data
lives in.

### Entry point 1: `install.sh` detects an existing install

In `main()`, right after `preflight`, add `detect_existing_install()`: if `$DATA_MOUNT/Autobleem/bin/autobleem/autobleem-gui`
already exists (found via the existing/discovered data partition - reuse `existing_data_partition()` earlier
than today, before deciding whether to partition), set `UPDATE_MODE=1` and skip straight to a shortened
sequence: `mount_data` (already-mounted is a no-op) -> `apply_payload_diff` over `Autobleem/`, `themes/`,
`Autobleem/rc/` -> `install_service` (re-writes the unit/session script only if they differ - same diff
helper) -> restart `autobleem.service` if it was running -> `summary`. Skip `install_packages`,
`ensure_data_partition`/`create_data_partition`, `install_retroarch`, `download_retroarch_content`,
`download_bios_pack`, `install_boot_splash`, `configure_boot` by default in this mode - those are exactly
the slow/destructive/already-done steps. Add explicit opt-in flags for anyone who does want to force one of
them on an update: `--update` (forces update mode even if autodetection is ambiguous), `--refresh-retroarch`,
`--refresh-bios`, `--refresh-boot-config` (each just un-skips that one call). Document in `usage()` and
`README.md`. This directly replaces the ad-hoc `--retroarch none --no-downloads --yes` incantation that
`tools/make_rpi_package.sh --push`'s own usage text already prints today (`tools/make_rpi_package.sh:126-133`)
- once `install.sh` self-detects, that block simplifies to a plain `sudo bash install.sh --yes`.

### Entry point 2: tarball dropped on the exFAT partition

New folder in the payload layout: `Autobleem/update/` (an empty dir kept by a `placeholder` file, like
`Games/`/`Apps/` are today). A user copies `autobleem-rpi.tar.gz` (or `-arm64`) there from a PC.

`payload_rpi/system/autobleem-session.sh` gains an `apply_pending_update()` step, called once right after
`boot_splash_down` and before the `while true` launcher loop (so it runs once per boot, not once per
launcher-restart-after-RetroArch cycle):
- Look for exactly one `*.tar.gz` under `$DATA_MOUNT/Autobleem/update/`. None -> return immediately (the
  common case, no cost).
- Print progress straight to `/dev/tty1` (the service's `StandardOutput` is `journal`, which is invisible
  before the SDL launcher owns the screen, so this step writes to the console device directly - the same
  place the kernel/plymouth messages were, consoleblank is off): "Applying AutoBleem update: <name>...".
- Extract to `$DATA_MOUNT/Autobleem/update/.extract` (same filesystem - no cross-device copy).
- Verify architecture matches (`uname -m` vs. the extracted `Autobleem/bin/emu/pcsx-ab`'s ELF class, or
  simpler: the tarball name itself, `-arm64` suffix vs `dpkg --print-architecture`); mismatch -> log a
  warning to tty1, move the tarball to `.rejected` and continue booting normally rather than bricking the
  boot loop.
- Call the same diff-copy logic `install.sh` uses (factor it into `payload_rpi/system/lib/payload_diff.sh`,
  sourced by both `install.sh` and `autobleem-session.sh`, so there is exactly one implementation) over
  `.extract/Autobleem` -> `$DATA_MOUNT/Autobleem`, `.extract/themes` -> `$DATA_MOUNT/themes`. Preserve
  `config.ini` the same way `install_payload()` does.
- Rename the tarball to `applied-<FULL_VERSION or timestamp>.tar.gz.done` (kept, not deleted - a visible
  history/undo point) and clean up `.extract`.
- Print "Update applied, starting AutoBleem..." to tty1, then fall into the existing loop unchanged.
- Any failure at any step (bad tar, disk full, hash mismatch after copy) logs to tty1 and to
  `$LOG_DIR/update.log`, cleans up `.extract`, and **falls through to starting the launcher anyway** with
  the old files untouched - an update must never be the reason the appliance fails to boot.

`payload_rpi/README.md` gets a short "Updating" section covering both paths (ssh re-run of `install.sh`,
and drag a new tarball into `Autobleem/update/` and reboot).

### Why this is "fastest possible" without adding new infrastructure

sha256-diffing ~200 small files and copying only the changed ones (typically just the binary + one or two
resource files after a code change) is the cheap, dependency-free version of an rsync delta - no rsync
package to add to the Pi image, no extra network service, and it is identical logic whether the source is a
freshly extracted tarball (update-from-partition path) or the staged tree over ssh (`install.sh` re-run
path), so one implementation covers both entry points the owner asked for.

## Part 2 - Prebuilt image for Raspberry Pi Imager

### Format Imager needs

Raspberry Pi Imager's "Use custom" only accepts a plain `.img` (optionally `.img.xz`/`.img.zip`/`.img.gz`)
with **no customisation offered** unless it's given metadata via a local JSON pointed at with `--repo` (or,
simpler for us: publish our own small `os_list` JSON - `name`, `description`, `url`, `image_download_size`,
`image_download_sha256`, `extract_size`, `extract_sha256`, `init_format`, `devices` - that a user loads once
via Imager's "Use custom" -> local `.json`, or that we host and point Imager's `--repo` flag at). Modern Lite
images (Trixie) already ship `init_format: cloudinit-rpi`; since our image *starts from* an official Lite
image and we don't touch its first-boot customisation files, Imager's normal hostname/user/WiFi/SSH/locale
UI keeps working unmodified - we inherit `init_format` from whichever base image we start from and pass it
through unchanged in our own JSON. This means our first-boot service must tolerate running *after* cloud-init
(or firstrun.sh) has done its thing, on whichever boot that lands on - hence "first or second boot", handled
by re-arming (see below) rather than by racing it.

### Build host and inputs

Built natively on the Pi 400 (`ssh psc-build` pattern already exists for a different remote; this is a new,
separate helper - the Pi has no cross-toolchain concerns here, it's just image manipulation, not compiling).
New script: `tools/make_rpi_image.sh` (run *on* the Pi over the existing ssh access - the plink/sudo -S
recipe from CLAUDE.md), taking `--base <official Lite .img.xz URL or local path>`, `--arch armhf|arm64`,
`--package <path to autobleem-rpi*.tar.gz>` (built beforehand with the existing `tools/make_rpi_package.sh`
and copied over, exactly as today's manual flow does).

Steps:
1. Download (or reuse a cached) official Raspberry Pi OS Lite `.img.xz` for the matching architecture,
   verify its published sha256, `xz -d` it (or decompress to a sparse file to save the Pi's SD card space).
2. `losetup -fP` the `.img` to get partition device nodes for its boot (FAT) and root (ext4) partitions -
   both are plain filesystems Pi OS Lite ships as-is, no qemu/chroot required since we are only *injecting
   files*, never executing anything inside the image at build time. Mount root at a scratch dir.
3. Copy the package tarball into a fixed path on the root filesystem, e.g.
   `/opt/autobleem-image/autobleem-rpi.tar.gz` (chosen over the FAT boot partition - Lite's boot partition is
   small and shared with kernel/firmware; the injected tree here is small, ~30-50 MB, since the package's
   `RetroArch/` entry is just empty standard subfolders - no cores/BIOS/thumbnails are bundled, those still
   come down on first boot exactly as they do today).
4. Install a new systemd oneshot unit, `autobleem-firstboot.service` (`payload_rpi/system/autobleem-firstboot.service`
   + a small runner script `autobleem-firstboot.sh`, both new files checked into the repo and copied in by
   the build script - not generated ad hoc), `WantedBy=multi-user.target`, ordered `After=multi-user.target`
   so it runs once other boot-time setup (including cloud-init/firstrun) has had its shot. The script:
   - Checks a marker (`/opt/autobleem-image/.done`). Present -> disable itself
     (`systemctl disable autobleem-firstboot`) and exit - this is the re-arm-until-it-works logic that makes
     "runs on the first or second boot" safe: if network/cloud-init isn't ready yet on boot 1, the unit
     simply runs again on boot 2 (still `WantedBy=multi-user.target`, nothing disables it until it succeeds).
   - Runs `bash /opt/autobleem-image/autobleem-rpi/install.sh --yes` (the tarball is unpacked once, lazily,
     into `/opt/autobleem-image/autobleem-rpi/` the first time this script runs) with the *default* options
     (full first-install: packages, RetroArch source build, downloads, BIOS pack, boot splash) - this is the
     one case where the slow first-run path is correct and expected, same as today's manual flow.
   - On success: touch `.done`, `systemctl disable autobleem-firstboot`, remove the now-redundant
     `/opt/autobleem-image/autobleem-rpi.tar.gz` and the unpacked tree (install.sh already copied everything
     it needs onto the exFAT data partition), `reboot` once so the machine comes up clean into the
     `autobleem.service` tty1 session with the final `cmdline.txt`/plymouth boot splash active (some of
     `install.sh`'s effects, like the HDMI mode and boot splash, only take visible effect on the *next*
     boot).
   - On failure (e.g. no network yet): log to the journal and simply return non-zero without touching the
     marker, so the next boot retries; cap retries with a counter file so a permanently offline Pi doesn't
     loop forever - after N attempts, disable itself and leave a note in the journal + `System/Logs` once
     the data partition exists telling the owner to run `install.sh` by hand.
5. `mount_data`/root unmount, `losetup -d`, re-compress the modified image with `xz -T0` (Pi 400 is 4 cores -
   confirm at implementation time; falls back to `-T1` if `nproc` is 1-2) to
   `autobleem-rpi-image-<arch>.img.xz`, and print/compute the `sha256`/size fields needed for the Imager JSON.
6. Emit `tools/rpi_imager_repo.json` (checked-in template, sizes/hashes filled in by the script) alongside
   the image, ready to hand to Imager's "Use custom" with a local JSON, or to host and reference by URL
   later.

### What the first real boot changed (2026-09-19)

The first image was flashed plain ("Use custom", no presets) onto the Pi 400 and booted. Three things the
plan above had not accounted for, all now in the implementation:

1. **No network on first boot is the normal case, not an edge case.** With no Imager presets, Raspberry Pi
   OS's own wizard asks for a keyboard layout and a user; WiFi stays `rfkill`-blocked because no country was
   set. `autobleem-firstboot` then ran `install.sh --yes` in the background with no network, `apt-get
   install` failed (`Temporary failure resolving 'deb.debian.org'`), and the screen showed a login prompt
   with no hint of any of it. Now the service owns its own console, tty8 (`StandardInput/Output=tty`,
   `TTYPath=/dev/tty8`, `chvt 8` on start and `chvt 1` on failure - the first attempt, tty1 with
   `Conflicts=getty@tty1.service`, never ran: a `Wants=`-pulled unit conflicting with another unit in the
   same boot transaction gets its job dropped), so the whole first boot is on the screen, and the script asks for WiFi when
   there is none: country (needed to unblock rfkill), an `nmcli` scan, pick/hidden/Ethernet/skip, password,
   connect, a real fetch check, then an NTP wait before `apt`. Preset WiFi is *not* an AutoBleem option -
   Imager's customisation screen and the boot partition's own cloud-init `network-config` already are that.
2. **The root grows over the whole card on the stock first boot**, which leaves nothing for the data
   partition. On Trixie the grow is done by the initramfs when `cmdline.txt` contains the word `resize`
   (`local-premount/resize_early`; `set_partuuid` next to it also keys on it); `make_rpi_image.sh` removes
   that word, and `install.sh` gained `--grow-root GIB` (sfdisk `-N` + `partx -u` + online `resize2fs`,
   capped to leave room for the data partition, run before `apt` because a fresh Lite root has ~400 MB
   free) to grow the root to a bounded size instead. `autobleem.txt` on the boot partition carries
   `root_gib=8` and the other install options.
3. **Imager's customisation screen needs metadata for a local file.** `tools/rpi_imager_local_manifest.py`
   writes the `*.rpi-imager-manifest` (`file://` URLs, the same shape as Imager's own
   `create_local_json.py` output) from the build's `rpi_imager_repo.json`, so a locally built image gets
   the user/WiFi/SSH screen.

Still to verify on hardware after this: the interactive WiFi prompt end to end, a first boot with Imager
presets through the local manifest, `--grow-root` on a real card, and the install-then-reboot handoff.

### What stays out of scope for this round (per the "injection only" choice)

No package pre-install, no RetroArch pre-built into the image, no qemu/chroot - first boot after the
firstboot service still does the full `install.sh` work over the network, same total time as today's manual
flow, just triggered automatically instead of typed by hand. This is a deliberate stepping stone: once
proven, a later round can move to a chroot-based build (on WSL2 or the Pi, since the Pi's own kernel is
64-bit and can already loop-mount+chroot an arm64 image; a chroot into an armhf image from the Pi's aarch64
kernel needs `qemu-arm-static`/binfmt, unlike injection-only) to pre-install apt packages and cut first-boot
time.

## Files touched / added

- `payload_rpi/install.sh` - `detect_existing_install()`, `apply_payload_diff()` (or sourced from the new
  shared lib), update-mode branch in `main()`, new flags, `usage()`/README updates.
- `payload_rpi/system/lib/payload_diff.sh` - new, the shared diff-copy function.
- `payload_rpi/system/autobleem-session.sh` - `apply_pending_update()`.
- `payload_rpi/Autobleem/update/placeholder` - new empty dir.
- `payload_rpi/system/autobleem-firstboot.service`, `payload_rpi/system/autobleem-firstboot.sh` - new.
- `tools/make_rpi_package.sh` - simplify the printed re-install usage block now that `install.sh` self-detects.
- `tools/make_rpi_image.sh` - new, run on the Pi 400.
- `tools/rpi_imager_repo.json` - new template.
- `payload_rpi/README.md` - "Updating" section, "Flashing with Raspberry Pi Imager" section.
- `CLAUDE.md` - new dated entries under the Raspberry Pi port section once implemented (per existing
  convention in this file).

## Verification

- Update, entry point 1: on the Pi 400 (already installed per the Pi test setup notes), change one file
  (e.g. touch a theme asset or bump `Version`), rebuild+repackage, `scp` the tarball up, run
  `sudo bash install.sh --yes` and confirm from the log/timing that only the changed file(s) were copied and
  the service restarted, games/config/RetroArch content untouched.
- Update, entry point 2: drop a tarball into `Autobleem/update/` from a Windows share mount of the exFAT
  partition, reboot, confirm tty1 shows the "Applying AutoBleem update" progress before the launcher's own
  splash, and the tarball is renamed to `applied-*.done`.
- Failure injection: a corrupt/truncated tarball in `Autobleem/update/` should log and still boot normally.
- Image build: run `tools/make_rpi_image.sh` on the Pi 400 for armhf against a fresh official Lite `.img.xz`,
  flash the result with Raspberry Pi Imager onto a spare card (with the local-JSON custom-image path),
  optionally set a hostname/WiFi/SSH via Imager's own dialog, boot on real hardware, and confirm: Imager's
  customisation took effect (hostname/WiFi), `autobleem-firstboot` ran and disabled itself, the machine
  rebooted once and came up as the tty1 AutoBleem appliance with the boot splash, and `.done`/log files
  reflect a clean single first-boot run (or a documented one-retry case if network wasn't up immediately).
