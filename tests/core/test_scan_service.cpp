//
// ScanService: the background scan and its incremental regional.db reconciliation. Every test here drives
// the worker's methods (runScan()/checkForChanges()) directly, synchronously - see their header comment -
// so these tests need no real thread and no timing.
//
#include "doctest/doctest.h"

#include "../support/fake_game.h"
#include "../support/game_library_fixture.h"
#include "../support/rdb_builder.h"
#include "../support/string_maker.h"

#include "core/services/scan_service.h"

#include <ableem/engine/ini_file.h>
#include <ableem/engine/serial_scanner.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using std::string;
using std::vector;

namespace {

struct ScanServiceFixture : GameLibraryFixture {
    ScanServiceFixture() : svc(library) { env.setWorkingPath(tmp.path()); }

    // runs one scan cycle and applies every event it produced - what a real frame loop does over several
    // poll() calls, collapsed into one for a test that only cares about the end state
    ScanUpdate runAndPoll() {
        svc.runScan();
        return svc.poll();
    }

    string gamesDir() const { return tmp.at("Games"); }

    ScanService svc;
};

vector<string> titlesOf(const PsGames &games) {
    vector<string> titles;
    for (const auto &g : games)
        titles.push_back(g->title);
    return titles;
}

} // namespace

TEST_CASE("a first scan adds every verified game and saves the fingerprint") {
    ScanServiceFixture fx;
    test_support::makeFakeGame(fx.gamesDir(), "Crash Bandicoot", "SLUS_012.34");
    test_support::makeFakeGame(fx.gamesDir(), "Spyro", "SLUS_012.35");

    ScanUpdate update = fx.runAndPoll();

    CHECK(update.finished);
    CHECK(update.finishedGameCount == 2);
    CHECK(update.finishedFailedCount == 0);
    CHECK(update.addedGames.size() == 2);
    CHECK(update.updatedGames.empty());
    CHECK(update.removedGameIds.empty());

    vector<string> titles = titlesOf(update.addedGames);
    CHECK(std::find(titles.begin(), titles.end(), "Crash Bandicoot") != titles.end());
    CHECK(std::find(titles.begin(), titles.end(), "Spyro") != titles.end());

    CHECK(fx.library.usbGames().countGames() == 2);
    CHECK(ableem::DirEntry::exists(fx.tmp.at("games.fingerprint")));
    CHECK(ableem::DirEntry::exists(fx.tmp.at("autobleem.list")));
}

TEST_CASE("rescanning an unchanged game updates its row in place - same id, no duplicate") {
    ScanServiceFixture fx;
    test_support::makeFakeGame(fx.gamesDir(), "Crash Bandicoot", "SLUS_012.34");

    ScanUpdate first = fx.runAndPoll();
    REQUIRE(first.addedGames.size() == 1);
    int id = first.addedGames[0]->gameId;

    fx.library.usbGames().updateHistory(id, 1);
    fx.library.usbGames().updateDatePlayed(id, 999);

    ScanUpdate second = fx.runAndPoll();
    CHECK(second.addedGames.empty());
    REQUIRE(second.updatedGames.size() == 1);
    CHECK(second.updatedGames[0]->gameId == id);

    CHECK(fx.library.usbGames().countGames() == 1); // no duplicate row

    auto games = fx.library.usbGames().loadUsbGames();
    REQUIRE(games.size() == 1);
    CHECK(games[0].history == 1);       // history survives a rescan
    CHECK(games[0].last_played == 999); // so does last_played
}

