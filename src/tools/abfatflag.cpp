// abfatflag - the "volume dirty" flag of a FAT/exFAT volume, for the console's rc scripts.
//
//   abfatflag DEVICE          prints "clean" or "dirty" (and the volume type); exit 0 clean, 1 dirty, 2 error
//   abfatflag DEVICE clean    clears the flag
//   abfatflag DEVICE dirty    sets it
//
// Why the console needs it: Linux's fat driver sets the flag on every rw mount and clears it on umount -
// unless the volume was already dirty when it was mounted, in which case it never touches it again. A
// stick pulled during Sony's boot standby (mounted rw by usb_watch, nothing of ours has run yet) comes
// back dirty and would stay dirty for ever, Windows offering a scan at every insertion. rc/checkstick.sh
// clears it at boot when the previous session ended cleanly, and rc/selection.sh after the standby's
// umount. Use it on an unmounted or read-only-mounted volume only. Engine only - no SDL.
#include <ableem/engine/fat_dirty_flag.h>

#include <cstdio>
#include <cstring>

using ableem::FatDirtyFlag;

int main(int argc, char **argv) {
    if (argc < 2 || argc > 3) {
        std::fprintf(stderr, "usage: abfatflag DEVICE [clean|dirty]\n");
        return 2;
    }
    const char *device = argv[1];
    const FatDirtyFlag::Kind kind = FatDirtyFlag::kindOf(device);
    if (kind == FatDirtyFlag::Kind::Unknown) {
        std::fprintf(stderr, "abfatflag: %s is not a FAT or exFAT volume (or cannot be read)\n", device);
        return 2;
    }
    if (argc == 3) {
        const bool dirty = std::strcmp(argv[2], "dirty") == 0;
        if (!dirty && std::strcmp(argv[2], "clean") != 0) {
            std::fprintf(stderr, "usage: abfatflag DEVICE [clean|dirty]\n");
            return 2;
        }
        if (!FatDirtyFlag::set(device, dirty)) {
            std::fprintf(stderr, "abfatflag: cannot write %s\n", device);
            return 2;
        }
    }
    const FatDirtyFlag::State state = FatDirtyFlag::status(device);
    std::printf("%s %s\n", state == FatDirtyFlag::State::Dirty ? "dirty" : "clean", FatDirtyFlag::kindName(kind));
    return state == FatDirtyFlag::State::Dirty ? 1 : 0;
}
