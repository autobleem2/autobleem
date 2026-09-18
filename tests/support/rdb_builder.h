//
// A RetroArch .rdb built byte by byte: rmsgpack is simple enough that the parser's edge cases can be
// pinned without checking a real 1 MB database in. Shared by the RdbReader and MetadataLookup suites.
//
#pragma once

#include "doctest/doctest.h"
#include "temp_dir.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace test_support {

typedef std::vector<unsigned char> Bytes;

inline void appendU64BE(Bytes &out, uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8)
        out.push_back(static_cast<unsigned char>((value >> shift) & 0xff));
}

inline void appendString(Bytes &out, const std::string &value) { // fixstr: up to 31 bytes
    REQUIRE(value.size() < 32u);
    out.push_back(static_cast<unsigned char>(0xa0 | value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

inline void appendBin(Bytes &out, const std::string &value) { // bin8 - how the real database stores serials
    out.push_back(0xc4);
    out.push_back(static_cast<unsigned char>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

inline void appendUint16(Bytes &out, unsigned value) {
    out.push_back(0xcd);
    out.push_back(static_cast<unsigned char>((value >> 8) & 0xff));
    out.push_back(static_cast<unsigned char>(value & 0xff));
}

// a game record as the PlayStation database has them (region, publisher, year, players)
inline void appendGameRecord(Bytes &out, const std::string &name, const std::string &serial, const std::string &region,
                             const std::string &publisher, unsigned year, unsigned users) {
    out.push_back(0x86); // fixmap, 6 entries
    appendString(out, "name");
    appendString(out, name);
    appendString(out, "region");
    appendString(out, region);
    appendString(out, "serial");
    appendBin(out, serial);
    appendString(out, "publisher");
    appendString(out, publisher);
    appendString(out, "releaseyear");
    appendUint16(out, year);
    appendString(out, "users");
    out.push_back(static_cast<unsigned char>(users));
}

// a game record as the cartridge databases have them: rom_name, a 4-byte big-endian crc, size
inline void appendRomRecord(Bytes &out, const std::string &name, const std::string &romName, uint32_t crc,
                            const std::string &publisher = "", unsigned year = 0, unsigned users = 0) {
    out.push_back(0x86); // fixmap, 6 entries
    appendString(out, "name");
    appendString(out, name);
    appendString(out, "rom_name");
    appendString(out, romName);
    appendString(out, "crc");
    std::string crcBytes;
    for (int shift = 24; shift >= 0; shift -= 8)
        crcBytes += static_cast<char>((crc >> shift) & 0xff);
    appendBin(out, crcBytes);
    appendString(out, "publisher");
    appendString(out, publisher);
    appendString(out, "releaseyear");
    appendUint16(out, year);
    appendString(out, "users");
    out.push_back(static_cast<unsigned char>(users));
}

inline Bytes makeRdb(uint64_t metadataOffset, const Bytes &records, const Bytes &metadata = Bytes()) {
    Bytes out;
    const unsigned char magic[] = {'R', 'A', 'R', 'C', 'H', 'D', 'B', '\0'};
    out.insert(out.end(), magic, magic + sizeof(magic));
    appendU64BE(out, metadataOffset);
    out.insert(out.end(), records.begin(), records.end());
    out.insert(out.end(), metadata.begin(), metadata.end());
    return out;
}

inline std::string writeRdb(const TempDir &tmp, const std::string &name, const Bytes &data) {
    std::string path = tmp.at(name);
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(data.data()), data.size());
    REQUIRE(out.good());
    return path;
}

} // namespace test_support
