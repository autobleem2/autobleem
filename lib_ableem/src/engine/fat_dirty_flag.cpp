#include "ableem/engine/fat_dirty_flag.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace std;

namespace ableem {

namespace {

uint16_t le16(const unsigned char *p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}
uint32_t le32(const unsigned char *p) {
    return static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24));
}

struct Layout {
    FatDirtyFlag::Kind kind = FatDirtyFlag::Kind::Unknown;
    long stateOffset = 0;   // the byte with the dirty bit
    unsigned char mask = 0; // its bit
    // FAT12/16/32 only: where FAT[1] is in every FAT copy, and its clean-shutdown bit
    long fatOffset = 0;
    long fatSize = 0;
    int fatCount = 0;
    uint32_t cleanBit = 0;
};

// the type is decided by the cluster count, as the specification says - not by the "FAT32" label
Layout layoutOf(const unsigned char *bs) {
    Layout l;
    if (memcmp(bs + 3, "EXFAT   ", 8) == 0) {
        l.kind = FatDirtyFlag::Kind::ExFat;
        l.stateOffset = 106; // VolumeFlags, 2 bytes little-endian; bit 1 = VolumeDirty
        l.mask = 0x02;
        return l;
    }
    const uint32_t bytesPerSector = le16(bs + 11);
    const uint32_t sectorsPerCluster = bs[13];
    const uint32_t reserved = le16(bs + 14);
    const uint32_t fats = bs[16];
    const uint32_t rootEntries = le16(bs + 17);
    uint32_t totalSectors = le16(bs + 19);
    if (totalSectors == 0)
        totalSectors = le32(bs + 32);
    uint32_t fatSize = le16(bs + 22);
    if (fatSize == 0)
        fatSize = le32(bs + 36);
    if (bytesPerSector < 512 || (bytesPerSector & (bytesPerSector - 1)) != 0 || sectorsPerCluster == 0 || fats == 0 ||
        fatSize == 0 || totalSectors == 0 || bs[510] != 0x55 || bs[511] != 0xAA)
        return l;
    const uint32_t rootDirSectors = (rootEntries * 32 + bytesPerSector - 1) / bytesPerSector;
    const uint32_t used = reserved + fats * fatSize + rootDirSectors;
    if (used >= totalSectors)
        return l;
    const uint32_t clusters = (totalSectors - used) / sectorsPerCluster;
    l.fatOffset = static_cast<long>(reserved) * bytesPerSector;
    l.fatSize = static_cast<long>(fatSize) * bytesPerSector;
    l.fatCount = static_cast<int>(fats);
    l.mask = 0x01;
    if (clusters < 4085) {
        l.kind = FatDirtyFlag::Kind::Fat12;
        l.stateOffset = 0x25;
        l.cleanBit = 0; // FAT12 has no clean-shutdown bit
    } else if (clusters < 65525) {
        l.kind = FatDirtyFlag::Kind::Fat16;
        l.stateOffset = 0x25;
        l.cleanBit = 0x8000;
    } else {
        l.kind = FatDirtyFlag::Kind::Fat32;
        l.stateOffset = 0x41;
        l.cleanBit = 0x08000000;
    }
    return l;
}

bool readBootSector(FILE *f, vector<unsigned char> &bs) {
    bs.assign(512, 0);
    return fseek(f, 0, SEEK_SET) == 0 && fread(bs.data(), 1, 512, f) == 512;
}

} // namespace

//*******************************
// FatDirtyFlag::kindOf
//*******************************
FatDirtyFlag::Kind FatDirtyFlag::kindOf(const string &device) {
    FILE *f = fopen(device.c_str(), "rb");
    if (!f)
        return Kind::Unknown;
    vector<unsigned char> bs;
    const bool ok = readBootSector(f, bs);
    fclose(f);
    return ok ? layoutOf(bs.data()).kind : Kind::Unknown;
}

//*******************************
// FatDirtyFlag::status
//*******************************
FatDirtyFlag::State FatDirtyFlag::status(const string &device) {
    FILE *f = fopen(device.c_str(), "rb");
    if (!f)
        return State::Unreadable;
    vector<unsigned char> bs;
    const bool ok = readBootSector(f, bs);
    fclose(f);
    if (!ok)
        return State::Unreadable;
    const Layout l = layoutOf(bs.data());
    if (l.kind == Kind::Unknown)
        return State::Unreadable;
    return (bs[l.stateOffset] & l.mask) ? State::Dirty : State::Clean;
}

//*******************************
// FatDirtyFlag::set
//*******************************
bool FatDirtyFlag::set(const string &device, bool dirty) {
    FILE *f = fopen(device.c_str(), "r+b");
    if (!f)
        return false;
    vector<unsigned char> bs;
    if (!readBootSector(f, bs)) {
        fclose(f);
        return false;
    }
    const Layout l = layoutOf(bs.data());
    if (l.kind == Kind::Unknown) {
        fclose(f);
        return false;
    }
    unsigned char byte = bs[l.stateOffset];
    byte = dirty ? static_cast<unsigned char>(byte | l.mask) : static_cast<unsigned char>(byte & ~l.mask);
    bool ok = fseek(f, l.stateOffset, SEEK_SET) == 0 && fwrite(&byte, 1, 1, f) == 1;
    // FAT[1]'s clean-shutdown bit in every FAT copy goes with a clear: Linux leaves it alone (so a volume
    // only ever mounted there still has it), it is set here in case a Windows tool cleared it
    if (ok && !dirty && l.cleanBit != 0) {
        const size_t entrySize = l.kind == Kind::Fat32 ? 4 : 2; // FAT[1] is the second entry
        for (int i = 0; i < l.fatCount && ok; ++i) {
            const long at = l.fatOffset + i * l.fatSize + static_cast<long>(entrySize);
            unsigned char e[4] = {0, 0, 0, 0};
            ok = fseek(f, at, SEEK_SET) == 0 && fread(e, 1, entrySize, f) == entrySize;
            if (!ok)
                break;
            const uint32_t entry = le32(e) | l.cleanBit;
            e[0] = static_cast<unsigned char>(entry & 0xFF);
            e[1] = static_cast<unsigned char>((entry >> 8) & 0xFF);
            e[2] = static_cast<unsigned char>((entry >> 16) & 0xFF);
            e[3] = static_cast<unsigned char>((entry >> 24) & 0xFF);
            ok = fseek(f, at, SEEK_SET) == 0 && fwrite(e, 1, entrySize, f) == entrySize;
        }
    }
    ok = (fflush(f) == 0) && ok;
    fclose(f);
    return ok;
}

//*******************************
// FatDirtyFlag::kindName
//*******************************
const char *FatDirtyFlag::kindName(Kind kind) {
    switch (kind) {
    case Kind::Fat12:
        return "FAT12";
    case Kind::Fat16:
        return "FAT16";
    case Kind::Fat32:
        return "FAT32";
    case Kind::ExFat:
        return "exFAT";
    default:
        return "unknown";
    }
}

} // namespace ableem
