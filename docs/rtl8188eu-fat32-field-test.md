# PlayStation Classic Wi-Fi and FAT32 USB field test

This fork keeps AutoBleem as the launcher. It does not include a store app, games, catalogs, Wi-Fi credentials, or kernel images.

## FAT32 USB safety

The boot checker now runs `fsck.fat` when a FAT32 volume's dirty bit survives a read-only remount, even when the previous session marker is absent. An interrupted shutdown can remove that marker before USB unmount, so the marker alone cannot prove the volume is clean. A failed filesystem check does not trigger a raw dirty-bit clear.

The PlayStation Classic Power option in this fork unmounts `/media` before requesting a full Linux shutdown. If unmount fails after ten attempts, it cancels shutdown and returns to the launcher. This changes the previous suspend-to-RAM behavior; the Power button caused a full boot on the tested console. The original `standby()` implementation remains in the source for comparison.

On one PlayStation Classic with AutoBleem 2.0 alpha1.1, a rear OTG powered hub, a FAT32 USB drive, and an RTL8188EU adapter, three boot/power-off cycles left the USB clean. Windows `chkdsk` found no file or folder errors after the third cycle. The adapter reconnected to Wi-Fi after a full boot. This is a field test of that setup, not a compatibility claim for every USB drive, hub, filesystem, or AutoBleem version.

The existing `Autobleem/bin/fsck/fsck.fat` and its license are already present in the official source payload. Keep them in any distribution of this change.
