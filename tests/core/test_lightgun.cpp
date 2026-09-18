//
// LightgunService: the RetroArch list in System/lightguns.txt, the PS1 flag through the settings service,
// and the Lightgun set the query builds from both.
//
#include "doctest/doctest.h"

#include "../support/game_library_fixture.h"
#include "../support/string_maker.h"
#include "../support/temp_dir.h"

#include "core/services/config.h"
#include "core/services/game_query.h"
#include "core/services/game_settings.h"
#include "core/services/lightgun.h"

#include <memory>
#include <string>
#include <vector>

using std::string;
using std::vector;

namespace {

bool contains(const string &haystack, const string &needle) {
    return haystack.find(needle) != string::npos;
}

struct FakeRetroArch : RetroArchGames {
    PsGames games;
    PsGames gamesInPlaylist(const string &) override { return games; }
    string historyPlaylistName() override { return "History"; }
    PsGames allGames() override { return games; }
    PsGamePtr add(const string &title, const string &imagePath) {
        PsGamePtr game = std::make_shared<PsGame>();
        game->title = title;
        game->foreign = true;
        game->image_path = imagePath;
        games.push_back(game);
        return game;
    }
};

struct Lib : GameLibraryFixture {
    Lib() {
        addUsbGame(1, "Time Crisis");
        addUsbGame(2, "Crash Bandicoot");
        addSubDirRow(0, "Games", 0, 2);
        putGameInSubDirRow(0, 1);
        putGameInSubDirRow(0, 2);
        addInternalGame(10, "Point Blank");
        tmp.makeSubDir("System");
        tmp.makeSubDir("Games/!MemCards");
        ableem::Environment::setWorkingPath(tmp.path());
        tmp.writeFile("config.ini", "Origames=true\n");
        config = std::make_unique<Config>();
        settings = std::make_unique<GameSettingsService>(library);
    }
    PsGamePtr usbGame(const string &title) {
        for (auto &g : PsGame::fromRecords(library.usbGames().loadUsbGames()))
            if (g->title == title)
                return g;
        REQUIRE(false);
        return nullptr;
    }
    PsGamePtr internalGame() {
        PsGames games = PsGame::fromRecords(library.internalGames().loadInternalGames());
        REQUIRE(games.size() == 1);
        return games[0];
    }
    vector<string> titlesOf(const PsGames &games) {
        vector<string> titles;
        for (const auto &g : games)
            titles.push_back(g->title);
        return titles;
    }
    std::unique_ptr<Config> config;
    std::unique_ptr<GameSettingsService> settings;
};

} // namespace

TEST_CASE("a USB game's light-gun flag round-trips through Game.ini and switches Play using RA on") {
    Lib lib;
    GameSettings s = lib.settings->open(lib.usbGame("Time Crisis"));
    CHECK_FALSE(s.game->play_using_ra);

    lib.settings->setLightgun(s, true);
    CHECK(s.game->lightgun);
    CHECK(s.game->play_using_ra); // guncon lives in RetroArch's core
    string ini = lib.tmp.readFile("Games/Time Crisis/Game.ini");
    CHECK(contains(ini, "Lightgun=1"));
    CHECK(contains(ini, "Play_using_ra=true"));
    CHECK(lib.usbGame("Time Crisis")->lightgun); // the loader reads it back

    lib.settings->setLightgun(s, false);
    CHECK_FALSE(lib.usbGame("Time Crisis")->lightgun);
    CHECK(lib.usbGame("Time Crisis")->play_using_ra); // left as it was

    LightgunService lightguns(lib.library);
    CHECK_FALSE(lightguns.isLightgun(*lib.usbGame("Time Crisis")));
    lib.settings->setLightgun(s, true);
    CHECK(lightguns.isLightgun(*lib.usbGame("Time Crisis")));
}

