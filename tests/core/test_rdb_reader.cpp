//
// RdbReader: RetroArch's .rdb files, built here byte by byte (rmsgpack is simple enough) so the parser's
// edge cases - the two header forms, the NIL sentinel, serial suffix matching - are pinned without
// checking a real 1 MB database in. AutoBleem-NG's rdb_reader_test, on doctest.
//
#include "doctest/doctest.h"

#include "../support/rdb_builder.h"
#include "../support/temp_dir.h"

#include <ableem/engine/rdb_reader.h>

#include <string>

using ableem::RdbReader;
using std::string;

namespace {

using namespace test_support;

// a full record, with an unknown key the reader has to skip over
void appendRecord(Bytes &out) {
    out.push_back(0x88); // fixmap, 8 entries
    appendString(out, "name");
    appendString(out, "Puzzle & Action (USA)");
    appendString(out, "region");
    appendString(out, "USA");
    appendString(out, "serial");
    appendBin(out, "SLUS-12345-01");
    appendString(out, "publisher");
    appendString(out, "Example Co., Ltd.");
    appendString(out, "developer");
    appendString(out, "Example Dev");
    appendString(out, "releaseyear");
    appendUint16(out, 1997);
    appendString(out, "users");
    out.push_back(2);
    appendString(out, "crc"); // a 9-byte "crc" is not a CRC, but the reader must get past it
    appendBin(out, "crc-bytes");
}

void appendSimpleRecord(Bytes &out, const string &name, const string &serial) {
    out.push_back(0x82); // fixmap, 2 entries
    appendString(out, "name");
    appendString(out, name);
    appendString(out, "serial");
    appendString(out, serial);
}

} // namespace

TEST_CASE("a database with a zero metadata offset is read to the NIL sentinel") {
    TempDir tmp("rdb");
    Bytes records;
    appendRecord(records);
    records.push_back(0xc0); // nil sentinel

    RdbReader reader;
    REQUIRE(reader.open(writeRdb(tmp, "zero_offset.rdb", makeRdb(0, records))));
    CHECK(reader.isValid());
    CHECK(reader.size() == 1u);

    const RdbReader::Record *byName = reader.findByName("Puzzle & Action (USA)");
    REQUIRE(byName != nullptr);
    CHECK(byName->publisher == "Example Co., Ltd.");
    CHECK(byName->developer == "Example Dev");
    CHECK(byName->region == "USA");
    CHECK(byName->releaseyear == 1997);
    CHECK(byName->users == 2);

    const RdbReader::Record *bySerial = reader.findBySerial("SLUS-12345"); // prefix of "SLUS-12345-01"
    REQUIRE(bySerial != nullptr);
    CHECK(bySerial->name == "Puzzle & Action (USA)");
}

TEST_CASE("records stop at the metadata offset") {
    TempDir tmp("rdb");
    Bytes records;
    appendRecord(records);
    records.push_back(0xc0);
    const uint64_t metadataOffset = 16 + records.size();

    RdbReader reader;
    REQUIRE(reader.open(writeRdb(tmp, "with_metadata.rdb", makeRdb(metadataOffset, records, {0xff, 0xff, 0xff}))));
    CHECK(reader.size() == 1u);
}

TEST_CASE("a file that is not an rdb, or is missing, leaves the reader invalid") {
    TempDir tmp("rdb");
    RdbReader reader;
    CHECK_FALSE(reader.open(writeRdb(tmp, "bad_magic.rdb", {'B', 'A', 'D'})));
    CHECK_FALSE(reader.isValid());
    CHECK_FALSE(reader.open(tmp.at("missing.rdb")));
    CHECK_FALSE(reader.isValid());
    CHECK(reader.findBySerial("SLUS-00001") == nullptr);
}

TEST_CASE("findBySerial: a non-digit suffix matches, a digit suffix does not, an exact hit wins") {
    TempDir tmp("rdb");
    Bytes records;
    appendSimpleRecord(records, "Final Fantasy IX (USA) (Disc 1)", "SLUS-01251GH-F-0"); // Greatest Hits
    appendSimpleRecord(records, "Other Game", "SLUS-012510");
    appendSimpleRecord(records, "Other Variant", "SLUS-12345-99");
    appendSimpleRecord(records, "Exact", "SLUS-12345");
    records.push_back(0xc0);

    RdbReader reader;
    REQUIRE(reader.open(writeRdb(tmp, "serials.rdb", makeRdb(0, records))));

    const RdbReader::Record *gh = reader.findBySerial("SLUS-01251");
    REQUIRE(gh != nullptr);
    CHECK(gh->name == "Final Fantasy IX (USA) (Disc 1)");

    CHECK(reader.findBySerial("SLUS-01252") == nullptr); // nothing starts with it
    CHECK(reader.findBySerial("SLUS-0125") == nullptr);  // "SLUS-01251..." continues with a digit

    const RdbReader::Record *exact = reader.findBySerial("SLUS-12345");
    REQUIRE(exact != nullptr);
    CHECK(exact->name == "Exact");
}

TEST_CASE("a cartridge record: crc (4-byte binary) and rom_name are indexed, an integer crc is read too") {
    TempDir tmp("rdb");
    Bytes records;
    appendRomRecord(records, "Adventures of Lolo (USA)", "Adventures of Lolo (USA).nes", 0xD9C4CBF7u, "HAL Laboratory",
                    1989, 1);
    appendRomRecord(records, "Metal Slug (NGM-2510)", "mslug.zip", 0x0AC09D00u, "Nazca", 1996, 2);
    records.push_back(0x82); // an integer crc, as a hand-made database might spell it
    appendString(records, "name");
    appendString(records, "Int Crc Game");
    appendString(records, "crc");
    records.push_back(0xce); // uint32
    records.insert(records.end(), {0x12, 0x34, 0x56, 0x78});
    records.push_back(0xc0);
    string path = writeRdb(tmp, "Nintendo - Nintendo Entertainment System.rdb", makeRdb(0, records));

    RdbReader rdb;
    REQUIRE(rdb.open(path));
    CHECK(rdb.size() == 3);

    const RdbReader::Record *lolo = rdb.findByCrc(0xD9C4CBF7u);
    REQUIRE(lolo);
    CHECK(lolo->name == "Adventures of Lolo (USA)");
    CHECK(lolo->romName == "Adventures of Lolo (USA).nes");
    CHECK(lolo->publisher == "HAL Laboratory");
    CHECK(lolo->releaseyear == 1989);
    CHECK(lolo->users == 1);

    const RdbReader::Record *mslug = rdb.findByRomName("mslug.zip");
    REQUIRE(mslug);
    CHECK(mslug->name == "Metal Slug (NGM-2510)");
    CHECK(mslug->crc == 0x0AC09D00u);

    REQUIRE(rdb.findByCrc(0x12345678u));
    CHECK(rdb.findByCrc(0x12345678u)->name == "Int Crc Game");
    CHECK(rdb.findByCrc(0) == nullptr);
    CHECK(rdb.findByCrc(1) == nullptr);
    CHECK(rdb.findByRomName("nope.zip") == nullptr);
}
