//
// GameSettingsService: a game's Game.ini flags and pcsx.cfg values, as the game editor edits them.
//
#include "doctest/doctest.h"

#include "../support/game_library_fixture.h"
#include "../support/string_maker.h"

#include "core/services/game_settings.h"

#include <memory>
#include <string>

using std::string;

namespace {

// The cfg files are rewritten in text mode, so a rewritten line ends in whatever the platform's endl is;
// the expectations below name a line without its ending.
bool contains(const string &text, const string &piece) {
    return text.find(piece) != string::npos;
}

// A pcsx.cfg with every line the editor reads. The levels are hex: 0x39 = 57, 0x46 = 70.
const char *const PcsxCfg = "gpu_neon.enhancement_enable = 0\n"
                            "gpu_neon.enhancement_no_main = 0\n"
                            "psx_clock = 39\n"
                            "Gpu3 = builtin_gpu\n"
                            "frameskip3 = 0\n"
                            "gpu_peops.iUseDither = 1\n"
                            "scanlines = 0\n"
                            "scanline_level = 46\n"
                            "spu_config.iUseInterpolation = 1\n";

// A library with one USB game and one internal game. The USB game's Game.ini is written by the fixture
// (Automation=1, Favorite=0) - tests that need more overwrite it.
struct Editing : GameLibraryFixture {
    Editing() {
        addUsbGame(1, "Driver 2");
        addSubDirRow(0, "Games", 0, 1);
        putGameInSubDirRow(0, 1);
        addInternalGame(10, "Jumping Flash");
        tmp.makeSubDir("Games/!MemCards");
        service = std::make_unique<GameSettingsService>(library);
    }

    PsGamePtr usbGame() {
        PsGames games = PsGame::fromRecords(library.usbGames().loadUsbGames());
        REQUIRE(games.size() == 1);
        return games[0];
    }

    // the record points its save states at /gaadata, which a test cannot write; move them into the tree
    PsGamePtr internalGame() {
        PsGames games = PsGame::fromRecords(library.internalGames().loadInternalGames());
        REQUIRE(games.size() == 1);
        games[0]->ssFolder = tmp.at("Games/!SaveStates/10");
        return games[0];
    }

    void writeGameIni(const string &body) { tmp.writeFile("Games/Driver 2/" + string(ableem::GAME_INI), body); }
    string readGameIni() const { return tmp.readFile("Games/Driver 2/" + string(ableem::GAME_INI)); }

    // the three places ConfigFileEditor::replaceUsb writes: the game's own copy and the two under !SaveStates
    void writeAllUsbCfgs(const string &body = PcsxCfg) {
        tmp.writeFile("Games/Driver 2/pcsx.cfg", body);
        tmp.writeFile("Games/!SaveStates/Driver 2/pcsx.cfg", body);
        tmp.writeFile("Games/!SaveStates/Driver 2/cfg/slot1.cfg", body);
    }

    std::unique_ptr<GameSettingsService> service;
};

} // namespace

TEST_CASE("open reads a USB game's Game.ini and its pcsx.cfg") {
    Editing lib;
    lib.writeGameIni(
        "[Game]\nTitle=Driver 2\nPublisher=Infogrames\nYear=2000\nPlayers=2\nMemcard=SONY\nAutomation=1\n");
    lib.tmp.writeFile("Games/Driver 2/pcsx.cfg", PcsxCfg);

    GameSettings s = lib.service->open(lib.usbGame());

    CHECK_FALSE(s.internal);
    CHECK(s.ini.path == lib.tmp.at("Games/Driver 2/Game.ini"));
    CHECK(s.ini.entry == "Driver 2"); // the folder name - what ConfigFileEditor keys the !SaveStates copies by
    CHECK(s.ini.values["title"] == "Driver 2");
    CHECK(s.ini.values["publisher"] == "Infogrames");
    CHECK(s.ini.values["memcard"] == "SONY");

    CHECK(s.pcsx.clock == 57);
    CHECK(s.pcsx.scanlineLevel == 70);
    CHECK(s.pcsx.interpolation == 1);
    CHECK(s.pcsx.dither == 1);
    CHECK(s.pcsx.gpu == "builtin_gpu");
}

