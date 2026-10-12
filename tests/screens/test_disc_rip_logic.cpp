//
// Copying a disc to Games (evoui/screens/disc_rip_logic.h): who is offered the copy, the names of the CHD, its .sbi
// and the set's .m3u, the LibCrypt .sbi that is copied, the refusals before a rip, and the popup's words.
//
#include "doctest/doctest.h"

#include "support/temp_dir.h"

#include "disc_rip_logic.h"

#include <set>
#include <string>

using namespace discrip;
using discview::Drive;

TEST_CASE("the copy is offered on a ready PS1 disc the databases know, in the disc view only") {
    CHECK(offered(Drive::ReadyGame, true, true));
    CHECK_FALSE(offered(Drive::ReadyGame, true, false)); // the volume label is no title
    CHECK_FALSE(offered(Drive::ReadyGame, false, true)); // the carousel shows the library, not the disc
    CHECK_FALSE(offered(Drive::ReadyAudio, true, true));
    CHECK_FALSE(offered(Drive::NotPs1, true, true));
    CHECK_FALSE(offered(Drive::Reading, true, true));
    CHECK_FALSE(offered(Drive::Empty, true, true));
    CHECK_FALSE(offered(Drive::NoDrive, true, true));
}

TEST_CASE("the disc number is the record name's (Disc N), disc 1 when it has none") {
    CHECK(discNumberOf("Tekken 3 (Europe) (Disc 2)") == 2);
    CHECK(discNumberOf("Final Fantasy VII (USA) (Disc 3)") == 3);
    CHECK(discNumberOf("Final Fantasy VII (USA) (Disc 1)") == 1);
    CHECK(discNumberOf("Crash Bandicoot (USA)") == 1);
    CHECK(discNumberOf("") == 1);
    CHECK(discNumberOf("Odd (Disc )") == 1);
    CHECK(discNumberOf("Odd (Disc 0)") == 1);
    CHECK(discNumberOf("Odd (Disc 2") == 1);
    CHECK(inSet("Tekken 3 (Europe) (Disc 2)"));
    CHECK(inSet("Final Fantasy VII (USA) (Disc 1)"));
    CHECK_FALSE(inSet("Crash Bandicoot (USA)"));
}

TEST_CASE("the target is Games/<title>/<title>.chd, disc 2 and on <title> (Disc N).chd in the same folder") {
    CHECK(chdPath("/g", "Tekken 3", 1) == "/g/Tekken 3/Tekken 3.chd");
    CHECK(chdPath("/g", "Tekken 3", 2) == "/g/Tekken 3/Tekken 3 (Disc 2).chd");
    CHECK(chdPath("/g", "Tekken 3", 3) == "/g/Tekken 3/Tekken 3 (Disc 3).chd");
    CHECK(chdPath("/g", "Tekken 3", 0) == "/g/Tekken 3/Tekken 3.chd");
    CHECK(folderOf("/g", "Tekken 3") == "/g/Tekken 3");
    CHECK(m3uPath("/g", "Tekken 3") == "/g/Tekken 3/Tekken 3.m3u");
}

TEST_CASE("a title that is not a file name on FAT is made safe the way the Games folders are") {
    CHECK(chdPath("/g", "Castlevania: Symphony of the Night", 1) ==
          "/g/Castlevania - Symphony of the Night/Castlevania - Symphony of the Night.chd");
    CHECK(chdPath("/g", "Who? What/Where", 2) == "/g/Who WhatWhere/Who WhatWhere (Disc 2).chd");
}

TEST_CASE("the .sbi is named like the image it belongs to") {
    CHECK(sbiPathFor("/g/Spyro/Spyro.chd") == "/g/Spyro/Spyro.sbi");
    CHECK(sbiPathFor("/g/FF/FF (Disc 2).chd") == "/g/FF/FF (Disc 2).sbi");
    CHECK(sbiPathFor("noext") == "noext.sbi");
}

TEST_CASE("the .m3u lists the CHDs that are there, in disc order") {
    std::set<std::string> there = {"FF (Disc 3).chd", "FF.chd", "FF (Disc 2).chd"};
    auto exists = [&there](const std::string &name) { return there.count(name) > 0; };
    CHECK(m3uText("FF", exists) == "FF.chd\nFF (Disc 2).chd\nFF (Disc 3).chd\n");
    there.erase("FF (Disc 2).chd");
    CHECK(m3uText("FF", exists) == "FF.chd\nFF (Disc 3).chd\n");
    there.clear();
    CHECK(m3uText("FF", exists).empty());
    there.insert("FF (Disc 2).chd"); // disc 2 copied first
    CHECK(m3uText("FF", exists) == "FF (Disc 2).chd\n");
}