TEST_CASE("the Game.ini flags the editor writes survive a rescan") {
    // Favorite and Play_using_ra are read back from Game.ini on every scan and written out again;
    // a misspelt key in that read-back (play_us_ra) used to reset Play using RA to false each time
    ScanServiceFixture fx;
    test_support::makeFakeGame(fx.gamesDir(), "Crash Bandicoot", "SLUS_012.34");
    REQUIRE(fx.runAndPoll().addedGames.size() == 1);

    string iniPath = fx.tmp.at("Games/Crash Bandicoot/Game.ini");
    ableem::IniFile ini;
    ini.load(iniPath);
    REQUIRE_FALSE(ini.values.empty());
    ini.values["favorite"] = "1";
    ini.values["play_using_ra"] = "true";
    ini.save(iniPath);

    ScanUpdate second = fx.runAndPoll();
    REQUIRE(second.updatedGames.size() == 1);
    CHECK(second.updatedGames[0]->favorite);
    CHECK(second.updatedGames[0]->play_using_ra);

    ini.values.clear();
    ini.load(iniPath);
    CHECK(ini.values["favorite"] == "1");
    CHECK(ini.values["play_using_ra"] == "true");
}

TEST_CASE("a locked game keeps the serial its Game.ini holds; an unlocked one is read from the image") {
    ScanServiceFixture fx;
    test_support::makeFakeGame(fx.gamesDir(), "Crash Bandicoot", "SLUS_012.34");
    test_support::makeFakeGame(fx.gamesDir(), "Spyro", "SLUS_012.35");
    REQUIRE(fx.runAndPoll().addedGames.size() == 2);

    auto setSerial = [&](const string &game, const string &automation, const string &serial) {
        string iniPath = fx.tmp.at("Games/" + game + "/Game.ini");
        ableem::IniFile ini;
        ini.load(iniPath);
        REQUIRE(ini.values["serial"] != serial);
        ini.values["automation"] = automation;
        ini.values["serial"] = serial;
        ini.values["region"] = "";
        ini.save(iniPath);
    };
    setSerial("Crash Bandicoot", "0", "SCUS-94900"); // locked: the user's serial is the truth
    setSerial("Spyro", "1", "SCUS-94901");           // unlocked: the image is

    fx.runAndPoll();

    // regional.db holds no serial for a USB game - Game.ini is where it lives (the meta panel reads it
    // from there), so that is what the scan must have left right
    auto iniValue = [&](const string &game, const string &key) {
        ableem::IniFile ini;
        ini.load(fx.tmp.at("Games/" + game + "/Game.ini"));
        return ini.values[key];
    };
    CHECK(iniValue("Crash Bandicoot", "serial") == "SCUS-94900");
    CHECK(iniValue("Crash Bandicoot", "region") ==
          ableem::SerialScanner::serialToRegion("SCUS-94900")); // derived, not left blank
    CHECK(iniValue("Spyro", "serial") == "SLUS-01235");
}

TEST_CASE("with a RetroArch tree the scan takes the title from the rdb and caches the thumbnail paths") {
    ScanServiceFixture fx;
    fx.env.setRetroarchDir(fx.tmp.makeSubDir("retroarch"));
    fx.tmp.makeSubDir("retroarch/database/rdb");
    test_support::Bytes records;
    test_support::appendGameRecord(records, "Crash Bandicoot (USA)", "SLUS-01234", "USA", "SCEA", 1996, 1);
    records.push_back(0xc0);
    test_support::writeRdb(fx.tmp, "retroarch/database/rdb/Sony - PlayStation.rdb", test_support::makeRdb(0, records));
    fx.tmp.makeSubDir("retroarch/thumbnails/Sony - PlayStation/Named_Boxarts");
    fx.tmp.writeFile("retroarch/thumbnails/Sony - PlayStation/Named_Boxarts/Crash Bandicoot (USA).png", "png");
    fx.tmp.makeSubDir("retroarch/thumbnails/Sony - PlayStation/Named_Snaps");
    fx.tmp.writeFile("retroarch/thumbnails/Sony - PlayStation/Named_Snaps/Crash Bandicoot (USA).png", "png");

    test_support::makeFakeGame(fx.gamesDir(), "crash", "SLUS_012.34"); // folder name is not the title
    ScanUpdate update = fx.runAndPoll();
    REQUIRE(update.addedGames.size() == 1);
    const PsGame &game = *update.addedGames[0];
    CHECK(game.title == "Crash Bandicoot"); // the rdb's, without its "(USA)"
    CHECK(game.publisher == "SCEA");
    CHECK(game.year == 1996);
    CHECK(game.recordName == "Crash Bandicoot (USA)");
    CHECK(game.coverPath ==
          fx.tmp.at("retroarch/thumbnails/Sony - PlayStation/Named_Boxarts/Crash Bandicoot (USA).png"));
    CHECK(game.snapPath == fx.tmp.at("retroarch/thumbnails/Sony - PlayStation/Named_Snaps/Crash Bandicoot (USA).png"));

    ableem::IniFile ini;
    ini.load(fx.tmp.at("Games/crash/Game.ini"));
    CHECK(ini.values["thumbnail_record_name"] == "Crash Bandicoot (USA)");
    CHECK(ini.values["cached_cover_path"] == game.coverPath);
    CHECK(ini.values["cached_snap_path"] == game.snapPath);
}

