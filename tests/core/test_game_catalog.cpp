//
// GameCatalogService: the writes - history ranking, deleting a game, flushing covers.
//
#include "doctest/doctest.h"

#include "../support/game_library_fixture.h"
#include "../support/string_maker.h"

#include "core/services/config.h"
#include "core/services/game_catalog.h"
#include "core/services/game_query.h"

#include <memory>
#include <string>
#include <vector>

using std::string;
using std::vector;

namespace {

vector<string> titlesOf(const PsGames &games) {
    vector<string> titles;
    for (const auto &game : games)
        titles.push_back(game->title);
    return titles;
}

PsGamePtr findByTitle(const PsGames &games, const string &title) {
    for (const auto &game : games)
        if (game->title == title)
            return game;
    return PsGamePtr();
}

// the library, a Config and both services, which is what every test here needs
struct Catalog : GameLibraryFixture {
    Catalog() {
        ableem::Environment::setWorkingPath(tmp.path());
        tmp.writeFile("config.ini", "Origames=true\n");
        config = std::make_unique<Config>();
        query = std::make_unique<GameQueryService>(library, *config);
        catalog = std::make_unique<GameCatalogService>(library, *query);
    }

    // The history set as the carousel shows it. query->history() only filters - gamesFor() is what applies
    // the most-recently-played-first order, so that is the path worth asserting on.
    PsGames historySet() {
        GameSetSelection selection;
        selection.set = GameSet::PS1;
        selection.ps1SelectState = Ps1SelectState::History;
        return query->gamesFor(selection);
    }
    vector<string> historyOrder() { return titlesOf(historySet()); }
    vector<int> historyRanks() {
        vector<int> ranks;
        for (const auto &game : historySet())
            ranks.push_back(game->history);
        return ranks;
    }

    void play(const string &title) {
        PsGamePtr game = findByTitle(query->allPs1Games(true, true), title);
        REQUIRE(game);
        catalog->recordGamePlayed(game);
    }

    std::unique_ptr<Config> config;
    std::unique_ptr<GameQueryService> query;
    std::unique_ptr<GameCatalogService> catalog;
};

} // namespace

TEST_CASE("playing a game puts it at the top of the history") {
    Catalog lib;
    lib.addUsbGame(1, "Tekken 3");
    lib.addUsbGame(2, "Crash Bandicoot");
    lib.addSubDirRow(0, "Games", 0, 2);
    lib.putGameInSubDirRow(0, 1);
    lib.putGameInSubDirRow(0, 2);

    lib.play("Tekken 3");
    CHECK(lib.historyOrder() == vector<string>{"Tekken 3"});

    lib.play("Crash Bandicoot");
    CHECK(lib.historyOrder() == vector<string>{"Crash Bandicoot", "Tekken 3"});
}

TEST_CASE("replaying a game moves it back to the top instead of duplicating it") {
    Catalog lib;
    lib.addUsbGame(1, "Tekken 3");
    lib.addUsbGame(2, "Crash Bandicoot");
    lib.addUsbGame(3, "Ridge Racer");
    lib.addSubDirRow(0, "Games", 0, 3);
    for (int id : {1, 2, 3})
        lib.putGameInSubDirRow(0, id);

    lib.play("Tekken 3");
    lib.play("Crash Bandicoot");
    lib.play("Ridge Racer");
    CHECK(lib.historyOrder() == vector<string>{"Ridge Racer", "Crash Bandicoot", "Tekken 3"});

    lib.play("Tekken 3");
    CHECK(lib.historyOrder() == vector<string>{"Tekken 3", "Ridge Racer", "Crash Bandicoot"});
}

TEST_CASE("the history ranks are 1..N with no gaps and no ties") {
    Catalog lib;
    lib.addUsbGame(1, "Tekken 3");
    lib.addUsbGame(2, "Crash Bandicoot");
    lib.addUsbGame(3, "Ridge Racer");
    lib.addSubDirRow(0, "Games", 0, 3);
    for (int id : {1, 2, 3})
        lib.putGameInSubDirRow(0, id);

    lib.play("Tekken 3");
    lib.play("Crash Bandicoot");
    lib.play("Ridge Racer");

    CHECK(lib.historyRanks() == vector<int>{1, 2, 3});
}

TEST_CASE("internal games share the one history ranking with USB games") {
    Catalog lib;
    lib.addUsbGame(1, "Tekken 3");
    lib.addSubDirRow(0, "Games", 0, 1);
    lib.putGameInSubDirRow(0, 1);
    lib.addInternalGame(10, "Jumping Flash");

    // The internal game is played FIRST, so the next play has to renumber it. Playing it last would pass
    // even if internal games were excluded from the renumbering, because nothing would need renumbering.
    lib.play("Jumping Flash");
    lib.play("Tekken 3");

    CHECK(lib.historyOrder() == vector<string>{"Tekken 3", "Jumping Flash"});
    // if internal games were left out of the renumbering, both would still claim rank 1
    CHECK(lib.historyRanks() == vector<int>{1, 2});
}

