// lib_ableem - engine: the CRC-32 of a file (the zip/PNG polynomial, what RetroArch's databases key their
// records by). Streamed, so a large image costs no memory; over a size limit the file is not read at all.
#pragma once

#include <cstdint>
#include <string>

namespace ableem {

//******************
// Crc32
//******************
struct Crc32 {
    // false when the file cannot be read or is larger than maxBytes (0 = no limit); crc is 0 then
    static bool ofFile(const std::string &path, uint32_t &crc, uint64_t maxBytes = 0);
    static uint32_t ofBytes(const std::string &bytes);
    // "%08X|crc" - how a playlist spells a CRC; "00000000|crc" for none
    static std::string playlistText(uint32_t crc);
};

} // namespace ableem
