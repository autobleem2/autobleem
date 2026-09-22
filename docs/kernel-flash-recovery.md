# PlayStation Classic — kernel flash recovery

What to do when a kernel flash (abflashkit) goes wrong. Three levels, softest first:
**Sony's own recovery** (automatic, no PC) → **Restore Mode / a shell** → **fastboot / MediaTek
download mode** (hardware, last resort). The console is a MediaTek **MT8167**; its eMMC partitions
are `BOOTIMG1` (kernel), `ROOTFS1` (system), `USRDATA` (`/data`), `TEE1` (trustzone), `MISC` (the
recovery flag).

> ⚠️ The one rule that makes all of this work: **never delete `LBOOT.EPB`** from your stick, and keep a
> copy somewhere else too. Backups are **universal** — an `LBOOT.EPB` from *any* PSC restores *any* PSC.

---

## 0. Before you flash (do this once)

- Run **Full backup** in abflashkit first (writes `USB/LBOOT.EPB` — all four partitions). Copy that file
  off the stick to your PC as well.
- Have a **second stick** ready, and ideally a **known-good `LBOOT.EPB`** from another console.
- For the hardware path later: a PC with **SP Flash Tool** (or **mtkclient**), a **USB-A ↔ USB-A** cable,
  and knowledge of the **PSC fastboot/BROM test points on the PCB** (you have these — this doc assumes it).

## 1. What the power LED tells you during a flash

| LED | Meaning |
|---|---|
| **green, steady** | idle / done |
| **blinking green** | making the backup (slow — can take minutes; do **not** pull power) |
| **red** | recovery flag is **set** and the kernel is being written — the danger window |
| **off** | about to reboot |

`abflashkit` sets the `MISC` recovery flag (`recovery-on.img` → red LED), `dd`s `boot.img` to `BOOTIMG1`,
unpacks the overlay, then clears the flag (`recovery-off.img` → green) and reboots. If it dies between
"red" and "green", the flag is still set — which is exactly what triggers level 2 automatically.

---

## Level 1 — Sony's recovery (automatic, no PC needed)

**Symptom:** after a failed flash or a bad kernel, the console **won't boot** — the power LED **blinks
green, then red**.

That means the `MISC` recovery flag is still on, so the bootloader entered the console's **own recovery**.
It will restore from `LBOOT.EPB` because that file carries the `RAWCAW` signature the recovery accepts.

**Do this:**
1. Put the stick with `LBOOT.EPB` back in a USB port.
2. **Replace the power cord** (a full power cycle — not the button).
3. Wait. Recovery reads `LBOOT.EPB`, restores `BOOTIMG1`/`ROOTFS1`/`USRDATA`/`TEE1`, clears `MISC`, and
   reboots normally. This takes a few minutes — **don't interrupt it**.

This is the intended safety net and fixes the large majority of bad flashes.

---

## Level 2 — Deliberate rollback (the console still runs, or you have a shell)

### 2a. Restore Mode (from abflashkit)
If the console **does boot into AutoBleem** but the new kernel misbehaves (e.g. Bluetooth/Wi-Fi broken but
the UI works), or you just want stock back:

- abflashkit → **Restore Mode**. It checks the backup is AutoBleem's, inspects it for a stock kernel
  (warns if the backup looks modified or is an old ABFK 1.0a backup with no rootfs), then asks
  **"Set recovery mode and reboot now?"** → sets the flag → reboots → Sony's recovery restores from
  `LBOOT.EPB`.
- Needs a valid `LBOOT.EPB` on the stick. (`validlboot` next to it skips the inspection.)

### 2b. Force it from a shell (dropbear over the network, if the old kernel still networks)
The AutoBleem overlay runs `dropbear`, so if the console boots far enough to bring up the network you can
SSH in and drive the recovery by hand:

```sh
# put the console into recovery and let LBOOT.EPB restore it
dd if=/media/Apps/abflashkit/kernel/recovery-on.img of=/dev/disk/by-partlabel/MISC
sync
systemctl reboot        # boots into Sony's recovery -> restores from LBOOT.EPB on the stick
```

