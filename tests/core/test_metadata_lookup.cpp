//
// MetadataLookup: the rdb in front of the covers databases, and what each source contributes.
//
#include "doctest/doctest.h"

#include "../support/rdb_builder.h"
#include "../support/temp_dir.h"

#include <ableem/engine/game_database.h>
#include <ableem/engine/metadata_lookup.h>

#include <string>

using ableem::GameMetadata;
using ableem::MetadataLookup;
using std::string;

namespace {

using namespace test_support;

// covers<region>.db with the real schema and one game whose cover is four bytes of "PNG"
void makeCoversDb(const TempDir &tmp, const string &region, const string &serial, const string &title) {
    ableem::GameDatabase db;
    REQUIRE(db.open(tmp.at("db/covers" + region + ".db")));
    REQUIRE(db.execute("CREATE TABLE GAME (ID INTEGER NOT NULL UNIQUE, TITLE TEXT NOT NULL, PUBLISHER TEXT NOT NULL, "
                       "RELEASE INTEGER NOT NULL, PLAYERS INTEGER NOT NULL, COVER BLOB, PRIMARY KEY(ID))",
                       "create GAME"));
    REQUIRE(db.execute("CREATE TABLE SERIALS (SERIAL TEXT NOT NULL, GAME INTEGER NOT NULL, PRIMARY KEY(SERIAL))",
                       "create SERIALS"));
    string insert = "INSERT INTO GAME VALUES (1, '" + title + "', 'Db Publisher', 1999, 2, X'89504E47')";
    REQUIRE(db.execute(insert.c_str(), "insert GAME"));
    insert = "INSERT INTO SERIALS VALUES ('" + serial + "', 1)";
    REQUIRE(db.execute(insert.c_str(), "insert SERIALS"));
}

string makeRdbFile(const TempDir &tmp) {
    Bytes records;
    appendGameRecord(records, "Crash Bandicoot (USA)", "SCUS-94900", "USA", "Sony Computer Entertainment.", 1996, 1);
    appendGameRecord(records, "Tekken 3 (Europe) (Disc 1)", "SCES-01237", "Europe", "Namco", 1998, 2);
    appendGameRecord(records, "Biohazard (Japan)", "SLPS-00222", "Japan", "Capcom", 1996, 1);
    records.push_back(0xc0);
    return writeRdb(tmp, "Sony - PlayStation.rdb", makeRdb(0, records));
}

} // namespace

TEST_CASE("cleanTitle drops every trailing tag group and nothing else") {
    CHECK(MetadataLookup::cleanTitle("Crash Bandicoot (USA)") == "Crash Bandicoot");
    CHECK(MetadataLookup::cleanTitle("Tekken 3 (Europe) (Disc 1)") == "Tekken 3");
    CHECK(MetadataLookup::cleanTitle("Metal Gear Solid (USA) (Disc 2) (Rev 1)") == "Metal Gear Solid");
    CHECK(MetadataLookup::cleanTitle("Puzzle & Action") == "Puzzle & Action");
    CHECK(MetadataLookup::cleanTitle("Vib-Ribbon (Europe) Special") == "Vib-Ribbon (Europe) Special"); // not trailing
    CHECK(MetadataLookup::cleanTitle("") == "");
}

TEST_CASE("regionLetter: the rdb's region first, the PAL countries too, the serial when it says nothing") {
    CHECK(MetadataLookup::regionLetter("USA", "") == "U");
    CHECK(MetadataLookup::regionLetter("Europe", "") == "P");
    CHECK(MetadataLookup::regionLetter("Germany", "") == "P");
    CHECK(MetadataLookup::regionLetter("Japan", "") == "J");
    CHECK(MetadataLookup::regionLetter("", "SLPS-00222") == "J");
    CHECK(MetadataLookup::regionLetter("", "SLES-00001") == "P");
    CHECK(MetadataLookup::regionLetter("", "SLUS-00001") == "U");
    CHECK(MetadataLookup::regionLetter("World", "") == "U");
}

TEST_CASE("with an rdb the metadata comes from it: cleaned title, region letter, players, canonical name") {
    TempDir tmp("meta");
    tmp.makeSubDir("db");
    MetadataLookup lookup(tmp.at("db"), makeRdbFile(tmp));
    REQUIRE(lookup.hasRdb());
    CHECK_FALSE(lookup.hasAnyCovers());

    GameMetadata md;
    REQUIRE(lookup.findBySerial("SCES-01237", md));
    CHECK(md.title == "Tekken 3");
    CHECK(md.recordName == "Tekken 3 (Europe) (Disc 1)");
    CHECK(md.publisher == "Namco");
    CHECK(md.year == 1998);
    CHECK(md.players == 2);
    CHECK(md.serial == "SCES-01237");
    CHECK(md.lastRegion == "P");
    CHECK(md.bytes.empty()); // no covers db, no PNG
    CHECK(md.valid);

    GameMetadata crash;
    REQUIRE(lookup.findBySerial("SCUS-94900", crash));
    CHECK(crash.publisher == "Sony Computer Entertainment"); // the trailing "." cleaned like the db's
    CHECK(crash.lastRegion == "U");

    GameMetadata none;
    CHECK_FALSE(lookup.findBySerial("SLUS-99999", none));
}

TEST_CASE("the rdb answers by exact name too, and the covers db by title when it cannot") {
    TempDir tmp("meta");
    tmp.makeSubDir("db");
    makeCoversDb(tmp, "U", "SLUS-00001", "Db Only Game");
    MetadataLookup lookup(tmp.at("db"), makeRdbFile(tmp));

    GameMetadata md;
    REQUIRE(lookup.findByTitle("Biohazard (Japan)", md));
    CHECK(md.title == "Biohazard");
    CHECK(md.lastRegion == "J");

    GameMetadata fromDb;
    REQUIRE(lookup.findByTitle("Db Only Game", fromDb));
    CHECK(fromDb.recordName.empty());
    CHECK(fromDb.lastRegion == "U");
}

TEST_CASE("without an rdb the covers db answers as before; with both, the rdb's text and the db's PNG") {
    TempDir tmp("meta");
    tmp.makeSubDir("db");
    makeCoversDb(tmp, "P", "SCES-01237", "Tekken 3 from the db");

    MetadataLookup dbOnly(tmp.at("db"), tmp.at("no-such.rdb"));
    CHECK_FALSE(dbOnly.hasRdb());
    REQUIRE(dbOnly.hasAnyCovers());
    GameMetadata md;
    REQUIRE(dbOnly.findBySerial("SCES-01237", md));
    CHECK(md.title == "Tekken 3 from the db");
    CHECK(md.publisher == "Db Publisher");
    CHECK(md.lastRegion == "P");
    CHECK(md.recordName.empty());
    CHECK(md.bytes.size() == 4);

    MetadataLookup both(tmp.at("db"), makeRdbFile(tmp));
    GameMetadata combined;
    REQUIRE(both.findBySerial("SCES-01237", combined));
    CHECK(combined.title == "Tekken 3"); // the rdb's
    CHECK(combined.publisher == "Namco");
    CHECK(combined.bytes.size() == 4); // the db's cover still comes along
}
