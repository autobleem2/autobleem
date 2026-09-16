//
// GameQueryService: which games each carousel set shows, and in what order.
//
#include "doctest/doctest.h"

#include "../support/game_library_fixture.h"
#include "../support/string_maker.h"
#include "../support/temp_dir.h"

#include "core/services/config.h"
#include "core/services/game_query.h"

#include <memory>
#include <string>
#include <vector>

using std::string;
using std::vector;

namespace {

// the titles in the order the service returned them - what almost every assertion below is about
vector<string> titlesOf(const PsGames &games) {
    vector<string> titles;
    for (const auto &game : games) titles.push_back(game->title);
    return titles;
}

// stands in for RetroArchService, so the PS1 sets are tested with no RetroArch tree at all
struct FakeRetroArch : RetroArchGames {
    PsGames playlist;
    string historyName = "History";
    string askedFor;

    PsGames gamesInPlaylist(const string &playlistName) override {
        askedFor = playlistName;
        return playlist;
    }
    string historyPlaylistName() override { return historyName; }

    void add(const string &title) {
        PsGamePtr game{new PsGame};
        game->title = title;
        game->foreign = true;
        playlist.push_back(game);
    }
};

// three USB games on one sub-dir row, plus two of the console's built-in ones
struct ThreeGames : GameLibraryFixture {
    ThreeGames() {
        addUsbGame(1, "Tekken 3");
        addUsbGame(2, "Crash Bandicoot");
        addUsbGame(3, "Ridge Racer");
        addSubDirRow(0, "Games", 0, 3);
        for (int id : {1, 2, 3}) putGameInSubDirRow(0, id);

        addInternalGame(10, "Battle Arena Toshinden");
        addInternalGame(11, "Jumping Flash");
    }
};

// Config reads config.ini out of the working path, so each test points that at its own tree
struct ConfigIn {
    ConfigIn(const TempDir &tmp, const string &contents) {
        ableem::Environment::setWorkingPath(tmp.path());
        tmp.writeFile("config.ini", contents);
        config.reset(new Config);
    }
    Config &operator*() { return *config; }
    std::unique_ptr<Config> config;
};

} // namespace

TEST_CASE("the PS1 all-games set is the USB games, sorted by title") {
    ThreeGames lib;
    ConfigIn cfg(lib.tmp, "Origames=false\n");
    GameQueryService query(lib.library, *cfg);

    GameSetSelection selection;
    selection.set = GameSet::PS1;
    selection.ps1SelectState = Ps1SelectState::AllGames;

    CHECK(titlesOf(query.gamesFor(selection)) ==
          vector<string>{"Crash Bandicoot", "Ridge Racer", "Tekken 3"});
}

TEST_CASE("origames=false pushes the internal-games views back to the sub-dir view") {
    ThreeGames lib;
    ConfigIn cfg(lib.tmp, "Origames=false\n");
    GameQueryService query(lib.library, *cfg);

    CHECK_FALSE(query.showInternalGames());

    SUBCASE("from AllGames") {
        GameSetSelection selection;
        selection.ps1SelectState = Ps1SelectState::AllGames;
        query.gamesFor(selection);
        CHECK(selection.ps1SelectState == Ps1SelectState::GamesSubdir);
    }
    SUBCASE("from InternalOnly") {
        GameSetSelection selection;
        selection.ps1SelectState = Ps1SelectState::InternalOnly;
        query.gamesFor(selection);
        CHECK(selection.ps1SelectState == Ps1SelectState::GamesSubdir);
    }
    SUBCASE("but Favorites is left alone") {
        GameSetSelection selection;
        selection.ps1SelectState = Ps1SelectState::Favorites;
        query.gamesFor(selection);
        CHECK(selection.ps1SelectState == Ps1SelectState::Favorites);
    }
}

TEST_CASE("origames=true mixes the internal games into the all-games set") {
    ThreeGames lib;
    ConfigIn cfg(lib.tmp, "Origames=true\n");
    GameQueryService query(lib.library, *cfg);

    CHECK(query.showInternalGames());

    GameSetSelection selection;
    selection.ps1SelectState = Ps1SelectState::AllGames;
    PsGames games = query.gamesFor(selection);

    CHECK(selection.ps1SelectState == Ps1SelectState::AllGames);   // not pushed off this time
    CHECK(titlesOf(games) == vector<string>{"Battle Arena Toshinden", "Crash Bandicoot",
                                            "Jumping Flash", "Ridge Racer", "Tekken 3"});
}