Or restore a single partition directly from your backup, without going through recovery — unzip
`LBOOT.EPB` first (it's a plain zip; ignore the 4096-byte trailer) to get `boot.img`, `rootfs.ext4`,
`userdata.ext4`, `tz.img`:

```sh
dd if=boot.img        of=/dev/disk/by-partlabel/BOOTIMG1   # the STOCK kernel from your backup
dd if=recovery-off.img of=/dev/disk/by-partlabel/MISC      # make sure the recovery flag is clear
sync; systemctl reboot
```

Only ever write `BOOTIMG1` and `MISC` this way unless you deliberately mean to restore `ROOTFS1`/`USRDATA`.
**Leave the preloader and `TEE1` alone.**

---

## Level 3 — Fastboot / MediaTek download mode (hardware, last resort)

Use this only when level 1 fails — e.g. the console **blinks red even with the backup stick inserted**
(corrupt `LBOOT.EPB` or a `MISC`/bootloader too damaged to reach recovery). This talks to the SoC directly
through the **test points on the PCB**, so it works even when nothing boots.

> **Honesty note:** the exact PSC test-point locations and whether the stock/AutoBleem U-Boot exposes a
> plain `fastboot` interface are hardware facts **you hold**, not something in these sources. The routine
> below is *what to do once the console is in download/fastboot mode* — plug in your known points.

### The images you flash (all come from `LBOOT.EPB`)
Unzip `LBOOT.EPB` → `boot.img`, `rootfs.ext4`, `userdata.ext4`, `tz.img`. `recovery-off.img` (16 zero bytes,
in `Apps/abflashkit/kernel/`) clears `MISC`. Work from a **known-good** backup — your own pre-flash one, or
one from another PSC (they're universal).

### 3a. If U-Boot fastboot is reachable
Trigger fastboot via your PCB point, then on the PC:

```sh
fastboot devices                        # the console must appear
fastboot flash BOOTIMG1 boot.img        # stock kernel back
fastboot flash MISC     recovery-off.img# clear the recovery flag
fastboot flash ROOTFS1  rootfs.ext4     # only if the rootfs is damaged
fastboot reboot
```

Partition names are the `by-partlabel` names above. **Flash only what's broken.** Do not flash preloader
or `TEE1` unless you know exactly why.

### 3b. MediaTek BROM download mode — the true unbrick (SP Flash Tool / mtkclient)
For a hard brick (no fastboot, dead bootloader): short the **BROM/download test point to ground while
connecting USB**, so the MT8167 boot ROM enters download mode *before* the preloader runs. Then:

- **SP Flash Tool** (Windows) with a **scatter file** describing the PSC's eMMC layout, **Download Only**
  mode — or **mtkclient** (open source, Linux/Windows).
- **Read back / dump the current partitions first** if the tool can, so you have an exact map before you
  write anything.
- Write **`BOOTIMG1`** and **`MISC`** at minimum; add `ROOTFS1`/`USRDATA` only if they were wiped.
- ⚠️ **Wrong region, preloader, or `TEE1` writes can HARD-brick the console.** Never "format all". Only
  download the specific partitions you have correct images for, from your own `LBOOT.EPB`.

---

## Quick decision tree

```
Console won't boot after a flash?
├─ blinks green→red, stick with LBOOT.EPB in?  →  replace power cord, wait   (Level 1)
├─ boots into AutoBleem but kernel is bad?      →  abflashkit Restore Mode    (Level 2a)
├─ networks but won't run the tool?             →  ssh + dd MISC/BOOTIMG1     (Level 2b)
└─ blinks RED even with the backup stick?       →  fastboot / SP Flash Tool   (Level 3)
```

## Golden rules

- **Never delete `LBOOT.EPB`.** It is the whole recovery.
- Backups are **universal** — one from any PSC restores any PSC.
- During a flash, **red = danger window**; only pull power if it's clearly hung for many minutes.
- Touch **only `BOOTIMG1` and `MISC`** unless you are deliberately restoring `ROOTFS1`/`USRDATA`.
  **Never** the preloader or `TEE1`.
- Everything you need to reflash is inside `LBOOT.EPB` (`boot.img`, `rootfs.ext4`, `userdata.ext4`,
  `tz.img`) plus `recovery-off.img` from `Apps/abflashkit/kernel/`.
