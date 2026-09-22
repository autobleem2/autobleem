//
// FatDirtyFlag - the FAT/exFAT "volume dirty" bit, on synthetic boot sectors in image files.
//
#include "doctest/doctest.h"

#include "../support/temp_dir.h"

#include <ableem/engine/fat_dirty_flag.h>

#include <fstream>
#include <string>
#include <vector>

using ableem::FatDirtyFlag;
using std::string;
using std::vector;

namespace {

void put16(vector<unsigned char> &b, size_t at, unsigned v) {
    b[at] = v & 0xFF;
    b[at + 1] = (v >> 8) & 0xFF;
}
void put32(vector<unsigned char> &b, size_t at, unsigned long v) {
    for (int i = 0; i < 4; ++i)
        b[at + i] = (v >> (8 * i)) & 0xFF;
}

// a FAT32 volume: 512-byte sectors, 1 per cluster, 32 reserved, two FATs of 100 sectors, 70000 sectors
// -> 69768 clusters (>= 65525). The image covers both FATs' first entries.
vector<unsigned char> fat32Image() {
    vector<unsigned char> b((32 + 2 * 100) * 512 + 512, 0);
    b[0] = 0xEB;
    put16(b, 11, 512);
    b[13] = 1;
    put16(b, 14, 32);
    b[16] = 2;
    put16(b, 17, 0);
    put16(b, 19, 0);
    put32(b, 32, 70000);
    put16(b, 22, 0);
    put32(b, 36, 100);
    b[510] = 0x55;
    b[511] = 0xAA;
    for (int fat = 0; fat < 2; ++fat) {
        const size_t at = (32 + fat * 100) * 512;
        put32(b, at, 0x0FFFFFF8);
        put32(b, at + 4, 0x07FFFFFF); // ClnShutBit (bit 27) cleared, as a Windows tool leaves a dirty volume
    }
    return b;
}

// a FAT16 volume: 1 reserved sector, two FATs of 80 sectors, 512 root entries (32 sectors), 20000 sectors
// -> 19807 clusters
vector<unsigned char> fat16Image() {
    vector<unsigned char> b((1 + 2 * 80 + 32) * 512, 0);
    b[0] = 0xEB;
    put16(b, 11, 512);
    b[13] = 1;
    put16(b, 14, 1);
    b[16] = 2;
    put16(b, 17, 512);
    put16(b, 19, 20000);
    put16(b, 22, 80);
    b[510] = 0x55;
    b[511] = 0xAA;
    for (int fat = 0; fat < 2; ++fat) {
        const size_t at = (1 + fat * 80) * 512;
        put16(b, at, 0xFFF8);
        put16(b, at + 2, 0x7FFF);
    }
    return b;
}

vector<unsigned char> exfatImage() {
    vector<unsigned char> b(4096, 0);
    b[0] = 0xEB;
    const char *sig = "EXFAT   ";
    for (int i = 0; i < 8; ++i)
        b[3 + i] = sig[i];
    b[510] = 0x55;
    b[511] = 0xAA;
    return b;
}

string writeImage(TempDir &tmp, const string &name, const vector<unsigned char> &bytes) {
    string path = tmp.at(name);
    std::ofstream os(path, std::ios::binary);
    os.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    return path;
}

vector<unsigned char> readImage(const string &path) {
    std::ifstream is(path, std::ios::binary);
    return vector<unsigned char>((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE("FAT32: the dirty bit is byte 0x41 bit 0, and a clear sets FAT[1]'s ClnShutBit in both FATs") {
    TempDir tmp("fatflag32");
    string img = writeImage(tmp, "fat32.img", fat32Image());
    CHECK(FatDirtyFlag::kindOf(img) == FatDirtyFlag::Kind::Fat32);
    CHECK(FatDirtyFlag::status(img) == FatDirtyFlag::State::Clean);

    REQUIRE(FatDirtyFlag::set(img, true));
    CHECK(FatDirtyFlag::status(img) == FatDirtyFlag::State::Dirty);
    CHECK(readImage(img)[0x41] == 0x01);

    REQUIRE(FatDirtyFlag::set(img, false));
    CHECK(FatDirtyFlag::status(img) == FatDirtyFlag::State::Clean);
    vector<unsigned char> after = readImage(img);
    CHECK(after[0x41] == 0x00);
    CHECK(after[32 * 512 + 4 + 3] == 0x0F);         // FAT 1: bit 27 set -> 0x0FFFFFFF
    CHECK(after[(32 + 100) * 512 + 4 + 3] == 0x0F); // FAT 2 too
    CHECK(after[32 * 512 + 3] == 0x0F);             // FAT[0] untouched
}

TEST_CASE("FAT16: byte 0x25 and bit 15 of FAT[1]; the other bits of the state byte are kept") {
    TempDir tmp("fatflag16");
    vector<unsigned char> bytes = fat16Image();
    bytes[0x25] = 0x02; // the "surface scan" bit, which is not ours
    string img = writeImage(tmp, "fat16.img", bytes);
    CHECK(FatDirtyFlag::kindOf(img) == FatDirtyFlag::Kind::Fat16);
    CHECK(FatDirtyFlag::status(img) == FatDirtyFlag::State::Clean);

    REQUIRE(FatDirtyFlag::set(img, true));
    CHECK(readImage(img)[0x25] == 0x03);
    CHECK(FatDirtyFlag::status(img) == FatDirtyFlag::State::Dirty);
    REQUIRE(FatDirtyFlag::set(img, false));
    vector<unsigned char> after = readImage(img);
    CHECK(after[0x25] == 0x02);
    CHECK(after[1 * 512 + 2 + 1] == 0xFF); // FAT[1] high byte: bit 15 set
    CHECK(after[(1 + 80) * 512 + 2 + 1] == 0xFF);
}

TEST_CASE("exFAT: VolumeFlags bit 1, nothing else touched") {
    TempDir tmp("fatflagx");
    vector<unsigned char> bytes = exfatImage();
    bytes[106] = 0x01; // ActiveFat, kept
    string img = writeImage(tmp, "exfat.img", bytes);
    CHECK(FatDirtyFlag::kindOf(img) == FatDirtyFlag::Kind::ExFat);
    CHECK(FatDirtyFlag::status(img) == FatDirtyFlag::State::Clean);
    REQUIRE(FatDirtyFlag::set(img, true));
    CHECK(readImage(img)[106] == 0x03);
    CHECK(FatDirtyFlag::status(img) == FatDirtyFlag::State::Dirty);
    REQUIRE(FatDirtyFlag::set(img, false));
    CHECK(readImage(img)[106] == 0x01);
    CHECK(FatDirtyFlag::status(img) == FatDirtyFlag::State::Clean);
}

TEST_CASE("not a FAT volume: unreadable, and set() refuses") {
    TempDir tmp("fatflagno");
    string img = writeImage(tmp, "junk.img", vector<unsigned char>(1024, 0x5A));
    CHECK(FatDirtyFlag::kindOf(img) == FatDirtyFlag::Kind::Unknown);
    CHECK(FatDirtyFlag::status(img) == FatDirtyFlag::State::Unreadable);
    CHECK_FALSE(FatDirtyFlag::set(img, false));
    CHECK(FatDirtyFlag::status(tmp.at("missing.img")) == FatDirtyFlag::State::Unreadable);
}
