// lib_ableem - engine: the three regional cover-art databases (coversU.db, coversP.db, coversJ.db) and the
// metadata lookups over them. Any of the three may be missing.
#pragma once

#include <memory>
#include <string>

#include "game_database.h"
#include "game_metadata.h"

namespace ableem {

//******************
// CoverDatabase
//******************
class CoverDatabase {
public:
    static const int regionCount = 3;
    std::unique_ptr<GameDatabase> covers[regionCount]; // U, P, J. nullptr when that region's covers db is not installed
    std::string regionStr[regionCount];                // "U", "P", "J"

    explicit CoverDatabase(
        const std::string &coversDir); // opens <coversDir>/covers<region>.db for every region present
    ~CoverDatabase();

    bool hasAnyRegion() const;

    // asks each region in turn (U, P, J); md.lastRegion says which one answered
    bool findBySerial(const std::string &serial, GameMetadata &md);
    bool findByTitle(const std::string &title, GameMetadata &md);
};

} // namespace ableem