TEST_CASE("open fills an internal game's ini in from the record, with no file behind it") {
    Editing lib;

    GameSettings s = lib.service->open(lib.internalGame());

    CHECK(s.internal);
    CHECK(s.ini.path == "");
    CHECK(s.ini.entry == "");
    CHECK(s.ini.values["title"] == "Jumping Flash");
    CHECK(s.ini.values["publisher"] == "Sony");
    CHECK(s.ini.values["year"] == "1995");
    CHECK(s.ini.values["players"] == "1");
    CHECK(s.ini.values["memcard"] == "SONY");
    CHECK(s.pcsx.clock == 0); // no pcsx.cfg under its !SaveStates folder yet
}

TEST_CASE("a card set the ini names but which is gone shows as SONY, in memory only") {
    Editing lib;
    lib.writeGameIni("[Game]\nTitle=Driver 2\nMemcard=Gone\nAutomation=1\n");

    GameSettings s = lib.service->open(lib.usbGame());
    CHECK(s.ini.values["memcard"] == "SONY");
    CHECK(contains(lib.readGameIni(), "Memcard=Gone")); // the file is not rewritten just for this

    // ...and a set that does exist is kept
    lib.tmp.makeSubDir("Games/!MemCards/Racing");
    lib.writeGameIni("[Game]\nTitle=Driver 2\nMemcard=Racing\nAutomation=1\n");
    s = lib.service->open(lib.usbGame());
    CHECK(s.ini.values["memcard"] == "Racing");
}

TEST_CASE("a USB game's favorite flag round-trips through its Game.ini") {
    Editing lib;
    GameSettings s = lib.service->open(lib.usbGame());

    lib.service->setFavorite(s, true);
    CHECK(s.ini.values["favorite"] == "1");
    CHECK(contains(lib.readGameIni(), "Favorite=1"));
    CHECK(lib.usbGame()->favorite); // loadUsbGames merges the ini, which is how the Favorites set sees it

    lib.service->setFavorite(s, false);
    CHECK(contains(lib.readGameIni(), "Favorite=0"));
    CHECK_FALSE(lib.usbGame()->favorite);

    // an ini with no Favorite key at all gets one
    lib.writeGameIni("[Game]\nTitle=Driver 2\nAutomation=1\n");
    s = lib.service->open(lib.usbGame());
    lib.service->setFavorite(s, true);
    CHECK(contains(lib.readGameIni(), "Favorite=1"));
}

TEST_CASE("an internal game's favorite and play-using-RA flags go to internal.db") {
    Editing lib;
    PsGamePtr game = lib.internalGame();
    GameSettings s = lib.service->open(game);

    lib.service->setFavorite(s, true);
    lib.service->setPlayUsingRa(s, true);
    CHECK(game->favorite); // the record in hand, which the editor renders from
    CHECK(game->play_using_ra);
    CHECK(lib.internalGame()->favorite); // and the database, re-read
    CHECK(lib.internalGame()->play_using_ra);
    CHECK(s.ini.path == ""); // nothing was written as an ini

    lib.service->setFavorite(s, false);
    CHECK_FALSE(lib.internalGame()->favorite);
}

TEST_CASE("play using RA is the strings true/false in a USB game's ini") {
    Editing lib;
    GameSettings s = lib.service->open(lib.usbGame());

    lib.service->setPlayUsingRa(s, true);
    CHECK(contains(lib.readGameIni(), "Play_using_ra=true"));
    CHECK(lib.usbGame()->play_using_ra);

    lib.service->setPlayUsingRa(s, false);
    CHECK(contains(lib.readGameIni(), "Play_using_ra=false"));
}

TEST_CASE("locking flips Automation, and only from its opposite value") {
    Editing lib;
    GameSettings s = lib.service->open(lib.usbGame()); // the fixture's ini says Automation=1
    REQUIRE(s.ini.values["automation"] == "1");

    lib.service->setLocked(s, true);
    CHECK(contains(lib.readGameIni(), "Automation=0"));
    CHECK(lib.usbGame()->locked);

    lib.service->setLocked(s, true); // already locked: still locked
    CHECK(contains(lib.readGameIni(), "Automation=0"));

    lib.service->setLocked(s, false);
    CHECK(contains(lib.readGameIni(), "Automation=1"));
    CHECK_FALSE(lib.usbGame()->locked);

    // Pinned, not endorsed: an ini with no Automation key is neither locked nor unlocked by this - the
    // editor only ever flipped the flag from its opposite value. The scanner always writes the key, so
    // nothing reaches this in practice.
    lib.writeGameIni("[Game]\nTitle=Driver 2\n");
    s = lib.service->open(lib.usbGame());
    lib.service->setLocked(s, true);
    CHECK(s.ini.values["automation"] == "");
    CHECK_FALSE(contains(lib.readGameIni(), "Automation=0"));
}

