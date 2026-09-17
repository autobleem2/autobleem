# AutoBleem on a Raspberry Pi

Turns a Raspberry Pi running **Raspberry Pi OS Lite (32-bit)** into an AutoBleem machine: it boots straight
into the launcher with no desktop, and its games live on an exFAT partition you can plug into a Windows, Mac
or Linux machine and drop games onto — the same way the PlayStation Classic's USB stick works.

## Status

This is a port in progress. Be aware of what is and is not here:

| | |
|---|---|
| Launcher, scanner, themes, memory cards, covers | built and packaged |
| RetroArch sets and playlists | work through the distribution's RetroArch |
| PS1 games | run through RetroArch's `pcsx_rearmed` core |
| **pcsx-ab** | **not ported yet.** Until it is, AutoBleem's own save-state slots ("Resume") do nothing for PS1 games — `pcsx_rearmed` cannot read them. Drop a Pi build of `pcsx-ab` into `Autobleem/bin/emu/` and `rc/launch.sh` picks it up with no other change. |
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

The installer: installs SDL2, exfatprogs, parted and RetroArch; finds or creates the exFAT data partition;
builds the AutoBleem tree on it; installs the launcher, themes and launch scripts; and wires up a systemd
service that owns tty1.

`--help` lists the options. The useful ones are `--shrink-root`, `--fetch-cores`, `--no-packages` and
`--stage`.

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

This cannot be done while the system is running — ext4 will not shrink while mounted — so the installer arms
a script that runs at the next boot before the root filesystem comes up, resizes everything, and reboots.
It is the same mechanism Raspberry Pi OS uses for its own first-boot expansion. It restores `cmdline.txt`
before it touches a single partition, so a failure costs you at worst a half-resized filesystem rather than a
card that will not boot — but it is still repartitioning. **Back up anything you care about first.**

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
  Autobleem/bin/emu/           pcsx-ab goes here once it is ported
  Autobleem/rc/                launch.sh, launch_rb.sh, retroarch.sh
  System/Databases/            regional.db (scanned games)
  System/Logs/                 AB_out.txt, AB_err.txt
  themes/                      UI themes (docs/theme-format.md)
  Apps/                        launchable apps
  retroarch/                   RetroArch's config and saves
```

Add games by copying a folder into `Games/` from any computer. The launcher notices on its own — the scanner
runs in the background and the carousel updates while you watch.

## How it boots

`autobleem.service` starts `/usr/local/bin/autobleem-session` on tty1 as root, with no X and no display
manager (Lite has neither). The session script runs the launcher in a loop: pick "RetroArch /
EmulationStation" from the L2+R2 system menu and it hands over to RetroArch, then puts the launcher back when
you quit. Power Off in that menu halts the Pi.

Running as root is deliberate — this is an appliance that needs the DRM device, every input node and the
power-off call, which is how the console runs it too. If that does not suit your setup, the service file is
plain systemd and easy to change.

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
`cmdline.txt.autobleem-backup`, `/etc/fstab.autobleem-backup`.

**Black screen, no launcher.** Usually SDL cannot get the display. Check `journalctl -u autobleem` for
`kmsdrm`; make sure `dtoverlay=vc4-kms-v3d` is in `/boot/firmware/config.txt` (it is the default on current
images) and that nothing else is holding the console.

**PS1 games do nothing.** There is no `pcsx_rearmed` core installed — `sudo apt install libretro-pcsx-rearmed`,
or re-run the installer with `--fetch-cores`.

**No titles or box art.** The cover databases were not in the package. Copy `covers*.db` into
`/media/autobleem/Autobleem/bin/db/` and re-scan.
