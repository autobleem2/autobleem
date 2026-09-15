// lib_ableem - engine: what the cover databases know about a game (see CoverDatabase::findBySerial).
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
