#include "ableem/engine/metadata_lookup.h"
#include "ableem/engine/filesystem.h"
#include "ableem/engine/serial_scanner.h"
#include "ableem/engine/strings.h"

#include <iostream>
#include "ableem/engine/log.h"

using namespace std;

namespace ableem {

//*******************************
// MetadataLookup::MetadataLookup
//*******************************
MetadataLookup::MetadataLookup(const string &coversDir, const string &rdbFile) : covers_(coversDir) {
    if (DirEntry::exists(rdbFile)) {
        rdb_.open(rdbFile);   // logs what it found, or why not
    } else {
        PLOG_INFO << "rdb: no " << rdbFile << " - game metadata comes from the covers databases only";
    }
}

//*******************************
// MetadataLookup::cleanTitle
//*******************************
string MetadataLookup::cleanTitle(const string &name) {
    string s = name;
    while (true) {
        auto pos = s.rfind(" (");
        if (pos == string::npos || s.empty() || s.back() != ')')
            break;
        s.erase(pos);
    }
    return s;
}

//*******************************
// MetadataLookup::regionLetter
//*******************************
string MetadataLookup::regionLetter(const string &rdbRegion, const string &serial) {
    if (rdbRegion == "Japan") return "J";
    if (rdbRegion == "USA") return "U";
    if (rdbRegion == "Europe") return "P";
    // the PAL countries the database names individually
    static const char *const pal[] = {"Australia", "France", "Germany", "Italy", "Spain", "Sweden", "Netherlands",
                                      "Norway", "Finland", "Denmark", "Portugal", "Russia", "Poland", "Greece", "UK"};
    for (const char *r : pal) {
        if (rdbRegion == r) return "P";
    }
    string fromSerial = SerialScanner::serialToRegion(serial);
    if (fromSerial == "Japan") return "J";
    if (fromSerial == "Europe-Aus") return "P";
    return "U";
}

//*******************************
// MetadataLookup::fromRecord
//*******************************
bool MetadataLookup::fromRecord(const RdbReader::Record &rec, const string &serial, GameMetadata &md) {
    md.title = cleanTitle(rec.name);
    md.recordName = rec.name;
    md.publisher = rec.publisher;
    Strings::cleanPublisherString(md.publisher);
    md.year = rec.releaseyear;
    md.players = rec.users > 0 ? rec.users : 1;
    md.serial = serial.empty() ? rec.serial : serial;   // what the disc says, not the database's suffixed form
    md.region = SerialScanner::serialToRegion(md.serial);
    md.lastRegion = regionLetter(rec.region, md.serial);
    md.bytes.clear();
    md.valid = true;

    // the covers db's PNG is still the cover when no thumbnails tree has one
    if (!md.serial.empty()) {
        GameMetadata fromDb;
        if (covers_.findBySerial(md.serial, fromDb))
            md.bytes = std::move(fromDb.bytes);
    }
    return true;
}

//*******************************
// MetadataLookup::findBySerial
//*******************************
bool MetadataLookup::findBySerial(const string &serial, GameMetadata &md) {
    if (rdb_.isValid()) {
        const RdbReader::Record *rec = rdb_.findBySerial(serial);
        if (rec != nullptr)
            return fromRecord(*rec, serial, md);
    }
    if (covers_.findBySerial(serial, md)) {
        md.recordName.clear();
        return true;
    }
    return false;
}

//*******************************
// MetadataLookup::findByTitle
//*******************************
bool MetadataLookup::findByTitle(const string &title, GameMetadata &md) {
    if (rdb_.isValid()) {
        const RdbReader::Record *rec = rdb_.findByName(title);
        if (rec != nullptr)
            return fromRecord(*rec, rec->serial, md);
    }
    if (covers_.findByTitle(title, md)) {
        md.recordName.clear();
        return true;
    }
    return false;
}

} // namespace ableem
