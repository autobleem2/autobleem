# ABFlashKit - developer context

The console's kernel installer, an App in `Apps/abflashkit` the launcher starts from its Apps set. Three
actions on one status bar: **Flash Kernel** (a recovery backup of the console's partitions to the USB
stick's `LBOOT.EPB` if there is none yet, the backup and the kernel image validated, the bootloader's
recovery flag set, `kernel/boot.img` written to the boot partition, the AutoBleem rootfs overlay unpacked
onto the data partition by `kernel/install_payload.sh`, the flag cleared, a reboot), **Full backup** (all
four partitions, rootfs included, to `LBOOT.EPB`) and **Restore Mode** (checks the backup is ours and is
the stock firmware, sets the recovery flag and reboots - Sony's own recovery then restores the console
from the file on the stick). Ported on 2026-09-18 from the 2020 standalone tool (a fork of the old AutoBleem
GUI, in git history under `psctools/abflashkit`) onto `lib_ableem` + `ab_core` + `ab_classic`; the root
CLAUDE.md covers those and the build.

**This tool writes to the console's flash.** Everything that does is in `ConsoleFlasher`, compiled on every
host but constructed only when `AB_DEBUG_HOST` is not defined; a dev host gets `FakeFlasher`, which touches
no device. Keep it that way.

## What the console provides

- `/dev/disk/by-partlabel/{BOOTIMG1,ROOTFS1,USRDATA,TEE1}` - read as files into the backup; `MISC` and
  `BOOTIMG1` are written with `dd` (the recovery flag from `kernel/recovery-{on,off}.img`, 16 bytes; the
  kernel from `kernel/boot.img`).
- `image_verify_tool` - the firmware's own checker for an `LBOOT.EPB`; "fail" in its output is a no.
- `kernel/install_payload.sh` (a console script in the payload, run with `bash`), `sync`, `killall -9
  autobleem-gui`, `systemctl stop weston`, `systemctl reboot`.
- `/sys/class/leds/{red,green}/brightness` - the power LED, the flasher's progress indicator (green idle,
  blinking green backing up, red while the flag is set and the kernel written, off just before the reboot).
- The backup lives at `<usb root>/LBOOT.EPB`; `<usb root>/validlboot`, when present, skips the restore's
  inspection of it. `/tmp/lbootzip` is where a backup is unpacked for that inspection.
- `/usr/bin/bleemsync_service` or `/etc/systemd/system/project_eris.service` present = another custom
  firmware: a flash is refused ("This PSC has not been restored to original condition").

## The backup format

`LbootBackup`: a plain zip (deflate, no zip64 - the recovery reads plain zip, which is why `ZipWriter`
gives miniz each entry's size up front) of the partitions as `boot.img`, `rootfs.ext4` (full backup
only), `userdata.ext4`, `tz.img`, followed by the 4096-byte trailer in `lboot_signature.h` - "RAWCAW", a
signature block Sony's recovery accepts (madmonkey's), padded with "autobleem" lines. **The trailer is byte
for byte the 2020 tool's; never regenerate it.** Its last nine bytes ("autobleem") are how
`isAutoBleemBackup()` tells our backup from someone else's. `inspect()` md5s an unpacked backup against the
stock firmware's `boot.img` (`28ce5f6d…`) and `rootfs.ext4` (`ca710a12…`): a modified one, or one
without a rootfs (an ABFK 1.0a backup), gets a warning question before a restore.

The vendored miniz needed one patch for this: its backward scan for the zip end record went wrong when
the record was within 4 KB of the file start (a small zip plus the trailer) - marked `AutoBleem:` in
`lib_ableem/third_party/miniz/miniz.c`, regression test in `tests/core/test_zip_writer.cpp`.

## Layout