TEST_CASE("the internal-only set is just the built-in games") {
    ThreeGames lib;
    ConfigIn cfg(lib.tmp, "Origames=true\n");
    GameQueryService query(lib.library, *cfg);

    GameSetSelection selection;
    selection.ps1SelectState = Ps1SelectState::InternalOnly;

    CHECK(titlesOf(query.gamesFor(selection)) ==
          vector<string>{"Battle Arena Toshinden", "Jumping Flash"});
}

TEST_CASE("the favorites set is only the games flagged favorite") {
    ThreeGames lib;
    lib.markFavorite("Tekken 3");
    lib.markFavorite("Ridge Racer");
    ConfigIn cfg(lib.tmp, "Origames=false\n");
    GameQueryService query(lib.library, *cfg);

    // the loader returns rows in title order, and favorites() does not re-sort
    CHECK(titlesOf(query.favorites()) == vector<string>{"Ridge Racer", "Tekken 3"});
}

TEST_CASE("the history set is ordered most recently played first") {
    ThreeGames lib;
    // History is ranked 1..N with 1 the latest game played. These ranks are deliberately not in title
    // order, so that sorting by title instead would give a different answer and fail this test.
    lib.markHistory(1, 1);   // Tekken 3, the latest
    lib.markHistory(2, 2);   // Crash Bandicoot
    lib.markHistory(3, 3);   // Ridge Racer, the oldest
    ConfigIn cfg(lib.tmp, "Origames=false\n");
    GameQueryService query(lib.library, *cfg);

    GameSetSelection selection;
    selection.ps1SelectState = Ps1SelectState::History;

    // by history rank, not by title
    CHECK(titlesOf(query.gamesFor(selection)) ==
          vector<string>{"Tekken 3", "Crash Bandicoot", "Ridge Racer"});
}

TEST_CASE("a game with no history rank is not in the history set") {
    ThreeGames lib;
    lib.markHistory(2, 1);
    ConfigIn cfg(lib.tmp, "Origames=false\n");
    GameQueryService query(lib.library, *cfg);

    CHECK(titlesOf(query.history()) == vector<string>{"Crash Bandicoot"});
}

TEST_CASE("sub-dir rows return only the games on that row") {
    GameLibraryFixture lib;
    lib.addUsbGame(1, "Tekken 3");
    lib.addUsbGame(2, "Gran Turismo");
    lib.addSubDirRow(0, "Games", 0, 2);
    lib.putGameInSubDirRow(0, 1);
    lib.putGameInSubDirRow(0, 2);
    lib.addSubDirRow(1, "Racing", 1, 1);
    lib.putGameInSubDirRow(1, 2);

    ConfigIn cfg(lib.tmp, "Origames=false\n");
    GameQueryService query(lib.library, *cfg);

    CHECK(titlesOf(query.ps1GamesInSubDirRow(0)) == vector<string>{"Gran Turismo", "Tekken 3"});
    CHECK(titlesOf(query.ps1GamesInSubDirRow(1)) == vector<string>{"Gran Turismo"});
}

TEST_CASE("the sub-dir view reports the name of the row it landed on") {
    GameLibraryFixture lib;
    lib.addUsbGame(1, "Gran Turismo");
    lib.addSubDirRow(0, "Games", 0, 1);
    lib.putGameInSubDirRow(0, 1);
    lib.addSubDirRow(1, "Racing", 1, 1);
    lib.putGameInSubDirRow(1, 1);

    ConfigIn cfg(lib.tmp, "Origames=false\n");
    GameQueryService query(lib.library, *cfg);

    GameSetSelection selection;
    selection.ps1SelectState = Ps1SelectState::GamesSubdir;
    selection.usbGameDirIndex = 1;

    query.gamesFor(selection);

    // the "Showing: USB Games Directory: X" line reads this back
    CHECK(selection.usbGameDirName == "Racing");
}