TEST_CASE(
    "a default.png placeholder next to a game is removed once the thumbnails tree has a cover; a real cover stays") {
    ScanServiceFixture fx;
    fx.env.setRetroarchDir(fx.tmp.makeSubDir("retroarch"));
    fx.tmp.writeFile("default.png", "placeholder png bytes"); // the working path's default cover
    fx.tmp.makeSubDir("retroarch/thumbnails/Sony - PlayStation/Named_Boxarts");
    fx.tmp.writeFile("retroarch/thumbnails/Sony - PlayStation/Named_Boxarts/Crash Bandicoot.png", "png");
    fx.tmp.writeFile("retroarch/thumbnails/Sony - PlayStation/Named_Boxarts/Spyro.png", "png");

    test_support::makeFakeGame(fx.gamesDir(), "Crash Bandicoot", "SLUS_012.34");
    fx.tmp.writeFile("Games/Crash Bandicoot/Crash Bandicoot.png", "placeholder png bytes"); // an old scan's copy
    test_support::makeFakeGame(fx.gamesDir(), "Spyro", "SLUS_012.35");
    fx.tmp.writeFile("Games/Spyro/Spyro.png", "the user's own cover");

    ScanUpdate update = fx.runAndPoll();
    REQUIRE(update.addedGames.size() == 2);
    CHECK_FALSE(ableem::DirEntry::exists(fx.tmp.at("Games/Crash Bandicoot/Crash Bandicoot.png")));
    CHECK(ableem::DirEntry::exists(fx.tmp.at("Games/Spyro/Spyro.png")));
    for (const auto &g : update.addedGames)
        CHECK(g->coverPath == fx.tmp.at("retroarch/thumbnails/Sony - PlayStation/Named_Boxarts/" + g->title + ".png"));
}