TEST_CASE("updateM3u writes the set's playlist into its folder and grows it with each disc") {
    TempDir tmp("rip_m3u");
    tmp.makeSubDir("FF");
    tmp.writeFile("FF/FF (Disc 2).chd", "x");
    CHECK(updateM3u(tmp.path(), "FF"));
    std::string text;
    REQUIRE(ableem::DirEntry::readFile(tmp.at("FF/FF.m3u"), text));
    CHECK(text == "FF (Disc 2).chd\n");

    tmp.writeFile("FF/FF.chd", "x");
    CHECK(updateM3u(tmp.path(), "FF"));
    REQUIRE(ableem::DirEntry::readFile(tmp.at("FF/FF.m3u"), text));
    CHECK(text == "FF.chd\nFF (Disc 2).chd\n");

    TempDir empty("rip_m3u_empty");
    empty.makeSubDir("None");
    CHECK_FALSE(updateM3u(empty.path(), "None")); // nothing to list: no file
    CHECK_FALSE(ableem::DirEntry::exists(empty.at("None/None.m3u")));
}

TEST_CASE("the LibCrypt .sbi is the one the launcher fetched into the disc's save folder") {
    std::set<std::string> there;
    auto exists = [&there](const std::string &path) { return there.count(path) > 0; };
    const std::vector<std::string> folders = {"/saves/SCES-02835-lib", "/saves/disc-SCES-02835"};

    CHECK(chooseSbi(folders, "SCES-02835", exists).empty()); // none: the rip asks the drive for the marks

    there.insert("/saves/disc-SCES-02835/SCES02835.sbi");
    CHECK(chooseSbi(folders, "SCES-02835", exists) == "/saves/disc-SCES-02835/SCES02835.sbi");

    there.insert("/saves/SCES-02835-lib/SCES02835.sbi"); // the first folder wins
    CHECK(chooseSbi(folders, "SCES-02835", exists) == "/saves/SCES-02835-lib/SCES02835.sbi");

    there.clear();
    there.insert("/saves/disc-SCES-02835/SCES02835.2.sbi"); // only a later version: not guessed
    CHECK(chooseSbi(folders, "SCES-02835", exists).empty());
    there.insert("/saves/disc-SCES-02835/SCES02835.1.sbi"); // several versions: the first
    CHECK(chooseSbi(folders, "SCES-02835", exists) == "/saves/disc-SCES-02835/SCES02835.1.sbi");

    CHECK(chooseSbi(folders, "", exists).empty());
    CHECK(chooseSbi({"", ""}, "SCES-02835", exists).empty());
}

TEST_CASE("before the rip: a target that is there is refused, then a stick without room") {
    TempDir tmp("rip_pre");
    tmp.makeSubDir("Game");
    uint64_t freeBytes = 0;
    const std::string target = tmp.at("Game/Game.chd");

    CHECK(preflight(target, tmp.path(), 1024, &freeBytes) == Check::Ok);

    tmp.writeFile("Game/Game.chd", "x");
    CHECK(preflight(target, tmp.path(), 1024, &freeBytes) == Check::AlreadyThere);

    // more than any stick holds
    CHECK(preflight(tmp.at("Game/Other.chd"), tmp.path(), UINT64_C(1) << 62, &freeBytes) == Check::NoRoom);
    // "already in Games" wins over the room
    CHECK(preflight(target, tmp.path(), UINT64_C(1) << 62, &freeBytes) == Check::AlreadyThere);
}

TEST_CASE("the space a disc needs comes from its table of contents") {
    CdToc toc;
    toc.leadout = 1000;
    CHECK(RipJob::neededBytes(toc) == 1000u * 2352u);
}

TEST_CASE("the popup's numbers: percent, speed as Nx, time left") {
    CHECK(percentOf(0, 0) == 0);
    CHECK(percentOf(0, 1000) == 0);
    CHECK(percentOf(500, 1000) == 50);
    CHECK(percentOf(999, 1000) == 99);
    CHECK(percentOf(1000, 1000) == 100);
    CHECK(percentOf(2000, 1000) == 100);

    CHECK(speedText(0) == "0.0x");
    CHECK(speedText(153600) == "1.0x");
    CHECK(speedText(153600 * 12.3) == "12.3x");
    CHECK(speedText(153600 * 40) == "40.0x");

    CHECK(secondsLeft(0, 0, 1000) == -1);
    CHECK(secondsLeft(0, 1000, 0) == -1);
    CHECK(secondsLeft(1000, 1000, 1000) == 0);
    CHECK(secondsLeft(0, 1000, 2352) == 1000);

    CHECK(timeText(-1) == "--:--");
    CHECK(timeText(0) == "0:00");
    CHECK(timeText(187) == "3:07");
    CHECK(timeText(3750) == "1:02:30");
}

TEST_CASE("sizes and the text templates") {
    CHECK(bytesText(0) == "0 MB");
    CHECK(bytesText(700ull * 1024 * 1024) == "700 MB");
    CHECK(bytesText(3ull * 1024 * 1024 * 1024 / 2) == "1.5 GB");
    CHECK(fill("Needs %1, %2 is free.", {"700 MB", "3.0 GB"}) == "Needs 700 MB, 3.0 GB is free.");
    CHECK(fill("%2 before %1", {"a", "b"}) == "b before a");
    CHECK(fill("nothing", {"a"}) == "nothing");
    CHECK(fill("%1 %1", {"a"}) == "a %1"); // each mark once
}
