# Installing AutoBleem on a Raspberry Pi, from a blank SD card

A step-by-step path from an empty SD card to a running AutoBleem Pi. For everything else (the data
partition, RetroArch, adding games, troubleshooting) see `payload_rpi/README.md` - this page only gets you
there.

**64-bit note**: the 64-bit build is cross-compiled, packaged and verified on the PC (real aarch64
binaries, a working tarball, the installer's architecture check exercised) but **has not been run on
actual 64-bit Pi hardware yet** - there is no 64-bit Pi OS card to test it on at the time of writing. The
32-bit build has been running on a Pi 400 since 2026-09-18. If you hit something odd on 64-bit, that is why.

## What you need

- A Raspberry Pi: 2, 3, 4 or Zero 2 W for the 32-bit build; 3, 4, 5, 400 or Zero 2 W for the 64-bit one
  (both need `armv7-a`+NEON or `armv8-a` - not the original Pi 1 or Zero).
- An SD card (8 GB minimum; more for games - the installer needs a few GB free for the data partition on
  top of whatever games you copy on).
- A PC with [Raspberry Pi Imager](https://www.raspberrypi.com/software/) installed, and an SD card reader.
- The tarball for your Pi's architecture, built from this repo:
  `./make_rpi.sh && ./tools/make_rpi_package.sh --arch armhf` -> `build_rpi/autobleem-rpi.tar.gz` (32-bit), or
  `./make_rpi64.sh && ./tools/make_rpi_package.sh --arch arm64` -> `build_rpi64/autobleem-rpi-arm64.tar.gz` (64-bit).
- A network the Pi and your PC both reach (for `scp`ing the tarball over and, later, adding games).
- A USB gamepad, for once it is running. A keyboard is only needed if you skip the SSH setup below.

## 1. Flash Raspberry Pi OS Lite

The OS's bit-width has to match the tarball's - the installer checks and refuses a mismatch (`unsupported
architecture` or the wrong `dpkg --print-architecture`), so get this right before anything else:

| Tarball | Flash |
|---|---|
| `autobleem-rpi.tar.gz` | **Raspberry Pi OS Lite (32-bit)** |
| `autobleem-rpi-arm64.tar.gz` | **Raspberry Pi OS Lite (64-bit)** |

1. Open Raspberry Pi Imager, **Choose Device** -> your Pi model.
2. **Choose OS** -> "Raspberry Pi OS (other)" -> the Lite build matching the table above. **Lite**, not the
   desktop image - AutoBleem owns the whole screen, it does not want a desktop under it.
3. **Choose Storage** -> your SD card. Double-check it is the right drive - this erases it.
4. Before writing, click the gear icon (or Ctrl+Shift+X) for the advanced options and set:
   - **Hostname** (e.g. `autobleem`) - lets you reach it as `autobleem.local` instead of hunting for an IP.
   - **Enable SSH**, with a password (or your public key).
   - A **username and password**.
   - Locale/timezone/keyboard layout if you care; not required.

   Setting these now means the Pi is reachable over SSH from its very first boot - no monitor or keyboard
   needed for anything in this guide.
5. **Write**, then eject the card once it verifies.

## 2. First boot

1. Put the card in the Pi and power it on. First boot resizes the filesystem and can take a minute or two
   longer than normal - give it a couple of minutes before trying to connect.
2. From your PC:
   ```bash
   ssh <username>@<hostname>.local        # e.g. ssh pi@autobleem.local
   ```
   If `.local` (mDNS) does not resolve on your network, find the Pi's IP from your router's client list
   instead and `ssh <username>@<ip>`.

## 3. Copy the tarball over

From your PC, in the directory with the tarball:

```bash
scp autobleem-rpi.tar.gz <username>@<hostname>.local:~/          # 32-bit
scp autobleem-rpi-arm64.tar.gz <username>@<hostname>.local:~/    # 64-bit
```

## 4. Run the installer

Back in the SSH session on the Pi:

```bash
tar xzf autobleem-rpi*.tar.gz
cd autobleem-rpi
sudo bash install.sh --dry-run   # optional: prints every change it would make, changes nothing
sudo bash install.sh
```

(`sudo bash install.sh`, not `./install.sh` - a tarball built on Windows loses the executable bit on the
way over, and the installer needs root anyway: it partitions the card, installs packages and writes the
boot config.)

It will ask before writing a new partition to the card ("This writes a new partition table entry..." -
type `YES`), then: install SDL2/RetroArch's build dependencies, build RetroArch from source (10-40 minutes
depending on the Pi - this is most of the wait), download ~130 RetroArch cores and their assets, download
the BIOS pack, install AutoBleem itself, and set up the boot splash. Let it finish - the summary at the end
tells you where everything landed.

If it stops at "no room to make one" because the card's root filesystem was already grown over the whole
card (Raspberry Pi Imager's default), that is expected and covered in `payload_rpi/README.md` under "The
data partition" - the short version is `sudo bash install.sh --shrink-root 8`, which repartitions the card
on the next boot (destructive if you already put something on the card - do this on a fresh card).

## 5. Reboot and play

```bash
sudo reboot
```

The Pi comes back up straight into the AutoBleem launcher on the TV/monitor, no login prompt. Plug in a USB
gamepad, and add games by copying them onto the `AUTOBLEEM` partition (`Games/<game name>/`) from any
computer - pull the card, or `scp`/network-share to the Pi while it runs. See `payload_rpi/README.md`
("Where things go", "Games for the other systems") for the full layout and how RetroArch's games are
scanned in.

## If it does not come up

`payload_rpi/README.md`'s "If something goes wrong" section covers this: `Alt+F2` for a login prompt,
`sudo journalctl -u autobleem -f` for what the launcher is doing, and the specific black-screen/no-launcher/
no-games symptoms it has already seen.