TEST_CASE("sibling (Disc n) folders are merged into one game before the scan, with an .m3u") {
    ScanServiceFixture fx;
    test_support::makeFakeGame(fx.gamesDir(), "Final Fantasy VII (Disc 2)", "SLUS_009.00");
    test_support::makeFakeGame(fx.gamesDir(), "Final Fantasy VII (Disc 1)", "SLUS_008.99");
    test_support::makeFakeGame(fx.gamesDir(), "Final Fantasy VII (Disc 3)", "SLUS_009.01");
    test_support::makeFakeGame(fx.gamesDir(), "Single Disc Game", "SLUS_012.34");
    // an earlier scan's Game.ini on disc 1, with the disc list of one disc
    fx.tmp.writeFile(
        "Games/Final Fantasy VII (Disc 1)/Game.ini",
        "[Game]\nAutomation=1\nTitle=Final Fantasy VII (Disc 1)\nDiscs=Final Fantasy VII (Disc 1)\nFavorite=1\n");

    ScanUpdate update = fx.runAndPoll();
    REQUIRE(update.addedGames.size() == 2); // one merged game, one single-disc one

    string merged = fx.tmp.at("Games/Final Fantasy VII");
    CHECK(ableem::DirEntry::exists(merged));
    CHECK_FALSE(ableem::DirEntry::exists(fx.tmp.at("Games/Final Fantasy VII (Disc 1)")));
    CHECK_FALSE(ableem::DirEntry::exists(fx.tmp.at("Games/Final Fantasy VII (Disc 2)")));
    CHECK_FALSE(ableem::DirEntry::exists(fx.tmp.at("Games/Final Fantasy VII (Disc 3)")));
    CHECK(ableem::DirEntry::exists(merged + "/Final Fantasy VII (Disc 1).cue"));
    CHECK(ableem::DirEntry::exists(merged + "/Final Fantasy VII (Disc 2).bin"));
    CHECK(ableem::DirEntry::exists(merged + "/Final Fantasy VII (Disc 3).cue"));
    CHECK(ableem::DirEntry::exists(
        merged + "/Final Fantasy VII (Disc 1).m3u")); // named after the first disc, like every multi-disc folder's

    const PsGame *ff = nullptr;
    for (const auto &g : update.addedGames)
        if (g->folder.find("Final Fantasy VII") != string::npos)
            ff = g.get();
    REQUIRE(ff != nullptr);
    CHECK(ff->cds == 3);
    CHECK(ff->title == "Final Fantasy VII"); // the folder-derived "(Disc 1)" title became the base name
    CHECK(ff->favorite);                     // disc 1's Game.ini survived the merge

    // a second scan finds nothing to merge and changes nothing
    ScanUpdate second = fx.runAndPoll();
    CHECK(second.addedGames.empty());
    CHECK(second.updatedGames.size() == 2);
}

TEST_CASE(
    "a merge is skipped when the merged folder already exists as something else, or a file would be overwritten") {
    ScanServiceFixture fx;
    test_support::makeFakeGame(fx.gamesDir(), "Tekken (Disc 1)", "SLUS_008.99");
    test_support::makeFakeGame(fx.gamesDir(), "Tekken (Disc 2)", "SLUS_009.00");
    test_support::makeFakeGame(fx.gamesDir(), "Tekken", "SLUS_009.01"); // a third, unrelated game in the way

    test_support::makeFakeGame(fx.gamesDir(), "Wipeout (Disc 1)", "SLUS_010.00");
    test_support::makeFakeGame(fx.gamesDir(), "Wipeout (Disc 2)", "SLUS_010.01");
    // disc 1's folder already holds a copy of disc 2's files: moving them in would overwrite these
    ableem::DirEntry::copy(fx.tmp.at("Games/Wipeout (Disc 2)/Wipeout (Disc 2).cue"),
                           fx.tmp.at("Games/Wipeout (Disc 1)/Wipeout (Disc 2).cue"));
    ableem::DirEntry::copy(fx.tmp.at("Games/Wipeout (Disc 2)/Wipeout (Disc 2).bin"),
                           fx.tmp.at("Games/Wipeout (Disc 1)/Wipeout (Disc 2).bin"));

    ScanUpdate update = fx.runAndPoll();
    CHECK(ableem::DirEntry::exists(fx.tmp.at("Games/Tekken (Disc 1)")));
    CHECK(ableem::DirEntry::exists(fx.tmp.at("Games/Tekken (Disc 2)")));
    CHECK(ableem::DirEntry::exists(fx.tmp.at("Games/Tekken")));
    CHECK(ableem::DirEntry::exists(fx.tmp.at("Games/Wipeout")));          // disc 1 was renamed...
    CHECK(ableem::DirEntry::exists(fx.tmp.at("Games/Wipeout (Disc 2)"))); // ...but disc 2 stayed where it was
    CHECK(ableem::DirEntry::exists(fx.tmp.at("Games/Wipeout (Disc 2)/Wipeout (Disc 2).cue"))); // not moved
    CHECK(update.addedGames.size() == 5);
}