TEST_CASE("the history stops at HistoryLimit games and the oldest drops off") {
    Catalog lib;
    const int limit = GameCatalogService::HistoryLimit;
    const int total = limit + 3;

    lib.addSubDirRow(0, "Games", 0, total);
    for (int i = 1; i <= total; ++i) {
        // zero-padded so title order and play order are unrelated
        string title = "Game " + string(3 - std::to_string(i).size(), '0') + std::to_string(i);
        lib.addUsbGame(i, title);
        lib.putGameInSubDirRow(0, i);
    }

    // play them all, oldest first, so "Game 001" is the least recent
    for (int i = 1; i <= total; ++i) {
        string title = "Game " + string(3 - std::to_string(i).size(), '0') + std::to_string(i);
        lib.play(title);
    }

    vector<string> history = lib.historyOrder();
    REQUIRE(history.size() == static_cast<size_t>(limit));
    CHECK(history.front() == "Game " + std::to_string(total)); // the last one played
    CHECK(history.back() == "Game 004");                       // 001..003 fell off the end
}

TEST_CASE("deleting a game removes its row and its folder") {
    Catalog lib;
    lib.addUsbGame(1, "Tekken 3");
    lib.addUsbGame(2, "Crash Bandicoot");
    lib.addSubDirRow(0, "Games", 0, 2);
    lib.putGameInSubDirRow(0, 1);
    lib.putGameInSubDirRow(0, 2);

    PsGamePtr game = findByTitle(lib.query->allPs1Games(true, false), "Tekken 3");
    REQUIRE(game);
    REQUIRE(ableem::DirEntry::exists(game->folder));

    auto result = lib.catalog->deleteUsbGame(*game);

    CHECK(result.removed);
    CHECK_FALSE(ableem::DirEntry::exists(game->folder));
    CHECK(titlesOf(lib.query->allPs1Games(true, false)) == vector<string>{"Crash Bandicoot"});
}

TEST_CASE("a save-state folder shared with another game is not offered for deletion") {
    Catalog lib;
    // both games point at the same !SaveStates folder, which is what a multi-disc set looks like
    lib.library.usbGames().insertGame(1, "Disc One", "Publisher", 1, 1997, lib.tmp.at("Games/Disc One"),
                                      lib.tmp.at("Games/shared-ss"), "SONY");
    lib.library.usbGames().insertDisc(1, 1, "Disc One");
    lib.library.usbGames().insertGame(2, "Disc Two", "Publisher", 1, 1997, lib.tmp.at("Games/Disc Two"),
                                      lib.tmp.at("Games/shared-ss"), "SONY");
    lib.library.usbGames().insertDisc(2, 1, "Disc Two");
    lib.tmp.makeSubDir("Games/Disc One");
    lib.tmp.makeSubDir("Games/Disc Two");
    lib.tmp.makeSubDir("Games/shared-ss");
    lib.addSubDirRow(0, "Games", 0, 2);
    lib.putGameInSubDirRow(0, 1);
    lib.putGameInSubDirRow(0, 2);

    PsGamePtr discOne = findByTitle(lib.query->allPs1Games(true, false), "Disc One");
    REQUIRE(discOne);

    auto result = lib.catalog->deleteUsbGame(*discOne);
    CHECK(result.removed);
    CHECK_FALSE(result.saveStateFolderIsNowUnused); // Disc Two still uses it

    PsGamePtr discTwo = findByTitle(lib.query->allPs1Games(true, false), "Disc Two");
    REQUIRE(discTwo);
    auto second = lib.catalog->deleteUsbGame(*discTwo);
    CHECK(second.removed);
    CHECK(second.saveStateFolderIsNowUnused); // now nothing does
}

TEST_CASE("removeSaveStateFolder deletes the folder it is handed") {
    Catalog lib;
    lib.tmp.makeSubDir("Games/!SaveStates/Tekken 3");
    string folder = lib.tmp.at("Games/!SaveStates/Tekken 3");
    REQUIRE(ableem::DirEntry::exists(folder));

    CHECK(lib.catalog->removeSaveStateFolder(folder));
    CHECK_FALSE(ableem::DirEntry::exists(folder));
}

TEST_CASE("flushing covers removes every png under Games, at any depth") {
    Catalog lib;
    lib.tmp.makeSubDir("Games/Tekken 3");
    lib.tmp.makeSubDir("Games/Racing/Gran Turismo");
    lib.tmp.writeFile("Games/Tekken 3/Tekken 3.png", "cover");
    lib.tmp.writeFile("Games/Racing/Gran Turismo/Gran Turismo.png", "cover");
    lib.tmp.writeFile("Games/Tekken 3/Game.ini", "[Game]\n"); // left alone
    lib.tmp.writeFile("Games/Tekken 3/Tekken 3.cue", "cue");  // left alone

    CHECK(lib.catalog->flushAllCovers() == 2);

    CHECK_FALSE(ableem::DirEntry::exists(lib.tmp.at("Games/Tekken 3/Tekken 3.png")));
    CHECK_FALSE(ableem::DirEntry::exists(lib.tmp.at("Games/Racing/Gran Turismo/Gran Turismo.png")));
    CHECK(ableem::DirEntry::exists(lib.tmp.at("Games/Tekken 3/Game.ini")));
    CHECK(ableem::DirEntry::exists(lib.tmp.at("Games/Tekken 3/Tekken 3.cue")));
    // the directories themselves survive - only the covers go
    CHECK(ableem::DirEntry::exists(lib.tmp.at("Games/Racing/Gran Turismo")));
}

TEST_CASE("flushing covers with nothing to flush is zero, not an error") {
    Catalog lib;
    CHECK(lib.catalog->flushAllCovers() == 0);
}
