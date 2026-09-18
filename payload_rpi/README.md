# AutoBleem on a Raspberry Pi

Turns a Raspberry Pi running **Raspberry Pi OS Lite (32-bit)** into an AutoBleem machine: it boots straight
into the launcher with no desktop, and its games live on an exFAT partition you can plug into a Windows, Mac
or Linux machine and drop games onto — the same way the PlayStation Classic's USB stick works.

## Status

This is a port in progress. Be aware of what is and is not here:

| | |
|---|---|
| Launcher, scanner, themes, memory cards, covers | built and packaged |
| PS1 games | run in **pcsx-ab**, the console's own emulator, built for the Pi (`Autobleem/bin/emu/`, with the `gpu_peops`/`gpu_unai` plugins) — save states, memory cards and "Resume" work the way they do on the console |
| BIOS | **you supply it**: `System/Bios/romw.bin` (plus `romJP.bin` for Japanese games — a copy of `romw.bin` will do). Without it pcsx-ab runs on its HLE BIOS, which many games tolerate and some do not |
| RetroArch | the **latest release, built from source by the installer** (libretro's buildbot has every armhf core but no frontend build), with all ~130 cores, core info, menu assets, pad autoconfigs and scanner databases downloaded into `RetroArch/` on the data partition. AutoBleem's RetroArch set shows the playlists RetroArch's scanner writes there; "Play using RA" runs a PS1 game in `pcsx_rearmed` |
| Internal games | gone, by design — a Pi has no built-in game list, so the set and its option are compiled out |
| Tested on real hardware | **no.** Everything here is cross-compiled and reviewed but has not yet been run on a Pi. Treat the first install as an experiment, on a card you can afford to re-flash. |

## What you need

- A Raspberry Pi 2, 3, 4 or Zero 2 W. The build targets `armv7-a` with NEON, so the original Pi 1 and Zero
  (armv6) are **not** supported.
- **32-bit** Raspberry Pi OS Lite. The binary is `arm-linux-gnueabihf`; a 64-bit image will refuse to install
  it (the installer checks and says so).
- An SD card with room for your games, a keyboard for the first login, and a USB gamepad.

## Build the package (on the PC)

```bash
./make_rpi.sh                    # cross-compiles with toolchains/rpi/RPitoolchain.cmake
./tools/make_rpi_package.sh      # -> build_rpi/autobleem-rpi.tar.gz
```

pcsx-ab is checked in under `payload_rpi/Autobleem/bin/emu/` and rides along. To refresh it from a new
build, run pcsx-rearmed-develop's `AUTOBLEEM_DIR=../autobleem-develop ./make_rpi.sh`, which copies its
`build_rpi/dist/` there.

Copy `autobleem-rpi.tar.gz` to the Pi (`scp`, or just put it on a USB stick).

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

The installer: installs SDL2, libpng, exfatprogs and parted; builds the latest RetroArch release from
source (10-40 minutes depending on the Pi - `--retroarch apt` takes the distribution's package instead,
`--retroarch none` skips it); finds or creates the exFAT data partition; builds the AutoBleem and RetroArch
trees on it; downloads every armhf core plus RetroArch's info/assets/autoconfig/database bundles from
`buildbot.libretro.com` (a few hundred MB; `--no-downloads` skips it, RetroArch's Online Updater can do it
later); installs the launcher, pcsx-ab, themes and launch scripts; wires up a systemd service that owns
tty1; and sets up a quiet boot at 720p with the AutoBleem logo on screen (plymouth) instead of the kernel
log. It works on Raspberry Pi OS Bookworm and Trixie (Trixie renamed some packages for its 64-bit `time_t`
transition; the installer tries both names).

Then put your BIOS in `System/Bios/` on the data partition — `romw.bin`, and `romJP.bin` for Japanese games
(the console uses one file for both; a copy is fine). pcsx-ab reads them from there on every launch.

`--help` lists the options. The useful ones are `--shrink-root`, `--retroarch`, `--no-downloads`,
`--no-packages`, `--stage`, `--hdmi-mode` (default `1280x720@60`; `none` keeps the screen's preferred mode),
`--no-boot-splash` and `--no-quiet-boot`.

## The data partition

This is the only fiddly part. Raspberry Pi OS grows its root filesystem over the **whole** card on first
boot, so on a normal install there is no free space left for a games partition. Pick one of these:

**1. Stop the card being expanded in the first place (easiest, do it before the first boot).**
After flashing, open the small FAT partition on your PC and edit `cmdline.txt`: delete the
`init=/usr/lib/raspberrypi-sys-mods/firstboot` (older images: `init=/usr/lib/raspi-config/init_resize.sh`)
part, keep the rest of the line intact — it must stay a single line. Boot the Pi; the root filesystem stays
image-sized and everything else on the card is free space the installer will happily use.

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
  Games/<game name>/           your games - one folder each, .cue+.bin / .pbp / .chd
  Games/!MemCards/             memory card sets
  Games/!SaveStates/           save states
  Autobleem/bin/autobleem/     autobleem-gui and its resources
  Autobleem/bin/db/            covers*.db - the cover art databases
  Autobleem/bin/emu/           pcsx-ab and plugins/ (gpu_peops.so, gpu_unai.so)
  System/Bios/                 romw.bin, romJP.bin - the PS1 BIOS, yours to provide
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
    system/                      the BIOS files the cores want (scph1001.bin, ...) - yours to provide
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

Add games by copying a folder into `Games/` from any computer. The launcher notices on its own — the scanner
runs in the background and the carousel updates while you watch.

## RetroArch

RetroArch is run with `--config /media/autobleem/RetroArch/retroarch.cfg`, so everything it reads or
writes stays on the data partition, where you can reach it from a PC. Copy games for other systems into
the matching `RetroArch/roms/<system>/` folder (they are created for you - NES, SNES, Game Boy, Mega Drive,
Master System, Game Gear, PC Engine, Neo Geo Pocket, Arcade, ...) and their BIOS files into
`RetroArch/system/`, then in RetroArch use *Import Content → Scan Directory* on `roms/`: the playlists it
writes turn up as AutoBleem's RetroArch set the next time the launcher starts. "RetroArch" in the L2+R2
system menu opens RetroArch's own menu; quitting it puts the launcher back. "RetroArch" in the launcher's L2+R2 system
menu opens RetroArch's own menu; quitting it puts the launcher back.

The cores live in `RetroArch/cores/` on the exFAT partition. That works because the partition is mounted
without `noexec` - keep it that way if you edit `/etc/fstab`.

## How it boots

`autobleem.service` starts `/usr/local/bin/autobleem-session` on tty1 as root, with no X and no display
manager (Lite has neither). The session script runs the launcher in a loop: pick "RetroArch /
EmulationStation" from the L2+R2 system menu and it hands over to RetroArch, then puts the launcher back when
you quit. Power Off in that menu halts the Pi.

Running as root is deliberate — this is an appliance that needs the DRM device, every input node and the
power-off call, which is how the console runs it too. If that does not suit your setup, the service file is
plain systemd and easy to change.

The screen is set to 1280x720 for the whole boot (`video=HDMI-A-1:1280x720@60 video=HDMI-A-2:...` in
`cmdline.txt` — with the KMS driver, `hdmi_mode` in `config.txt` does nothing), which is the launcher's own
resolution, so there is no mode change for the TV to re-sync to when it comes up. The AutoBleem logo is a
plymouth theme (`/usr/share/plymouth/themes/autobleem/`, packed into the initramfs) that stays up until the
session script quits it right before starting the launcher — `plymouth-quit.service` is kept out of the boot
by the service file so it cannot do that earlier. The same logo shows while the Pi shuts down. The
firmware's rainbow square is turned off in `config.txt` (`disable_splash=1`). `--hdmi-mode`,
`--no-boot-splash` and `--no-quiet-boot` on the installer undo each of these; `--no-boot-config` leaves the
boot files alone altogether.

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
needs the real one will not boot). "Play using RA" needs `RetroArch/cores/pcsx_rearmed_libretro.so`: re-run
the installer without `--no-downloads`, or fetch it from RetroArch's Online Updater.

**RetroArch shows no games / AutoBleem's RetroArch set is empty.** Nothing has been scanned yet — see the
RetroArch section above. The set is built from `RetroArch/playlists/*.lpl` and `RetroArch/info/*.info`.

**No titles or box art.** The cover databases were not in the package. Copy `covers*.db` into
`/media/autobleem/Autobleem/bin/db/` and re-scan.