TEST_CASE("a game folder that disappears is removed from regional.db on the next scan") {
    ScanServiceFixture fx;
    test_support::makeFakeGame(fx.gamesDir(), "Crash Bandicoot", "SLUS_012.34");
    test_support::makeFakeGame(fx.gamesDir(), "Spyro", "SLUS_012.35");
    ScanUpdate first = fx.runAndPoll();
    REQUIRE(first.addedGames.size() == 2);

    ableem::DirEntry::removeDirAndContents(fx.gamesDir() + ableem::sep + "Spyro");

    ScanUpdate second = fx.runAndPoll();
    REQUIRE(second.removedGameIds.size() == 1);
    CHECK(fx.library.usbGames().countGames() == 1);

    auto games = fx.library.usbGames().loadUsbGames();
    REQUIRE(games.size() == 1);
    CHECK(games[0].title == "Crash Bandicoot");
}

TEST_CASE("a game that fails verify() is dropped from regional.db") {
    ScanServiceFixture fx;
    test_support::makeFakeGame(fx.gamesDir(), "Crash Bandicoot", "SLUS_012.34");
    ScanUpdate first = fx.runAndPoll();
    REQUIRE(first.addedGames.size() == 1);

    // Break the game without removing its folder or its .bin, so it still looks like a game folder at the
    // hierarchy stage (GameSubDir::scanAll requires a .bin/.pbp/.img/.chd file - a folder missing one is
    // excluded from the scan entirely rather than reaching verify(), which is not what this test is after).
    // A second FILE line naming a disc that does not exist is what actually fails verify(): the scanner's
    // own cue repair (repairBrokenCueFiles) cannot heal this - it has only one real .bin to work with, so
    // its regenerated second track falls back to its own literal placeholder, which does not exist either.
    fx.tmp.writeFile("Games/Crash Bandicoot/Crash Bandicoot.cue", "FILE \"Crash Bandicoot.bin\" BINARY\n"
                                                                  "  TRACK 01 MODE2/2352\n"
                                                                  "    INDEX 01 00:00:00\n"
                                                                  "FILE \"Crash Bandicoot (Track 2).bin\" BINARY\n"
                                                                  "  TRACK 02 AUDIO\n"
                                                                  "    INDEX 00 00:02:00\n"
                                                                  "    INDEX 01 00:04:00\n");

    ScanUpdate second = fx.runAndPoll();
    CHECK(second.finishedFailedCount == 1);
    CHECK(second.lastFailedGamePath == fx.gamesDir() + ableem::sep + "Crash Bandicoot");
    REQUIRE(second.removedGameIds.size() == 1);
    CHECK(fx.library.usbGames().countGames() == 0);
}

TEST_CASE("checkForChanges: false when unchanged, true once a change is seen twice, false again after a scan") {
    ScanServiceFixture fx;

    CHECK_FALSE(fx.svc.checkForChanges()); // nothing scanned yet, empty dir: nothing changed

    test_support::makeFakeGame(fx.gamesDir(), "Crash Bandicoot", "SLUS_012.34");
    CHECK_FALSE(fx.svc.checkForChanges()); // changed, but not yet seen twice in a row (debounce)
    CHECK(fx.svc.checkForChanges());       // same state as the previous check: trigger

    fx.runAndPoll();
    CHECK_FALSE(fx.svc.checkForChanges()); // matches what was just scanned again
}

TEST_CASE("start/requestScan/poll over the real worker thread finds a game end to end") {
    ScanServiceFixture fx;
    test_support::makeFakeGame(fx.gamesDir(), "Crash Bandicoot", "SLUS_012.34");

    fx.svc.start();
    CHECK(fx.svc.requestScan());

    ScanUpdate total;
    bool finished = false;
    for (int i = 0; i < 200 && !finished; i++) { // up to ~10s of polling; a tiny fake scan finishes in well under 1s
        ScanUpdate update = fx.svc.poll();
        total.addedGames.insert(total.addedGames.end(), update.addedGames.begin(), update.addedGames.end());
        if (update.finished)
            finished = true;
        if (!finished)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    fx.svc.stop();

    REQUIRE(finished);
    REQUIRE(total.addedGames.size() == 1);
    CHECK(total.addedGames[0]->title == "Crash Bandicoot");
    CHECK(fx.library.usbGames().countGames() == 1);
}
