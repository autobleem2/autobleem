// lib_ableem - engine: the "volume dirty" flag of a FAT12/16/32 or exFAT volume, read and written on the
// raw device (or an image file) - what Windows looks at to offer "scan and fix" and what Linux's fat
// driver sets on an rw mount and clears on umount:
//   FAT32:    the boot sector's reserved byte at 0x41, bit 0 (fastfat's "CurrentHead" dirty bit); FAT[1]'s
//   FAT12/16: ... byte at 0x25, bit 0                  ClnShutBit (bit 27 / bit 15, 1 = clean) is set too
//   exFAT:    VolumeFlags at 106, bit 1 (VolumeDirty) - excluded from the boot checksum, so writable alone
// The console clears it itself after a standby with the stick unmounted (rc/selection.sh) and at boot for
// a stick pulled during Sony's own standby (rc/checkstick.sh); the kernel never clears a flag it found set
// at mount time (fat_set_state's sbi->dirty gate), so without this a stick stays "dirty" for ever once it
// was pulled once. Only ever used on an unmounted or read-only-mounted volume.
#pragma once

#include <string>

namespace ableem {

//******************
// FatDirtyFlag
//******************
struct FatDirtyFlag {
    enum class Kind { Unknown, Fat12, Fat16, Fat32, ExFat };
    enum class State { Clean, Dirty, Unreadable };

    // the volume's type from its boot sector; Unknown for anything that is not FAT/exFAT
    static Kind kindOf(const std::string &device);
    static State status(const std::string &device);
    // sets or clears the flag; false when the device cannot be read/written or is not FAT/exFAT
    static bool set(const std::string &device, bool dirty);
    static const char *kindName(Kind kind);
};

} // namespace ableem