```
src/core/       abflashkit_core (SDL-free, links ab_core; the tests link it)
  lboot_signature.h   the trailer bytes
  lboot_backup.*      LbootBackup - the partition lists, the trailer, isAutoBleemBackup, inspect
  flasher.*           Flasher interface; ConsoleFlasher (the commands above, through System::execUnixCommand,
                      the zip through ableem::ZipWriter, md5 through ableem::Md5) and FakeFlasher (every step
                      logged and delayed 1.5 s; a backup written for real from stand-in files under
                      <app dir>/fake/partitions; validation always passes; the reboot only sets a flag)
  led.*               Led interface; SysfsLed (a std::thread blinker, joined by the destructor - the 2020
                      tool forked one it never reaped) and NullLed (logs)
  flash_actions.*     FlashKitActions - flash()/fullBackup()/restore() step by step, through a FlashUi
                      (status line, confirm, wait) so the tests run them to the end against the fakes
src/screens/gui_flashkit_main.*  the one screen: the status bar, and the FlashUi over gui->drawText/GuiConfirm
src/abflashkit_app.*  AbFlashKit : AppBase - holds the Flasher and the Led, knows the paths
src/main.cpp          EnvironmentSetup::forTool, the flasher/led choice, logs
resources/            app.ini, run.sh, readme.txt, icon.png, lang/ (Polski from the 2020 tool)
```
The `kernel/` payload (`boot.img` 6.8 MB, `abrootfs.tgz` 22 MB, the recovery images, `install_payload.sh`,
the md5 files) is **not** in `resources/` - it lives only in `payload/Apps/abflashkit/kernel/`, release
artefacts that `make_psc.sh` leaves alone. `kernel/boot.md5` may be a bare hash or `md5sum`'s
`<hash>  boot.img`; `abrootfs.md5` is not checked (it never was).

## Theme and language

Drawn with the main GUI's theme through `AppBase` (the 2020 tool had its own `background.jpg`, button PNGs
and `zrnic.ttf` - gone). The language comes from the main GUI's `config.ini`; the tool's `lang/` is loaded
over the main file (`Lang::loadMore`). Regenerate `English.txt` with
`python tools/lang_tools.py --src-dir apps/abflashkit/src --lang-dir apps/abflashkit/resources/lang extract`
(then `update`); `make_win.sh` validates it.

## Build, run, test

Part of the root build: `make_win.sh` builds `abflashkit.exe` and runs `tests/apps/test_abflashkit_core.cpp`
(the three sequences with a scripted `FlashUi`, the backup round trip, the kernel check, the LED). `make_psc.sh`
builds it for the console, checks it, packs it and copies the binary plus `resources/` into
`payload/Apps/abflashkit/` (binary name `abflashkit` since the port; `run.sh` matches). Not built for the Pi.

Visual test on Windows: `python tools/make_usb.py usb` stages `usb/Apps/abflashkit/`;
`powershell -File tools\win_drive.ps1 -Usb <usb> -Tool abflashkit -Sequence "s;8;t;4;x;3;x;3;x;12"` does a
full backup, a restore (three confirmations, then the fake "reboot" closes the tool) - logs in
`usb/System/Logs/abflashkit.log`. There is no `kernel/` in the staged folder, so a Flash on the dev host
reports "Invalid backup or invalid kernel image" and "reboots"; drop a `kernel/boot.img` + `boot.md5` into
`usb/Apps/abflashkit/` to see the whole flash sequence.

## Behaviour changes vs the 2020 tool

- A confirmation before a Full backup overwrites the existing `LBOOT.EPB` (it is what a restore uses),
  and before a Restore sets the recovery flag and reboots (the point of no return). Neither was asked.
- A backup that fails half way is deleted, so the next flash does not take it for a good one.
- The kernel md5 and the backup's contents are checked in-process (`ableem::Md5`), not through `md5sum`.
- The blinker is a thread, not an orphaned fork; `systemctl reboot` and the rest are unchanged.
- The dev host runs against a fake and never runs `dd`, `systemctl` or `image_verify_tool`; the 2020 tool
  ran them all on x86 (a Cross would have rebooted the machine).
