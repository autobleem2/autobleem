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
    appendString(out, "crc");        // not a field we keep
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

    const RdbReader::Record *bySerial = reader.findBySerial("SLUS-12345");   // prefix of "SLUS-12345-01"
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
    appendSimpleRecord(records, "Final Fantasy IX (USA) (Disc 1)", "SLUS-01251GH-F-0");   // Greatest Hits
    appendSimpleRecord(records, "Other Game", "SLUS-012510");
    appendSimpleRecord(records, "Other Variant", "SLUS-12345-99");
    appendSimpleRecord(records, "Exact", "SLUS-12345");
    records.push_back(0xc0);

    RdbReader reader;
    REQUIRE(reader.open(writeRdb(tmp, "serials.rdb", makeRdb(0, records))));

    const RdbReader::Record *gh = reader.findBySerial("SLUS-01251");
    REQUIRE(gh != nullptr);
    CHECK(gh->name == "Final Fantasy IX (USA) (Disc 1)");

    CHECK(reader.findBySerial("SLUS-01252") == nullptr);   // nothing starts with it
    CHECK(reader.findBySerial("SLUS-0125") == nullptr);    // "SLUS-01251..." continues with a digit

    const RdbReader::Record *exact = reader.findBySerial("SLUS-12345");
    REQUIRE(exact != nullptr);
    CHECK(exact->name == "Exact");
}
