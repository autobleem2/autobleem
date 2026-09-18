// lib_ableem - engine: where a game's title, publisher, year and player count come from. RetroArch's
// "Sony - PlayStation.rdb" first when there is one, the covers*.db databases otherwise - and the covers db
// is still asked for its PNG when the rdb answered, so a stick without a thumbnails tree keeps its art.
#pragma once

#include <string>

#include "cover_database.h"
#include "game_metadata.h"
#include "rdb_reader.h"

namespace ableem {

//******************
// MetadataLookup
//******************
// One object per thread that scans (the scan worker has its own, like its CoverDatabase before it - the
// sqlite handles must not be shared). Either source may be missing; a lookup with neither simply fails.
class MetadataLookup {
public:
    // coversDir: where covers{U,P,J}.db live; rdbFile: the PlayStation .rdb (an absent file is fine)
    MetadataLookup(const std::string &coversDir, const std::string &rdbFile);

    bool hasRdb() const { return rdb_.isValid(); }
    bool hasAnyCovers() const { return covers_.hasAnyRegion(); }
    CoverDatabase &covers() { return covers_; }
    const RdbReader &rdb() const { return rdb_; }

    // md.title is the rdb name without its trailing "(USA)" / "(Disc 1)" tags, md.recordName the whole
    // name (what a thumbnail file is called); md.lastRegion is "U"/"P"/"J" from the rdb's region, else
    // from the serial. md.bytes is the covers db's PNG when it has this serial, else empty.
    bool findBySerial(const std::string &serial, GameMetadata &md);
    // the rdb by its exact name, else the covers db by title
    bool findByTitle(const std::string &title, GameMetadata &md);

    // "Crash Bandicoot (USA)" -> "Crash Bandicoot": every trailing " (...)" group goes
    static std::string cleanTitle(const std::string &name);
    // RetroArch's region string ("USA", "Europe", "Japan", the PAL countries) -> "U"/"P"/"J", falling back
    // to the serial's prefix; "U" when neither says
    static std::string regionLetter(const std::string &rdbRegion, const std::string &serial);

private:
    bool fromRecord(const RdbReader::Record &rec, const std::string &serial, GameMetadata &md);

    RdbReader rdb_;
    CoverDatabase covers_;
};

} // namespace ableem
