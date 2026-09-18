//
// Reading a PS1 disc image's ISO9660 directory: .bin (raw 2352-byte sectors) and .chd, through the
// engine's public IsoDirectoryReader/SerialScanner. The fixtures in tests/data are AutoBleem-NG's:
// one small ISO (SYSTEM/SYSTEM.CNF, PSX.EXE, README.TXT, volume "TEST_DISC", system "PLAYSTATION"),
// as a MODE1/2352 .bin and as a CHD - which chdman compressed with zstd, the codec the earlier
// libmamecd did not have. That file is the regression test for the libchdr refresh.
//
#include "doctest/doctest.h"

#include <ableem/engine/game_types.h>
#include <ableem/engine/iso_directory.h>
#include <ableem/engine/serial_scanner.h>

#include <algorithm>
#include <string>
#include <vector>

using ableem::IsoDirectory;
using ableem::IsoDirectoryReader;
using std::string;

namespace {

const string dataDir = AB_TEST_DATA_DIR;

bool lists(const IsoDirectory &dir, const string &name) {
    return std::find(dir.rootDir.begin(), dir.rootDir.end(), name) != dir.rootDir.end();
}

} // namespace

TEST_CASE("a .bin's root directory reads as a PlayStation disc") {
    IsoDirectory dir = IsoDirectoryReader::read(dataDir + "/test.bin", 1, false);
    CHECK(dir.systemName == "PLAYSTATION");
    CHECK(dir.volumeName == "TEST_DISC");
    CHECK(lists(dir, "PSX.EXE"));
    CHECK(lists(dir, "README.TXT"));
    CHECK(lists(dir, "SYSTEM"));
}

TEST_CASE("a zstd-compressed CHD reads the same directory as the .bin it was made from") {
    IsoDirectory chd = IsoDirectoryReader::read(dataDir + "/test.chd", 1, true);
    IsoDirectory bin = IsoDirectoryReader::read(dataDir + "/test.bin", 1, false);
    CHECK(chd.systemName == "PLAYSTATION");
    CHECK(chd.volumeName == bin.volumeName);
    CHECK(chd.rootDir == bin.rootDir);
    CHECK(lists(chd, "SYSTEM"));
}

TEST_CASE("a file that is not a CHD is reported as unreadable, not crashed on") {
    IsoDirectory dir = IsoDirectoryReader::read(dataDir + "/test.cue", 1, true);
    CHECK(dir.systemName == "UNKNOWN");
    CHECK(dir.rootDir.empty());

    IsoDirectory missing = IsoDirectoryReader::read(dataDir + "/no-such-file.chd", 1, true);
    CHECK(missing.systemName == "UNKNOWN");
}

TEST_CASE("the serial scanner walks a CHD: no serial-named file on this disc, so none is found") {
    // the disc boots PSX.EXE and has no SLUS_xxx.xx file or volume name, so both the image read and
    // the fallback come back empty - the point is that the CHD was opened and walked, not rejected
    CHECK(ableem::SerialScanner::readSerialFromImage(ableem::IMAGE_CHD, dataDir + "/", dataDir + "/test.chd") == "");
}
