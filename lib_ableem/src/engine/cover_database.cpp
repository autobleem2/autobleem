#include "ableem/engine/cover_database.h"
#include "ableem/engine/filesystem.h"

#include <iostream>
#include "ableem/engine/log.h"

using namespace std;

namespace ableem {

//*******************************
// CoverDatabase::CoverDatabase
//*******************************
CoverDatabase::CoverDatabase(const string &coversDir) {
    regionStr[0] = "U";
    regionStr[1] = "P";
    regionStr[2] = "J";

    for (int i = 0; i < regionCount; i++) {
        auto filename = coversDir + sep + "covers" + regionStr[i] + ".db";
        if (DirEntry::exists(filename)) {
            covers[i].reset(new GameDatabase());
            if (!covers[i]->open(filename)) {
                PLOG_WARNING << "failed to open database " << filename;
                covers[i].reset();
            }
        } else {
            PLOG_WARNING << "database file " << filename << " not found";
        }
    }
}

//*******************************
// CoverDatabase::~CoverDatabase
//*******************************
CoverDatabase::~CoverDatabase() {
    // the unique_ptrs close and delete the databases
}

//*******************************
// CoverDatabase::hasAnyRegion
//*******************************
bool CoverDatabase::hasAnyRegion() const {
    for (const auto &db : covers) {
        if (db != nullptr)
            return true;
    }
    return false;
}

//*******************************
// CoverDatabase::findBySerial
//*******************************
bool CoverDatabase::findBySerial(const string &serial, GameMetadata &md) {
    for (int i = 0; i < regionCount; i++) {
        GameDatabase *db = covers[i].get();
        if (db == nullptr)
            continue;

        if (db->findMetadataBySerial(serial, &md)) {
            md.lastRegion = regionStr[i];
            return true;
        }
    }
    return false;
}

//*******************************
// CoverDatabase::findByTitle
//*******************************
bool CoverDatabase::findByTitle(const string &title, GameMetadata &md) {
    for (int i = 0; i < regionCount; i++) {
        GameDatabase *db = covers[i].get();
        if (db == nullptr)
            continue;

        if (db->findMetadataByTitle(title, &md)) {
            md.lastRegion = regionStr[i];
            return true;
        }
    }
    return false;
}

} // namespace ableem
