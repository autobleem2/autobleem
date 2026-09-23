# Tester checklist - the release channels, the installers and the updaters

What needs a person with real hardware (or a VM) before the channel work of 2026-09-23 counts as proven.
Every item has what you need, what to do, what you should see, and where the logs are. Report each item as
**pass / fail / blocked**, with the build's version (Options -> About, or the `VERSION` file), the logs
listed for it, and a photo of the screen when something looks wrong.

The three channels, everywhere below:

| channel | what it is | where the site lists it |
|---|---|---|
| **Release** | the newest stable `v*` release - **none exists yet**, so this channel has nothing to offer today; "nothing to install" is the right answer | `releases/latest.json` |
| **Testing** | the one pre-release (today `v2.0.0-alpha2`) | `releases/unstable.json` |
| **Nightly** | the newest development build of `develop` (`v2.0.0-alpha2-N-g<hash>`) | `nightly/latest.json` |

Status column: **ready** = can be tested now; **blocked** = waits for the item named.

---

## 1. PC USB stick flasher (Windows) - ready once `AutoBleemFlasher-<v>.zip` is on the site

**You need:** a Windows 10/11 PC, a USB stick of 8 GB or more whose contents you can lose, an internet
connection; for the boot test a PC (or VirtualBox) that can boot from USB with Secure Boot off.
**Get it:** the download page -> PC -> PC USB stick -> "Flasher for Windows". Unzip, run `AutoBleemFlasher.exe`.

| # | Do | Expect |
|---|---|---|
| 1.1 | Start it | Windows asks for administrator rights (UAC). The window shows the AutoBleem picture, a Channel box and a USB stick box. Within a few seconds the text under them names the channel's version and the download size. |
| 1.2 | Pick **Release** | "The release channel has no stick image" (no stable release yet) - Write is greyed out. |
| 1.3 | Plug a stick in, without pressing Refresh | It appears in the USB stick box by itself, described as model - size (USB, disk N) - drive letter. |
| 1.4 | Have a USB **hard drive** plugged in too, if you have one | It is **not** listed. Tick "Show USB hard drives too": it appears, marked "USB hard drive". Untick it again. |
| 1.5 | Pick **Nightly**, the stick, press Write | Two questions: the first names the stick and says everything is erased (Cancel is the default); the second is a last "erase ... now?" (No is the default). Answering No stops everything, and the stick is untouched. |
| 1.6 | Write again, answer yes twice | Step 1 of 3 downloads (~650 MB, with a bar), Step 2 of 3 writes (the bar in GB), Step 3 of 3 reads back. It ends with "Done - the stick is ready". |
| 1.7 | Run it again, same channel, another stick (or the same one) | Step 1 finishes at once - "is already there" (the image is kept in `%TEMP%\AutoBleemFlasher`). |
| 1.8 | Pick **An image file on this PC...**, choose an `.img.xz` you downloaded from the site, write | Same as 1.6 without the download. With the site's `.sha256` file next to it, the log says "matches its .sha256". |
| 1.9 | Start a write and press Stop half way through | It stops. The message says the stick is not usable until it is written again. Writing it again works. |
| 1.10 | Open a file from the stick in Explorer (or leave an Explorer window on it), then Write | Either it works (Windows let go of the stick), or it fails with "the stick is in use - close every window and program...". It never writes half a stick silently. |
| 1.11 | Boot a PC (BIOS, and UEFI if you can) from a stick written in 1.6 | GRUB, the AutoBleem boot picture, the first-boot setup screen, then the launcher - as for a stick written with `dd`. |

**Logs:** the flasher's window log (copy it with a screenshot); for 1.11 the stick's
`/var/log/autobleem-firstboot-install.log`.

---

## 2. PlayStation Classic installer - the channel box (Windows) - ready

