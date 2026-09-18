// lib_ableem - engine: RetroArch's libretro-database files (.rdb), read whole into memory and indexed by
// serial, by name, by CRC and by ROM file name. "Sony - PlayStation.rdb" is what the scanner takes a game's
// title, publisher, year and player count from when a RetroArch tree is around (the covers*.db lookups are
// the fallback); the other systems' databases are what the ROM scanner identifies a file by.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace ableem {

//******************
// RdbReader
//******************
// The file is a 16-byte header - "RARCHDB\0" and a big-endian offset to the metadata block (0 when the
// records simply run to the end) - followed by one rmsgpack map per game, terminated by a NIL. Only the
// rmsgpack subset the PS1 database actually uses is decoded (fix/8/16/32 strings and bins, unsigned and
// signed ints, nil/bool, and enough of arrays/maps to skip a value); unknown keys are skipped.
// Ported from AutoBleem-NG (autobleem/code/engine/rdb_reader.*).
class RdbReader {
public:
    struct Record {
        std::string name;   // the canonical libretro name, e.g. "Crash Bandicoot (USA)" - also the thumbnail file name
        std::string region; // as the database spells it: "USA", "Europe", "Japan", ...
        std::string serial; // "SLUS-00593", sometimes with a suffix: "SLUS-01251GH", "SLUS-00594-1"
        std::string publisher;
        std::string developer;
        std::string genre;
        int releaseyear = 0;
        int releasemonth = 0;
        int users = 0;       // players
        uint32_t crc = 0;    // of the ROM (a cartridge image, or an arcade set's whole archive); 0 = none
        uint64_t size = 0;   // its size in bytes; 0 = none
        std::string romName; // "Adventures of Lolo (USA).nes", an arcade set's "mslug.zip"
    };

    // reads the whole file; false (and isValid() false) for a missing, truncated or malformed one
    bool open(const std::string &path);
    bool isValid() const { return valid_; }
    size_t size() const { return records_.size(); }

    // an exact serial, else the first record whose serial is this one followed by a non-digit (a
    // re-release suffix or a disc separator), so "SLUS-01251" finds "SLUS-01251GH" but not "SLUS-012510"
    const Record *findBySerial(const std::string &serial) const;
    // exact only
    const Record *findByName(const std::string &name) const;
    // the first record with this CRC (nullptr for 0); the first with this rom_name, exact
    const Record *findByCrc(uint32_t crc) const;
    const Record *findByRomName(const std::string &romName) const;

private:
    std::vector<Record> records_;
    std::unordered_map<std::string, size_t> bySerial_;
    std::unordered_map<std::string, size_t> byName_;
    std::unordered_map<uint32_t, size_t> byCrc_;
    std::unordered_map<std::string, size_t> byRomName_;
    bool valid_ = false;
};

} // namespace ableem
