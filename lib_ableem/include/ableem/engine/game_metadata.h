// lib_ableem - engine: what the metadata sources know about a game (see MetadataLookup::findBySerial).
#pragma once

#include <string>
#include <vector>

namespace ableem {

//******************
// GameMetadata
//******************
class GameMetadata {
public:
    std::string title;
    std::string recordName;     // the libretro-database name ("Crash Bandicoot (USA)") when the rdb answered - a thumbnail's file name; else ""
    std::string publisher;
    int year = 0;
    std::string serial;
    std::string region;         // from the serial: "US", "Europe-Aus", "Japan"
    int players = 0;
    std::vector<char> bytes;    // the cover PNG, empty if none
    bool valid = false;

    std::string lastRegion = "U";   // which covers db answered: "U", "P" or "J"

    void clearCover() { bytes.clear(); }
};

} // namespace ableem
