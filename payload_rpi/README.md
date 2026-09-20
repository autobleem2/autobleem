# AutoBleem on a Raspberry Pi

Turns a Raspberry Pi running **Raspberry Pi OS Lite**, 32-bit (armhf) or 64-bit (arm64), into an AutoBleem
machine: it boots straight into the launcher with no desktop, and its games live on an exFAT partition you
can plug into a Windows, Mac or Linux machine and drop games onto — the same way the PlayStation Classic's
USB stick works. The two architectures build and install the same way; pick the tarball for the OS you
flashed (`autobleem-rpi.tar.gz` for 32-bit, `autobleem-rpi-arm64.tar.gz` for 64-bit) - `install.sh` detects
which it is running on and downloads the matching RetroArch cores.

Starting from a blank SD card? `docs/pi-install-guide.md` walks through flashing the OS, first boot and
running the installer, start to finish. This page is the fuller reference once you're there.

## Status

This is a port in progress. Be aware of what is and is not here:

| | |
|---|---|
| Launcher, scanner, themes, memory cards, covers | built and packaged |
| PS1 games | run in **pcsx-ab**, the console's own emulator, built for the Pi (`Autobleem/bin/emu/`, with the `gpu_peops`/`gpu_unai` plugins) — save states, memory cards and "Resume" work the way they do on the console |
| BIOS | **downloaded by the installer**: a ~190 MB (armhf) or ~230 MB (arm64) pack (`system/biospack.txt` / `biospack-arm64.txt`, built from [RetroBIOS](https://github.com/Abdess/retrobios) by `tools/biospack.py --arch armhf\|arm64`, one manifest per architecture's actual core list) into `RetroArch/system/` — every system with a `roms/` folder (consoles, handhelds, the Amiga/C64/MSX/Spectrum/PC-98/X68000 computers), arcade, Neo Geo CD, ScummVM, Doom — and the PS1 BIOS (SCPH-5501/5500) copied to `System/Bios/romw.bin` + `romJP.bin` for pcsx-ab, unless you have already put your own there. `--no-bios` skips it; without a `romw.bin` pcsx-ab runs on its HLE BIOS, which many games tolerate and some do not |
| RetroArch | **optional** - the image's first boot asks, `install.sh --retroarch none` is a PS1-only AutoBleem (the launcher hides its RetroArch set and menu items when none is installed). When wanted: the **latest release, prebuilt for the Pi by AutoBleem's download site** (`--retroarch prebuilt`, the default - libretro's buildbot has every core for armhf and arm64 but no frontend build; built from source by the installer when the site cannot be reached), with all ~130 cores, core info, menu assets, pad autoconfigs and scanner databases downloaded into `RetroArch/` on the data partition. AutoBleem's RetroArch set shows the playlists RetroArch's scanner writes there; "Play using RA" runs a PS1 game in `pcsx_rearmed` |
| Internal games | gone, by design — a Pi has no built-in game list, so the set and its option are compiled out |
| Tested on real hardware | **yes**, on a Pi 400 (32-bit) since 2026-09-18: boots, scans, plays PS1 games and the other-systems RetroArch set, sound over HDMI. The 64-bit build compiles and packages cleanly but has not yet been run on a 64-bit Pi OS card. |
| Flashing with Raspberry Pi Imager | `tools/make_rpi_image.sh` builds a flashable `.img.xz` whose first boot installs AutoBleem on the screen, asking for WiFi if it has none - see "Flashing with Raspberry Pi Imager" below. The build is verified on the Pi 400 for both architectures and the first image has booted once (which is what shaped the current first-boot flow); **the reworked first boot has not yet been through a fresh card end to end** - the manual tarball + `install.sh` flow above is the proven path. |

## What you need

- **32-bit**: a Raspberry Pi 2, 3, 4 or Zero 2 W. The build targets `armv7-a` with NEON, so the original Pi 1
  and Zero (armv6) are **not** supported. Flash **32-bit** Raspberry Pi OS Lite; the binary is
  `arm-linux-gnueabihf`.
- **64-bit**: a Raspberry Pi 3, 4, 5, 400 or Zero 2 W. The build targets `armv8-a`. Flash **64-bit**
  Raspberry Pi OS Lite; the binary is `aarch64-linux-gnu`. pcsx-ab has no aarch64 dynarec in this fork, so PS1
  games run through its C interpreter on 64-bit — correct, but slower per clock than the 32-bit build's NEON
  dynarec.
- Either way: the installer checks `dpkg --print-architecture` against the package and refuses a mismatch.
- An SD card with room for your games, a keyboard for the first login, and a USB gamepad.

## Build the package (on the PC)

```bash
./make_rpi.sh                          # 32-bit: cross-compiles with toolchains/rpi/RPitoolchain.cmake
./tools/make_rpi_package.sh --arch armhf     # -> build_rpi/autobleem-rpi.tar.gz

./make_rpi64.sh                        # 64-bit: cross-compiles with toolchains/rpi64/RPi64toolchain.cmake
./tools/make_rpi_package.sh --arch arm64     # -> build_rpi64/autobleem-rpi-arm64.tar.gz
```

pcsx-ab is checked in under `payload_rpi/Autobleem/bin/emu/` (32-bit) and `payload_rpi/Autobleem/bin/emu-arm64/`
(64-bit) and rides along. To refresh it from a new build, run pcsx-rearmed-develop's
`AUTOBLEEM_DIR=../autobleem-develop ./make_rpi.sh` (32-bit) or `./make_rpi64.sh` (64-bit), which copies its
`build_rpi/dist/` or `build_rpi64/dist/` there.

Copy the tarball for your Pi's architecture over (`scp`, or just put it on a USB stick).

Or build both on the build server's Docker image - `docker/run.sh ci/build.sh rpi rpi64`, see `docs/ci.md` -
which also builds pcsx-ab from the same run and includes the real cover databases (a package built on a PC
checkout has only the stubs in `db/`, so scanned games get no covers from that fallback).

## Install (on the Pi)

```bash
tar xzf autobleem-rpi.tar.gz
cd autobleem-rpi
sudo bash install.sh --dry-run   # prints every change it would make, changes nothing
sudo bash install.sh
sudo reboot
```

(`bash install.sh` rather than `./install.sh` because a package built on a Windows host loses the executable
bit on the way into the tarball.)

The installer: installs SDL2, libpng, exfatprogs and parted; installs the latest RetroArch release - the
build AutoBleem's download site has for this architecture, or from source when the site cannot be reached
(10-40 minutes depending on the Pi; `--retroarch source` asks for that outright, `--retroarch apt` takes
the distribution's package instead, `--retroarch none` skips it); finds or creates the exFAT data partition; builds the AutoBleem and RetroArch
trees on it; downloads every core for the Pi's architecture plus RetroArch's info/assets/autoconfig/database bundles from
`buildbot.libretro.com` (a few hundred MB; `--no-downloads` skips it, RetroArch's Online Updater can do it
later); downloads the BIOS pack (~190 MB, file by file with a SHA-256 check, only what is missing -
`--no-bios` skips it); installs the launcher, pcsx-ab, themes and launch scripts; wires up a systemd service that owns
tty1; and sets up a quiet boot at 1080p with the AutoBleem logo on screen (plymouth) instead of the kernel
log. It works on Raspberry Pi OS Bookworm and Trixie (Trixie renamed some packages for its 64-bit `time_t`
transition; the installer tries both names).

pcsx-ab reads its BIOS from `System/Bios/` on the data partition — `romw.bin`, and `romJP.bin` for Japanese
games. The installer fills both from the pack (SCPH-5501 and SCPH-5500); to use your own, put them there
before running it, or replace them afterwards — a re-run never overwrites them.

`--help` lists the options. The useful ones are `--shrink-root` / `--grow-root` (see "The data partition"),
`--retroarch`, `--repo` (AutoBleem's download site, for a mirror or a copy on your own network),
`--no-downloads`, `--no-bios`, `--no-packages`, `--stage`, `--hdmi-mode` (default
`1920x1080@60`, `1280x720@60` for a 720p screen; `none` keeps the screen's preferred mode), `--no-boot-splash`
and `--no-quiet-boot`.

## Flashing with Raspberry Pi Imager

A second way to get to the same place as the tarball + `install.sh` flow above: flash one card, boot it,
answer at most two questions (which WiFi; RetroArch or PS1-only), and watch AutoBleem install itself on the screen. The manual flow
above remains the fully proven path - see the "Status" table for how far this one has been exercised.

**The ready-made images are on AutoBleem's download site.** In Raspberry Pi Imager: *App Options*
(the gear, or Ctrl+Shift+X) *-> Content Repository -> Use custom* and enter

    https://autobleem.retromenele.pl/rpi-imager/os_list.json

(or start it as `rpi-imager --repo <that URL>`); AutoBleem then appears in the *Operating System* list
for both 32-bit and 64-bit, with Imager's own customisation screen (user, WiFi, SSH) on top. Or download
an `.img.xz` from <https://autobleem.retromenele.pl/> and flash it with *Use custom*. The site's
[Raspberry Pi page](https://autobleem.retromenele.pl/rpi-install.html) says which image fits which Pi.
What follows is how those images are built.

`tools/make_rpi_image.sh` takes an official Raspberry Pi OS Lite image (downloaded automatically, or your
own with `--base`) and injects an AutoBleem package plus a first-boot service. The only other changes to the
base image are on its boot partition: `cmdline.txt` loses the word `resize` (see "The data partition" -
this is what keeps the root from swallowing the whole card), and `autobleem.txt` is added. cloud-init's
`user-data`/`network-config` stay exactly as the base image ships them, so Raspberry Pi Imager's own OS
customisation lands on top of them as on a stock image.

```bash
./make_rpi.sh   && ./tools/make_rpi_package.sh --arch armhf     # -> build_rpi/autobleem-rpi.tar.gz
./make_rpi64.sh && ./tools/make_rpi_package.sh --arch arm64     # -> build_rpi64/autobleem-rpi-arm64.tar.gz

# on a Linux host with root (losetup/mount) - the two tarballs above, copied over, are the only inputs:
sudo ./tools/make_rpi_image.sh --arch armhf --package /path/to/autobleem-rpi.tar.gz
sudo ./tools/make_rpi_image.sh --arch arm64 --package /path/to/autobleem-rpi-arm64.tar.gz
```

Each run downloads that architecture's current "latest" Raspberry Pi OS Lite image (sha256-verified against
its published checksum), loop-mounts it, drops the package into `/opt/autobleem-image/` on its root
filesystem alongside `autobleem-firstboot.service` (enabled by hand-crafting the same symlink `systemctl
enable` would - no chroot, no qemu, nothing from the base image is ever executed at build time), edits the
boot partition as above, and recompresses it to `<out>/autobleem-<version>-rpi-<arch>.img.xz` - the version is
the package's `VERSION` file, which `tools/make_rpi_package.sh` writes from the build's own `version.h`
(`v2.0.0` for a clean tree at that tag, `v2.0.0-pre0-ad109aa` otherwise; `--version` overrides). `--dry-run`
prints what it would do without downloading, mounting or needing root; `--help` lists every option
(`--base`, `--work`, `--out`, `--keep-raw`, `--version`).

### What the first boot does

The base image boots as usual: cloud-init applies whatever Raspberry Pi Imager was told (user, hostname,
WiFi, SSH), or - with no presets - Raspberry Pi OS asks for a keyboard layout and a user on the screen. Then
`autobleem-firstboot.service` takes over the screen and keyboard (it switches to its own console, tty8) and:

1. waits for the network. **No network?** It asks: it lists the WiFi networks it can see, you pick one and
   type the password (or type a hidden network's name, or plug in an Ethernet cable and press `e`, or `s`
   to skip - it asks again on the next boot). It also sets the WiFi country first, because Raspberry Pi OS
   keeps WiFi blocked (`rfkill`) until one is set.
2. waits for the clock to sync (NTP) - `apt` distrusts a clock that is days off, and a Pi has no battery clock.
3. **asks whether to install RetroArch** (unless `autobleem.txt` already says). AutoBleem plays PS1 games on
   its own; RetroArch adds the other systems at the cost of close to a GB of downloads (the RetroArch
   build itself comes from AutoBleem's download site in seconds; only without it is it built here, in
   10-40 minutes). `n` gives a lean PS1-only install: no RetroArch, no cores, only the two PS1 BIOS files out of the
   pack; the launcher hides its RetroArch set and menu items when no RetroArch is
   installed. No answer within a minute means yes, so a Pi set up entirely from Imager's presets and left
   alone gets the full install. RetroArch can be added later by running `install.sh` again.
4. from here on the screen is graphical - the AutoBleem logo, a bar for the step (of nine), a bar for the
   download in progress and a box with the installer's last lines (`system/autobleem-install-ui.py`,
   drawing on the framebuffer: nothing but python3 and the console fonts exist at that point). It
   grows the root partition first (`install.sh --grow-root <root_gib> --grow-only`: from the base image's
   ~3 GB to `root_gib`, the rest of the card becomes the `AUTOBLEEM` partition) - the package unpacks to
   over 300 MB and a fresh root has less free than that - then unpacks the package and runs
   `install.sh --yes` with the options from `autobleem.txt` (below), with its whole output on the
   screen: packages, RetroArch if wanted, cores, BIOS.
   Box art is not mirrored: the launcher fetches each game's cover when it scans it (see below). The same
   output is kept in `/var/log/autobleem-firstboot-install.log` for reading over ssh.
5. on success: deletes the staged package, disables itself and reboots once more - the boot splash and HDMI
   mode only take full effect on the boot after `install.sh` sets them. On failure it says so, gives the
   login prompt back, and tries again on the next boot (up to 20 times, then it gives up and leaves a note).

`journalctl -u autobleem-firstboot` shows the script's own log lines; `systemctl status autobleem-firstboot`
whether it is still going.

### Setting things up before the first boot

Everything is on the small FAT boot partition, editable from any PC after flashing:

- **WiFi** - two routes, both belonging to the base image rather than to AutoBleem:
  - Raspberry Pi Imager's OS customisation screen when flashing (user, hostname, WiFi, SSH, locale). Imager
    only offers that screen for images it has metadata for, which a file picked through "Use custom" does
    not have. `tools/rpi_imager_local_manifest.py` writes that metadata for your local image:
    ```bash
    python tools/rpi_imager_local_manifest.py --repo build_rpi_image/rpi_imager_repo.json \
        --arm64 build_rpi_image/autobleem-v2.0.0-pre0-ad109aa-rpi-arm64.img.xz -o build_rpi_image/os_list_local.rpi-imager-manifest
    ```
    then double-click the `.rpi-imager-manifest` file (or Imager: App Options -> Content Repository -> Use
    custom file, or `rpi-imager --repo <file>`): the AutoBleem image appears in the OS list *with* the
    customisation screen. Without `--repo` the sizes and hashes are computed from the image file (a minute).
  - or edit `network-config` on the boot partition by hand - it is cloud-init's netplan-style file and has a
    commented WiFi example (`wifis: wlan0: access-points:` and `regulatory-domain`).
- **`autobleem.txt`** - AutoBleem's own first-boot options, one `key=value` per line, documented in the file:
  `root_gib` (default 8), `hdmi_mode`, `retroarch` (source/apt/none - unset means the first boot asks),
  `thumbnails` (none/boxarts/all), `bios` (yes/no), `downloads` (yes/no). They become `install.sh` options.

`tools/make_rpi_image.sh` also writes `<out>/rpi_imager_repo.json` - the checked-in `tools/rpi_imager_repo.json`
template with this run's real `extract_size`/`extract_sha256`/`image_download_size`/`image_download_sha256`/
`release_date` filled in for whichever architecture was just built (running it for both architectures into
the same `--out` directory fills in both). That file is the input for the local manifest above, and the
starting point for a hosted one: `url` (wherever you host the `.img.xz`), `icon` and an optional `devices`
filter are left as placeholders on purpose - the template's own `"//"` field says why.

## The data partition

This is the only fiddly part. Raspberry Pi OS grows its root filesystem over the **whole** card on first
boot, so on a normal install there is no free space left for a games partition. Pick one of these:

**1. Stop the card being expanded in the first place (easiest, do it before the first boot).**
After flashing, open the small FAT partition on your PC and edit `cmdline.txt`: delete the word `resize`
(Raspberry Pi OS Trixie - it is what the initramfs keys on; older images had an
`init=/usr/lib/raspberrypi-sys-mods/firstboot` or `init=/usr/lib/raspi-config/init_resize.sh` part instead),
keep the rest of the line intact — it must stay a single line. Boot the Pi; the root filesystem stays
image-sized (about 3 GB, too small for the RetroArch build), so run the installer with `--grow-root 8`: it
grows the root to 8 GiB in place, and everything after it on the card is free space for the games partition.
This is exactly what the flashable image below does for you.

**2. Shrink the root filesystem from another computer.** Put the card in a Linux machine and shrink
partition 2 with GParted, then run the installer.

**3. Let the installer do it.**

```bash
sudo bash install.sh --shrink-root 8     # leave the system 8 GiB, give the rest to games
```

This cannot be done while the system is running — ext4 will not shrink while mounted — so the installer puts
the resize tools into the initramfs and arms a script that runs there at the next boot, before the root
filesystem is mounted: it resizes everything, formats the new partition, and reboots (you see
`autobleem-shrink:` lines on the console). It restores `cmdline.txt` before it touches a single partition,
so a failure costs you at worst a half-resized filesystem rather than a card that will not boot — but it is
still repartitioning. **Back up anything you care about first.**

However you get there, the result is an exFAT partition labelled `AUTOBLEEM`, mounted at `/media/autobleem`.
Windows 10 (1903 and later), macOS and Linux all show it when you plug the card in; it appears as a second
drive alongside the small `bootfs` one.

## Where things go

```
/media/autobleem/
  Games/<game name>/           your games - one folder each, .cue+.bin / .pbp / .chd (files dropped straight
                               into Games/ are sorted into folders by the scan). A multi-disc game is
                               one folder with every disc in it (plus the .m3u the scan writes); folders named
                               "Game (Disc 1)", "Game (Disc 2)"... are merged into "Game" by the scan - the
                               other discs' own Game.ini and save states are deleted in the process
  Games/!MemCards/             memory card sets
  Games/!SaveStates/           save states
  Autobleem/bin/autobleem/     autobleem-gui and its resources
  Autobleem/bin/db/            covers*.db - the cover art databases
  Autobleem/bin/emu/           pcsx-ab and plugins/ (gpu_peops.so, gpu_unai.so)
  System/Bios/                 romw.bin, romJP.bin - the PS1 BIOS pcsx-ab uses (from the pack, or yours)
  Autobleem/rc/                launch.sh, launch_rb.sh, retroarch.sh
  System/Databases/            regional.db (scanned games)
  System/Logs/                 AB_out.txt, AB_err.txt
  themes/                      UI themes (docs/theme-format.md)
  Apps/                        launchable apps
  RetroArch/                   RetroArch's standard tree, and everything it needs:
    roms/<system>/               your games for the other systems: a folder per system is already there
                                 ("Nintendo - Nintendo Entertainment System", "Sega - Mega Drive - Genesis",
                                 ...), named as RetroArch's playlists are; the scanner (Import Content ->
                                 Scan Directory -> roms) turns them into playlists
    system/                      the BIOS files the cores want - the installer's pack (system/biospack.txt)
    database/rdb/                libretro-database - "Sony - PlayStation.rdb" is where the launcher takes a
                                 game's title, publisher, year and players from (the covers*.db is the fallback)
    thumbnails/Sony - PlayStation/Named_Boxarts/   the launcher's PS1 covers (install.sh --thumbnails; a game's
                                 own <name>.png next to it still wins), Named_Titles/ and Named_Snaps/ with "all"
    cores/ info/                 the ~130 libretro cores and their info files
    playlists/                   what AutoBleem's RetroArch set shows
    saves/ states/ config/       saves, save states, per-core options
    assets/ autoconfig/ database/ cheats/ overlays/ shaders/ thumbnails/ screenshots/ logs/
    retroarch.cfg                every directory above is set in here; RetroArch keeps it up to date
```

Add games by copying them into `Games/` from any computer - a folder per game, or just the files: the scan
moves loose game files into a folder of their own. The launcher notices on its own — the scanner runs in
the background and the carousel updates while you watch.

## RetroArch

RetroArch is run with `--config /media/autobleem/RetroArch/retroarch.cfg`, so everything it reads or
writes stays on the data partition, where you can reach it from a PC. "RetroArch" in the launcher's L2+R2
system menu opens RetroArch's own menu; quitting it puts the launcher back.

The cores live in `RetroArch/cores/` on the exFAT partition. That works because the partition is mounted
without `noexec` - keep it that way if you edit `/etc/fstab`.

## Games for the other systems

Everything that is not a PlayStation game goes through RetroArch. Copy the files into the right folder
and wait: the launcher's own scan sees them within a few seconds of the copy settling, writes the system's
playlist and the games appear in the RetroArch set (the line at the bottom of the screen says "Scanning
ROMs" while it runs). RetroArch's own scanner is still there for what the launcher's cannot do yet -
naming a game after its database entry rather than its file - see step 2.

**1. Copy the games in.** Either pull the SD card: the `AUTOBLEEM` partition shows up as a drive on
Windows, macOS or Linux, and the games go into `RetroArch/roms/<system>/`. Or over the network, with the Pi
running: `scp`/WinSCP/FileZilla to `/media/autobleem/RetroArch/roms/<system>/` (SSH has to be enabled:
`sudo raspi-config` → Interface Options → SSH). The launcher does not need to be stopped for that.

A folder exists for every system the Pi's cores play; use those names, because RetroArch's scanner names
its playlists the same way and a game in the right folder always lands in the right playlist. Most cores
read `.zip`ped ROMs directly, and the scanner identifies a game by its contents, not its file name. The BIOS
files every one of these systems needs are already in `RetroArch/system/` (the installer's BIOS pack).

| Put them in `RetroArch/roms/...` | Files | Core, and what to know |
|---|---|---|
| `Arcade` | one `.zip` per game, exactly as the ROM set names it (+ its `.chd` for CD games) | **fbneo** wants a *current* FBNeo set, **mame2003_plus** a MAME 0.78-based one, **mame2000** a 0.37b5 one; a zip from the wrong set will not start. The BIOS zips (`neogeo.zip`, `qsound.zip`, `pgm.zip`, ...) are in `system/`; one put next to the games is listed as a game until you remove it. Both folders land in the `FBNeo - Arcade Games` playlist. |
| `SNK - Neo Geo` | the same `.zip` sets (MVS/AES) | fbneo |
| `SNK - Neo Geo CD` | `.cue`+`.bin` or `.chd` | neocd |
| `The 3DO Company - 3DO` | `.cue`/`.iso`/`.chd` | opera |
| `Atari - 2600` / `5200` / `7800` / `Lynx` / `8-bit Family` | `.a26` `.bin` / `.a52` / `.a78` / `.lnx` / `.atr` `.xex` `.cas` | stella2014 / atari800 / prosystem / handy / atari800 |
| `Coleco - ColecoVision`, `Mattel - Intellivision`, `Magnavox - Odyssey2`, `Philips - Videopac+`, `Fairchild - Channel F`, `GCE - Vectrex` | `.col` / `.int` `.bin` / `.bin` / `.bin` / `.bin` `.chf` / `.vec` | bluemsx / freeintv / o2em / o2em / fbneo / vecx |
| `NEC - PC Engine - TurboGrafx 16`, `... SuperGrafx` | `.pce`, `.sgx` | mednafen_pce (fast), mednafen_supergrafx |
| `NEC - PC Engine CD - TurboGrafx-CD` | `.cue`+`.bin` or `.chd` | mednafen_pce; the system cards are in `system/` |
| `Nintendo - Game Boy` / `Game Boy Color` / `Game Boy Advance` | `.gb` / `.gbc` / `.gba` | gambatte, sameboy / mgba |
| `Nintendo - Nintendo Entertainment System`, `... Family Computer Disk System` | `.nes`, `.fds` | fceumm, nestopia, mesen |
| `Nintendo - Super Nintendo Entertainment System` | `.sfc` `.smc` | snes9x (snes9x2005 on a slow Pi) |
| `Nintendo - Virtual Boy`, `Nintendo - Pokemon Mini` | `.vb`, `.min` | mednafen_vb, pokemini |
| `Sega - SG-1000` / `Master System - Mark III` / `Game Gear` | `.sg` / `.sms` / `.gg` | genesis_plus_gx |
| `Sega - Mega Drive - Genesis` / `32X` | `.md` `.bin` `.gen` / `.32x` | genesis_plus_gx, picodrive (32X) |
| `Sega - Mega-CD - Sega CD` | `.cue`+`.bin` or `.chd` | genesis_plus_gx |
| `SNK - Neo Geo Pocket` / `Color`, `Bandai - WonderSwan` / `Color` | `.ngp` `.ngc`, `.ws` `.wsc` | mednafen_ngp, mednafen_wswan |
| `Commodore - Amiga` | `.adf` `.adz` `.dms` `.hdf` `.lha` (WHDLoad), `.m3u` for a multi-disk game | puae; the Kickstart ROMs are in `system/`. Use *Manual Scan*. |
| `Commodore - CD32` | `.cue`+`.bin` or `.chd` | puae |
| `Commodore - 64` / `VIC-20` / `Plus-4` | `.d64` `.t64` `.prg` `.crt` `.tap`, `.m3u` for multi-disk | vice_x64 / vice_xvic / vice_xplus4 |
| `Amstrad - CPC` | `.dsk` `.cdt` | cap32 |
| `Sinclair - ZX Spectrum` / `ZX 81` | `.tzx` `.tap` `.z80` `.sna` `.dsk` / `.p` | fuse / 81 |
| `Microsoft - MSX` / `MSX2` | `.rom` `.dsk` `.cas` | fmsx or bluemsx |
| `NEC - PC-88` / `PC-98`, `Sharp - X1` / `X68000` | `.d88` / `.hdi` `.fdi` `.d88`, `.d88` `.2d` / `.dim` `.xdf` `.hdf` | quasi88 / np2kai, x1 / px68k; use *Manual Scan* |
| `DOS` | one folder per game with its files; a `.bat`/`.exe` to start, or a DOSBox `.conf` | dosbox_core; *Manual Scan* on the `.exe`/`.bat`/`.conf` files, or load one from RetroArch's file browser |
| `ScummVM` | one folder per game with its data files, plus an empty `<gameid>.scummvm` file in it (`monkey.scummvm`, `sky.scummvm`, ... - the ids ScummVM uses) | scummvm; *Manual Scan* with the `scummvm` extension. The engine data is in `system/scummvm/`. |
| `DOOM`, `Wolfenstein 3D` | the game's `.wad` (`doom.wad`, `doom2.wad`, ...); the `.wl6`/`.wl1` files in a folder | prboom, ecwolf; `prboom.wad`/`ecwolf.pk3` are in `system/` |

Multi-disc CD games: put every disc in the folder and add a `.m3u` file listing the `.cue`/`.chd` names one
per line; scan the `.m3u`, not the discs. Save files and save states go to `RetroArch/saves/` and
`RetroArch/states/`, whatever the system.

**2. The scan.** The launcher does it: every file in `roms/<system>/` that the system's core plays becomes
an entry, a `.cue` or `.m3u` stands for the discs it lists, an arcade `.zip` goes in whole. Each game is
looked up in RetroArch's database for its system (`RetroArch/database/rdb/`, installed with everything
else) by the CRC of its ROM - an arcade set by its archive's name - and takes the database's name
("Adventures of Lolo (USA)"), which is also what its box art is filed under, plus its publisher, year and
player count for the launcher's panel. A game the database does not know keeps its file name and is
listed all the same. A playlist RetroArch wrote is kept and added to, never replaced, and anything you put
in a playlist by hand that points outside `roms/` stays. Only the folders named as above are scanned; a
folder no installed core plays is skipped (the log says which).

A game whose cover is not in `RetroArch/thumbnails/` is fetched from libretro's thumbnail server after
the scan when the Pi is online (Options → "Fetch box art online", on by default); covers the server does
not have are remembered and not asked for again. Without a network nothing happens beyond one quick check
per scan.

RetroArch's own scanner (*Import Content → Scan Directory* or *Manual Scan*) is still there and the two
agree: the launcher keeps what RetroArch identified. It is the way to add things the launcher's scan does
not cover - a DOS game's `.bat`, a ScummVM folder.

A game RetroArch will not start is nearly always the wrong ROM set for the core (arcade) or a missing
`.m3u`/`.cue`; the log is in `RetroArch/logs/`.

**3. In the launcher** - **Select** cycles the sets (PlayStation → RetroArch → Apps), and inside the
RetroArch set **L2+Select** picks the playlist (system). Cross starts the game in the core the playlist
names; Favorites and History are RetroArch's own, kept up to date after every session. Playlists copied
onto the card by hand (or edited from a PC) are picked up at the next launcher start.

## How it boots

`autobleem.service` starts `/usr/local/bin/autobleem-session` on tty1 as root, with no X and no display
manager (Lite has neither). The session script runs the launcher in a loop: pick "RetroArch /
EmulationStation" from the L2+R2 system menu and it hands over to RetroArch, then puts the launcher back when
you quit. Power Off in that menu halts the Pi.

Running as root is deliberate — this is an appliance that needs the DRM device, every input node and the
power-off call, which is how the console runs it too. If that does not suit your setup, the service file is
plain systemd and easy to change.

The screen is set to 1920x1080 for the whole boot (`video=HDMI-A-1:1920x1080@60 video=HDMI-A-2:...` in
`cmdline.txt` — with the KMS driver, `hdmi_mode` in `config.txt` does nothing). The launcher opens its window
at that mode and draws its 1280x720 layout at 1.5x — covers and text at the screen's own resolution instead of
being upscaled by the TV — so there is no mode change for the TV to re-sync to when it comes up; on a 720p
screen use `--hdmi-mode 1280x720@60` and it draws at 1x. The AutoBleem logo is a
plymouth theme (`/usr/share/plymouth/themes/autobleem/`, packed into the initramfs of every installed
kernel, so the card shows it on whichever Pi it is moved to) that stays up until the
session script quits it right before starting the launcher — `plymouth-quit.service` is kept out of the boot
by the service file so it cannot do that earlier. The same logo shows while the Pi shuts down. The
firmware's rainbow square is turned off in `config.txt` (`disable_splash=1`). `--hdmi-mode`,
`--no-boot-splash` and `--no-quiet-boot` on the installer undo each of these; `--no-boot-config` leaves the
boot files alone altogether.

If a card set up before 2026-09-18 shows the stock plymouth theme on another Pi, `sudo update-initramfs -u
-k all` packs the AutoBleem theme into the other kernels' images too.

## If something goes wrong

The installer leaves `getty` on tty2–tty6, so **Alt+F2 gives you a login prompt** even when the launcher has
the screen. Enabling SSH first is a good idea: `sudo raspi-config` → Interface Options → SSH.

```bash
sudo journalctl -u autobleem -f                      # what the launcher is doing
cat /media/autobleem/System/Logs/AB_err.txt          # its own error log
sudo systemctl disable --now autobleem               # give the screen back
sudo systemctl enable --now getty@tty1
```

Backups of everything the installer edits are left next to the originals:
`cmdline.txt.autobleem-backup`, `config.txt.autobleem-backup`, `/etc/fstab.autobleem-backup`.

**Logo on screen but no launcher.** plymouth was not quit — check `journalctl -u autobleem`; the session
script quits it before anything else, so the service itself did not start. Esc shows plymouth's details view.

**Black screen, no launcher.** Usually SDL cannot get the display. Check `journalctl -u autobleem` for
`kmsdrm`; make sure `dtoverlay=vc4-kms-v3d` is in `/boot/firmware/config.txt` (it is the default on current
images) and that nothing else is holding the console.

**PS1 games do nothing.** Look at `System/Logs/AB_out.txt` for the `AUTOBLEEM: starting PS1 game` line and
what pcsx-ab said after it. `no ... romw.bin` means the BIOS is missing (HLE is being used — a game that
needs the real one will not boot): re-run the installer without `--no-bios`. "Play using RA" needs `RetroArch/cores/pcsx_rearmed_libretro.so`: re-run
the installer without `--no-downloads`, or fetch it from RetroArch's Online Updater.

**RetroArch shows no games / AutoBleem's RetroArch set is empty.** Nothing has been scanned yet — see the
RetroArch section above. The set is built from `RetroArch/playlists/*.lpl` and `RetroArch/info/*.info`.

**No titles or box art.** The cover databases were not in the package. Copy `covers*.db` into
`/media/autobleem/Autobleem/bin/db/` and re-scan.