**You need:** a Windows PC, a USB stick for the console, internet. **Get it:** download page ->
PlayStation Classic -> "Installer for Windows" (`AutoBleemInstaller-<v>.zip`: the program and its README only -
the stick's files are downloaded).

| # | Do | Expect |
|---|---|---|
| 2.1 | Start it | Channel defaults to the installer's own kind (a nightly installer -> Nightly). After a few seconds the status says what the channel would install ("A fresh install of AutoBleem v..."). |
| 2.2 | Switch between the three channels | The version in the status changes; Release says it has nothing to install. |
| 2.3 | Fresh stick, **Testing**, Install | The package is downloaded and checked, then unpacked; the stick is named SONY; UpdateRoms is on it (`UpdateRoms/UpdateRoms.exe`); the console boots it. |
| 2.4 | Run it again over that stick with **Nightly** | The status says "AutoBleem v2.0.0-alpha2 is on this stick: it will be updated to ...". After Update, your games, saves and `config.ini` settings are still there and the console boots the new version (About screen). |
| 2.5 | Unplug the network cable / WiFi, start it | The status says the channel has nothing to install and mentions the internet connection. Install stays greyed out. |

**Logs:** `<stick>/System/Logs/installer.log`.

---

## 3. PlayStation Classic updating itself - blocked

**Blocked on:** the AutoBleem kernel payload gaining `curl` (not released yet), and the launcher branch
`feature/psc-self-update` being merged into a nightly. This item will be marked ready here when both are done.

**You need:** a console with the **AutoBleem kernel** (ABFlashKit) and WiFi set up in PSC-Bios, plus a
stick one version behind the channel you test (for example a Testing stick, with the Nightly channel chosen).

| # | Do | Expect |
|---|---|---|
| 3.1 | Options -> Updates | The row is there, with release / testing / nightly / off. |
| 3.2 | Choose **nightly**, go back to the carousel, wait up to a minute | An "Update available" dialog shows both versions. |
| 3.3 | "Remind me tomorrow" | The dialog goes away and does not come back when you restart the launcher. |
| 3.4 | L2+R2 -> Software Update | The check runs on screen and offers the update again. |
| 3.5 | "Update now" | A download bar is shown. The launcher then closes, the AutoBleem picture stays on screen while the stick is updated (up to a few minutes), and the launcher comes back. |
| 3.6 | Check the result | About shows the new version. Games, save states, memory cards, the theme and other settings are unchanged. |
| 3.7 | Turn WiFi off (or use a console with the stock kernel), then L2+R2 -> Software Update | "Not connected". No dialog appears at start and nothing slows down. |
| 3.8 | Pull the stick during 3.5's download (only on a stick you can re-install) | The launcher reports the failure. The next boot starts the old launcher. |

**Logs:** `System/Logs/update.log` (the update step), `System/Logs/installer.log`, `System/Logs/autobleem.log`
(the check and the download).

---

## 4. Raspberry Pi updating itself - ready

**You need:** a Pi with an AutoBleem image that is one version behind the channel you choose, a network
connection, and a keyboard or pad. Test both 32-bit and 64-bit if you can.

| # | Do | Expect |
|---|---|---|
| 4.1 | Options -> Updates -> nightly (or testing) | Within a minute: "Update available" with the AutoBleem version (and RetroArch if it is newer). |
| 4.2 | Update now | The download bar is shown. The launcher then leaves, the setup screen runs the update on tty1, and the launcher comes back with the new version. |
| 4.3 | Check | Games, settings, cores, BIOS files and samples are kept. The first start rescans the games once. |

**Logs:** `System/Logs/update.log` on the data partition, `System/Logs/autobleem.log`.

## 5. PC USB stick updating itself - ready

The same as 4, on a PC stick (or the VirtualBox VM) that is one version behind. When it was installed, the
setup asked for the graphical or text screen; the update must draw with the same one.

## 6. Windows program updating itself - ready once a newer `AutoBleemSetup` is published on the chosen channel

**You need:** AutoBleem installed with `AutoBleemSetup-<older>.exe` from the site.

| # | Do | Expect |
|---|---|---|
| 6.1 | Options -> Updates -> testing (or nightly) | "Update available" shows the site's version. |
| 6.2 | Update now | The download runs. The launcher closes, the setup wizard shows its progress, and the new launcher starts by itself. |
| 6.3 | Check | Data folder, games, settings, RetroArch and cores are all kept. Add/Remove Programs shows the new version. |

**Logs:** `<data folder>\System\Logs\autobleem.log`, and `System/update.json` (the check's state).

---

## 7. Raspberry Pi Imager - one list per channel - ready

**You need:** Raspberry Pi Imager 1.9 or newer and an SD card.

| # | Do | Expect |
|---|---|---|
| 7.1 | Download page -> Raspberry Pi: the box lists the Imager repository addresses | Only Testing and Nightly are listed (no stable release yet). Each has a Copy button. |
| 7.2 | Imager -> App Options -> Content Repository -> Use custom URL, paste the Testing one | AutoBleem appears with the 32-bit and 64-bit images of `v2.0.0-alpha2`. |
| 7.3 | The same with the Nightly one | The nightly build's images, newer version number. |
| 7.4 | Flash one of them with Imager's customisation (user, WiFi) and boot it | The first-boot setup screen, then the launcher (as described in the Pi manual on the site). |

## 8. The download page - ready

| # | Check | Expect |
|---|---|---|
| 8.1 | PC tab -> PC USB stick | "Flasher for Windows" is listed with the images, per channel, once published. The "Step by step" page's *Writing the stick* section starts with the flasher. |
| 8.2 | Every download link on every tab | Each one downloads, and its size matches the table. |
| 8.3 | Phone-width browser | No sideways scrolling. The tabs and tables stay readable. |

---

### Reporting

Send per item: pass/fail/blocked, the version, the logs named for it, and screenshots or photos of anything
unexpected. A failure is most useful with the steps that led to it and whether it happens again.
