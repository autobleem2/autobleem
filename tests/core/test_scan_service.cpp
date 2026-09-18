//
// ScanService: the background scan and its incremental regional.db reconciliation. Every test here drives
// the worker's methods (runScan()/checkForChanges()) directly, synchronously - see their header comment -
// so these tests need no real thread and no timing.
//
#include "doctest/doctest.h"

#include "../support/fake_game.h"
#include "../support/game_library_fixture.h"
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
    ScanServiceFixture() : svc(library) {
        env.setWorkingPath(tmp.path());
    }

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
    for (const auto &g : games) titles.push_back(g->title);
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

    CHECK(fx.library.usbGames().countGames() == 1);   // no duplicate row

    auto games = fx.library.usbGames().loadUsbGames();
    REQUIRE(games.size() == 1);
    CHECK(games[0].history == 1);        // history survives a rescan
    CHECK(games[0].last_played == 999);  // so does last_played
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
    setSerial("Crash Bandicoot", "0", "SCUS-94900");   // locked: the user's serial is the truth
    setSerial("Spyro", "1", "SCUS-94901");             // unlocked: the image is

    fx.runAndPoll();

    // regional.db holds no serial for a USB game - Game.ini is where it lives (the meta panel reads it
    // from there), so that is what the scan must have left right
    auto iniValue = [&](const string &game, const string &key) {
        ableem::IniFile ini;
        ini.load(fx.tmp.at("Games/" + game + "/Game.ini"));
        return ini.values[key];
    };
    CHECK(iniValue("Crash Bandicoot", "serial") == "SCUS-94900");
    CHECK(iniValue("Crash Bandicoot", "region") == ableem::SerialScanner::serialToRegion("SCUS-94900"));  // derived, not left blank
    CHECK(iniValue("Spyro", "serial") == "SLUS-01235");
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
    fx.tmp.writeFile("Games/Crash Bandicoot/Crash Bandicoot.cue",
                     "FILE \"Crash Bandicoot.bin\" BINARY\n"
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

    CHECK_FALSE(fx.svc.checkForChanges());   // nothing scanned yet, empty dir: nothing changed

    test_support::makeFakeGame(fx.gamesDir(), "Crash Bandicoot", "SLUS_012.34");
    CHECK_FALSE(fx.svc.checkForChanges());   // changed, but not yet seen twice in a row (debounce)
    CHECK(fx.svc.checkForChanges());         // same state as the previous check: trigger

    fx.runAndPoll();
    CHECK_FALSE(fx.svc.checkForChanges());   // matches what was just scanned again
}

TEST_CASE("start/requestScan/poll over the real worker thread finds a game end to end") {
    ScanServiceFixture fx;
    test_support::makeFakeGame(fx.gamesDir(), "Crash Bandicoot", "SLUS_012.34");

    fx.svc.start();
    CHECK(fx.svc.requestScan());

    ScanUpdate total;
    bool finished = false;
    for (int i = 0; i < 200 && !finished; i++) {   // up to ~10s of polling; a tiny fake scan finishes in well under 1s
        ScanUpdate update = fx.svc.poll();
        total.addedGames.insert(total.addedGames.end(), update.addedGames.begin(), update.addedGames.end());
        if (update.finished) finished = true;
        if (!finished) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    fx.svc.stop();

    REQUIRE(finished);
    REQUIRE(total.addedGames.size() == 1);
    CHECK(total.addedGames[0]->title == "Crash Bandicoot");
    CHECK(fx.library.usbGames().countGames() == 1);
}