TEST_CASE("an internal game's light-gun flag goes to internal.db") {
    Lib lib;
    PsGamePtr game = lib.internalGame();
    GameSettings s = lib.settings->open(game);
    lib.settings->setLightgun(s, true);
    CHECK(lib.internalGame()->lightgun);
    CHECK(lib.internalGame()->play_using_ra);
    lib.settings->setLightgun(s, false);
    CHECK_FALSE(lib.internalGame()->lightgun);
}

TEST_CASE("RetroArch games are listed in System/lightguns.txt by image path; a gone path is purged on load") {
    Lib lib;
    FakeRetroArch ra;
    lib.tmp.makeSubDir("roms");
    lib.tmp.writeFile("roms/House of the Dead.chd", "");
    lib.tmp.writeFile("roms/Virtua Cop.chd", "");
    PsGamePtr hotd = ra.add("House of the Dead", lib.tmp.at("roms/House of the Dead.chd"));
    PsGamePtr vcop = ra.add("Virtua Cop", lib.tmp.at("roms/Virtua Cop.chd"));

    LightgunService lightguns(lib.library);
    CHECK_FALSE(lightguns.anyRetroArchLightguns());
    CHECK_FALSE(lightguns.isLightgun(*hotd));

    lightguns.setRetroArchLightgun(*hotd, true);
    lightguns.setRetroArchLightgun(*vcop, true);
    CHECK(lightguns.isLightgun(*hotd));
    CHECK(lightguns.isLightgun(*vcop));
    string file = lib.tmp.readFile("System/lightguns.txt");
    CHECK(contains(file, "House of the Dead.chd"));
    CHECK(contains(file, "Virtua Cop.chd"));

    lightguns.setRetroArchLightgun(*vcop, false);
    CHECK_FALSE(lightguns.isLightgun(*vcop));
    CHECK_FALSE(contains(lib.tmp.readFile("System/lightguns.txt"), "Virtua Cop"));

    // a second instance reads the file; the game removed from disk is dropped from it
    ableem::DirEntry::removeFile(lib.tmp.at("roms/House of the Dead.chd"));
    LightgunService again(lib.library);
    CHECK_FALSE(again.isLightgun(*hotd));
    CHECK_FALSE(again.anyRetroArchLightguns());
    CHECK_FALSE(ableem::DirEntry::exists(lib.tmp.at("System/lightguns.txt"))); // empty list: no file

    PsGamePtr app = std::make_shared<PsGame>();
    app->foreign = true;
    app->app = true;
    app->image_path = "x";
    CHECK_FALSE(again.isLightgun(*app));
}

TEST_CASE("the Lightgun set is every flagged PS1 and RetroArch game, by title; empty with no service") {
    Lib lib;
    FakeRetroArch ra;
    lib.tmp.makeSubDir("roms");
    lib.tmp.writeFile("roms/Virtua Cop.chd", "");
    PsGamePtr vcop = ra.add("Virtua Cop", lib.tmp.at("roms/Virtua Cop.chd"));
    ra.add("Sonic", "roms/Sonic.md");

    GameSettings s = lib.settings->open(lib.usbGame("Time Crisis"));
    lib.settings->setLightgun(s, true);
    GameSettings i = lib.settings->open(lib.internalGame());
    lib.settings->setLightgun(i, true);

    LightgunService lightguns(lib.library);
    lightguns.setRetroArchLightgun(*vcop, true);

    GameQueryService query(lib.library, *lib.config);
    query.setRetroArchGames(&ra);
    GameSetSelection selection;
    selection.set = GameSet::Lightgun;
    CHECK(query.gamesFor(selection).empty()); // no LightgunService yet

    query.setLightguns(&lightguns);
    CHECK(lib.titlesOf(query.gamesFor(selection)) == vector<string>{"Point Blank", "Time Crisis", "Virtua Cop"});

    lib.tmp.writeFile("config.ini", "Origames=false\n");
    Config noInternal;
    GameQueryService query2(lib.library, noInternal);
    query2.setRetroArchGames(&ra);
    query2.setLightguns(&lightguns);
    CHECK(lib.titlesOf(query2.gamesFor(selection)) == vector<string>{"Time Crisis", "Virtua Cop"});
}