TEST_CASE("high res is written to every pcsx.cfg copy and remembered in the Game.ini") {
    Editing lib;
    lib.writeAllUsbCfgs();
    GameSettings s = lib.service->open(lib.usbGame());
    REQUIRE(s.pcsx.highres == 0);

    lib.service->setHighres(s, true);

    CHECK(s.pcsx.highres == 1); // read back from the file, not assumed
    CHECK(contains(lib.tmp.readFile("Games/Driver 2/pcsx.cfg"), "gpu_neon.enhancement_enable = 1"));
    CHECK(contains(lib.tmp.readFile("Games/!SaveStates/Driver 2/pcsx.cfg"), "gpu_neon.enhancement_enable = 1"));
    CHECK(contains(lib.tmp.readFile("Games/!SaveStates/Driver 2/cfg/slot1.cfg"), "gpu_neon.enhancement_enable = 1"));
    CHECK(contains(lib.readGameIni(), "Highres=1"));

    lib.service->setHighres(s, false);
    CHECK(s.pcsx.highres == 0);
    CHECK(contains(lib.readGameIni(), "Highres=0"));
}

TEST_CASE("an internal game's pcsx.cfg is the one under its !SaveStates folder, and no ini is written") {
    Editing lib;
    lib.tmp.writeFile("Games/!SaveStates/10/pcsx.cfg", PcsxCfg);
    lib.tmp.writeFile("Games/!SaveStates/10/cfg/slot1.cfg", PcsxCfg);
    GameSettings s = lib.service->open(lib.internalGame());
    REQUIRE(s.pcsx.clock == 57);

    lib.service->setHighres(s, true);
    lib.service->setSpeedhack(s, true);

    CHECK(s.pcsx.highres == 1);
    CHECK(s.pcsx.speedhack == 1);
    CHECK(contains(lib.tmp.readFile("Games/!SaveStates/10/pcsx.cfg"), "gpu_neon.enhancement_enable = 1"));
    CHECK(contains(lib.tmp.readFile("Games/!SaveStates/10/cfg/slot1.cfg"), "gpu_neon.enhancement_no_main = 1"));
    CHECK(s.ini.values["highres"] == "1"); // kept in the in-memory ini like a USB game's
    CHECK(s.ini.path == "");               // but there is no file to save it to

    // the USB-only settings do nothing for an internal game
    lib.service->setGpuPlugin(s, GameSettingsService::PeopsGpu);
    lib.service->setLocked(s, true);
    lib.service->setMemcard(s, "Racing");
    lib.service->rename(s, "Something else");
    CHECK(s.pcsx.gpu == "builtin_gpu");
    CHECK(s.ini.values["memcard"] == "SONY");
    CHECK(s.ini.values["title"] == "Jumping Flash");
}

TEST_CASE("the levels are written in hex and clamped to their ranges") {
    Editing lib;
    lib.writeAllUsbCfgs();
    GameSettings s = lib.service->open(lib.usbGame());

    lib.service->setClock(s, 100);
    CHECK(s.pcsx.clock == 100);
    CHECK(contains(lib.tmp.readFile("Games/Driver 2/pcsx.cfg"), "psx_clock = 64"));

    lib.service->setClock(s, 101);
    CHECK(s.pcsx.clock == 100);
    lib.service->setClock(s, -1);
    CHECK(s.pcsx.clock == 0);

    lib.service->setScanlineLevel(s, 71);
    CHECK(s.pcsx.scanlineLevel == 71);
    CHECK(contains(lib.tmp.readFile("Games/Driver 2/pcsx.cfg"), "scanline_level = 47"));
    lib.service->setScanlineLevel(s, 200);
    CHECK(s.pcsx.scanlineLevel == 100);

    lib.service->setFrameskip(s, 3);
    CHECK(s.pcsx.frameskip == 3);
    lib.service->setFrameskip(s, 4);
    CHECK(s.pcsx.frameskip == 3);

    lib.service->setInterpolation(s, 2);
    CHECK(s.pcsx.interpolation == 2);
    CHECK(contains(lib.tmp.readFile("Games/Driver 2/pcsx.cfg"), "spu_config.iUseInterpolation = 2"));
    lib.service->setInterpolation(s, 4);
    CHECK(s.pcsx.interpolation == 3);

    lib.service->setScanlines(s, true);
    CHECK(s.pcsx.scanlines == 1);
    CHECK(contains(lib.tmp.readFile("Games/Driver 2/pcsx.cfg"), "scanlines = 1"));
}