TEST_CASE("an out-of-range sub-dir row is empty rather than undefined") {
    GameLibraryFixture lib;
    lib.addUsbGame(1, "Tekken 3");
    lib.addSubDirRow(0, "Games", 0, 1);
    lib.putGameInSubDirRow(0, 1);

    ConfigIn cfg(lib.tmp, "Origames=false\n");
    GameQueryService query(lib.library, *cfg);

    string rowName = "untouched";
    CHECK(query.ps1GamesInSubDirRow(99, &rowName).empty());
    CHECK(query.ps1GamesInSubDirRow(-1, &rowName).empty());
    CHECK(rowName == "untouched");
}

TEST_CASE("a library with no games answers empty, not garbage") {
    GameLibraryFixture lib;
    ConfigIn cfg(lib.tmp, "Origames=true\n");
    GameQueryService query(lib.library, *cfg);

    CHECK(query.ps1GamesInSubDirRow(0).empty());
    CHECK(query.allPs1Games(true, true).empty());
    CHECK(query.favorites().empty());
    CHECK(query.history().empty());
}

TEST_CASE("the RetroArch set comes from the playlist and is sorted by title") {
    ThreeGames lib;
    ConfigIn cfg(lib.tmp, "Origames=false\n");
    GameQueryService query(lib.library, *cfg);

    FakeRetroArch retroArch;
    retroArch.add("Sonic");
    retroArch.add("Altered Beast");
    query.setRetroArchGames(&retroArch);

    GameSetSelection selection;
    selection.set = GameSet::RetroArch;
    selection.raPlaylistName = "Sega - Mega Drive.lpl";

    PsGames games = query.gamesFor(selection);

    CHECK(retroArch.askedFor == "Sega - Mega Drive.lpl");
    CHECK(titlesOf(games) == vector<string>{"Altered Beast", "Sonic"});
}

TEST_CASE("the RetroArch history playlist keeps the order it arrived in") {
    ThreeGames lib;
    ConfigIn cfg(lib.tmp, "Origames=false\n");
    GameQueryService query(lib.library, *cfg);

    FakeRetroArch retroArch;
    retroArch.add("Sonic");           // most recently played
    retroArch.add("Altered Beast");
    query.setRetroArchGames(&retroArch);

    GameSetSelection selection;
    selection.set = GameSet::RetroArch;
    selection.raPlaylistName = retroArch.historyName;

    // not re-sorted by title: the playlist is already most-recent-first
    CHECK(titlesOf(query.gamesFor(selection)) == vector<string>{"Sonic", "Altered Beast"});
}

TEST_CASE("with no RetroArch wired up the set is empty rather than a crash") {
    ThreeGames lib;
    ConfigIn cfg(lib.tmp, "Origames=false\n");
    GameQueryService query(lib.library, *cfg);   // setRetroArchGames never called

    GameSetSelection selection;
    selection.set = GameSet::RetroArch;
    selection.raPlaylistName = "Anything.lpl";

    CHECK(query.gamesFor(selection).empty());
}

TEST_CASE("the Apps set is built from each Apps/<name>/app.ini") {
    GameLibraryFixture lib;
    ConfigIn cfg(lib.tmp, "Origames=false\n");
    GameQueryService query(lib.library, *cfg);

    lib.tmp.makeSubDir("Apps/Wifi");
    lib.tmp.writeFile("Apps/Wifi/app.ini",
                      "Title=Wifi Setup\n"
                      "Author=screemer\n"
                      "Startup=wifi.sh\n"
                      "Image=icon.png\n"
                      "Readme=readme.txt\n"
                      "Kernel=true\n");
    lib.tmp.makeSubDir("Apps/NotAnApp");   // no app.ini, so it is skipped

    PsGames apps = query.apps();

    REQUIRE(apps.size() == 1);
    CHECK(apps[0]->title == "Wifi Setup");
    CHECK(apps[0]->publisher == "screemer");
    CHECK(apps[0]->startup == "wifi.sh");
    CHECK(apps[0]->kernel);
    CHECK(apps[0]->app);
    CHECK(apps[0]->foreign);   // no database row, so nothing may treat it as a PS1 game
    CHECK(apps[0]->base == lib.tmp.at("Apps/Wifi"));
}

TEST_CASE("no Apps directory at all is empty, not an error") {
    GameLibraryFixture lib;
    ConfigIn cfg(lib.tmp, "Origames=false\n");
    GameQueryService query(lib.library, *cfg);

    CHECK(query.apps().empty());
}