TEST_CASE("the GPU plugin line is Gpu3, and only a USB game has one") {
    Editing lib;
    lib.writeAllUsbCfgs();
    GameSettings s = lib.service->open(lib.usbGame());

    lib.service->setGpuPlugin(s, GameSettingsService::PeopsGpu);
    CHECK(s.pcsx.gpu == "gpu_peops.so");
    CHECK(contains(lib.tmp.readFile("Games/!SaveStates/Driver 2/pcsx.cfg"), "Gpu3 = gpu_peops.so"));

    lib.service->setGpuPlugin(s, GameSettingsService::BuiltinGpu);
    CHECK(s.pcsx.gpu == "builtin_gpu");
}

TEST_CASE("a pcsx.cfg with CRLF line endings reads clean values") {
    Editing lib;
    string crlf = PcsxCfg;
    for (string::size_type at = crlf.find('\n'); at != string::npos; at = crlf.find('\n', at + 2)) {
        crlf.replace(at, 1, "\r\n");
    }
    lib.writeAllUsbCfgs(crlf);

    GameSettings s = lib.service->open(lib.usbGame());
    CHECK(s.pcsx.clock == 57);
    CHECK(s.pcsx.gpu == "builtin_gpu"); // no trailing \r

    lib.service->setClock(s, 1);
    CHECK(s.pcsx.clock == 1);
    string written = lib.tmp.readFile("Games/Driver 2/pcsx.cfg");
    CHECK(contains(written, "psx_clock = 1"));
    CHECK(contains(
        written, "Gpu3 = builtin_gpu\r")); // untouched lines keep their \r; the rewritten one gets the platform's endl
}

TEST_CASE("a pcsx.cfg without the key is left alone, and the value reads as off") {
    Editing lib;
    lib.writeAllUsbCfgs("psx_clock = 39\n");
    GameSettings s = lib.service->open(lib.usbGame());

    lib.service->setScanlines(s, true);

    CHECK(s.pcsx.scanlines == 0); // ConfigFileEditor replaces lines, it never adds one
    CHECK(lib.tmp.readFile("Games/Driver 2/pcsx.cfg") == "psx_clock = 39\n");
}

TEST_CASE("a game with no pcsx.cfg at all reads every value as off and writes nothing") {
    Editing lib;
    GameSettings s = lib.service->open(lib.usbGame());

    CHECK(s.pcsx.highres == 0);
    CHECK(s.pcsx.gpu == "");

    lib.service->setSpeedhack(s, true);
    CHECK(s.pcsx.speedhack == 0);
    CHECK_FALSE(ableem::DirEntry::exists(lib.tmp.at("Games/Driver 2/pcsx.cfg")));
}

TEST_CASE("renaming a USB game writes the title and unlocks the ini for the scanner") {
    Editing lib;
    lib.writeGameIni("[Game]\nTitle=Driver 2\nAutomation=1\n");
    GameSettings s = lib.service->open(lib.usbGame());

    lib.service->rename(s, "Driver 2 - Back on the Streets");

    string ini = lib.readGameIni();
    CHECK(contains(ini, "Title=Driver 2 - Back on the Streets"));
    CHECK(contains(ini, "Automation=0"));
    CHECK(s.ini.values["title"] == "Driver 2 - Back on the Streets");
    // the database row is not touched here - GuiLauncher/GuiManager do that after the editor closes
    CHECK(lib.usbGame()->title == "Driver 2");
}

TEST_CASE("setMemcard records the set in the ini and nowhere else") {
    Editing lib;
    GameSettings s = lib.service->open(lib.usbGame());

    lib.service->setMemcard(s, "Fighting");

    CHECK(s.ini.values["memcard"] == "Fighting");
    CHECK(contains(lib.readGameIni(), "Memcard=Fighting"));
    // Pinned: the editor never wrote regional.db's MEMCARD column (MemcardService::setCardForGame does).
    // A launch reads the ini, so the stale column has no effect; it is recorded here so a change is deliberate.
    CHECK(lib.usbGame()->memcard == "SONY");

    lib.service->setMemcard(s, "SONY");
    CHECK(contains(lib.readGameIni(), "Memcard=SONY"));
}
