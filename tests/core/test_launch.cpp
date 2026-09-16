//
// LaunchService: what a game launch hands to rc/launch.sh / rc/launch_rb.sh, and what happens around it.
//
#include "doctest/doctest.h"

#include "../support/fake_process_runner.h"
#include "../support/game_library_fixture.h"
#include "../support/string_maker.h"

#include "core/services/launch.h"

#include <memory>
#include <string>
#include <vector>

using std::string;
using std::vector;

namespace {

bool contains(const string &text, const string &piece) {
    return text.find(piece) != string::npos;
}

// A library with one USB game and one internal game, the services a launch goes through, and a runner that
// records the argv instead of forking. config.ini is written per test through configure().
struct Launching : GameLibraryFixture {
    Launching() {
        addUsbGame(1, "Tekken 3");
        addSubDirRow(0, "Games", 0, 1);
        putGameInSubDirRow(0, 1);
        addInternalGame(10, "Jumping Flash");

        // MemcardManager::create copies these blanks into a new set
        tmp.makeSubDir("memcard");
        tmp.writeFile("memcard/card1.mcd", "blank one");
        tmp.writeFile("memcard/card2.mcd", "blank two");
        tmp.makeSubDir("Games/!MemCards");
        ableem::Environment::setWorkingPath(tmp.path());

        configure("");
    }

    // Config reads config.ini out of the working path and fills in its defaults; `cfg` is where the
    // selection script goes, which is not defaulted
    void configure(const string &contents) {
        tmp.writeFile("config.ini", "Cfg=" + tmp.at("autobleem_cfg.sh") + "\nTheme=aergb\n" + contents);
        config.reset(new Config);
        memcards.reset(new MemcardService(library));
        resumePoints.reset(new ResumePointService);
        service.reset(new LaunchService(*config, session, library, *memcards, *resumePoints, runner));
    }

    PsGamePtr usbGame() {
        PsGames games = PsGame::fromRecords(library.usbGames().loadUsbGames());
        REQUIRE(games.size() == 1);
        return games[0];
    }

    PsGamePtr internalGame() {
        PsGames games = PsGame::fromRecords(library.internalGames().loadInternalGames());
        REQUIRE(games.size() == 1);
        return games[0];
    }

    // a RetroArch playlist entry or an App: not one of ours, so no memory cards and no pcsx.cfg
    PsGamePtr foreignGame(bool app) {
        PsGamePtr game(new PsGame);
        game->foreign = true;
        game->app = app;
        game->title = app ? "Some App" : "Some ROM";
        game->base = app ? "/media/Apps/SomeApp" : "rom";
        game->startup = "run.sh";
        game->image_path = "/media/roms/snes/rom.sfc";
        game->core_path = "/media/retroarch/cores/snes9x_libretro.so";
        return game;
    }

    string ssFile(const PsGamePtr &game, const string &relative) const {
        return game->ssFolder + ableem::sep + relative;
    }
    string read(const string &fullPath) const {
        return tmp.readFile(fullPath.substr(tmp.path().size() + 1));
    }
    string rcScript(const string &name) const { return tmp.at("Autobleem/rc/" + name); }

    Session session;
    FakeProcessRunner runner;
    std::unique_ptr<Config> config;
    std::unique_ptr<MemcardService> memcards;
    std::unique_ptr<ResumePointService> resumePoints;
    std::unique_ptr<LaunchService> service;
};

} // namespace

// --- the selection script ---

TEST_CASE("writeSelectionScript records the menu choice and the settings the rc scripts read") {
    Launching lib;
    lib.configure("Mip=true\n");
    lib.session.menuOption = MENU_OPTION_START;

    lib.service->writeSelectionScript();

    // written in text mode, so the line endings are the platform's; the lines are what is asserted
    string script = lib.tmp.readFile("autobleem_cfg.sh");
    CHECK(contains(script, "#!/bin/sh"));
    CHECK(contains(script, "AB_SELECTION=5"));
    CHECK(contains(script, "AB_THEME=aergb"));
    CHECK(contains(script, "AB_PCSX=bleemsync"));
    CHECK(contains(script, "AB_MIP=true"));
}

// --- PCSX ---

TEST_CASE("PCSX is started through rc/launch.sh with the nine arguments the script reads") {
    Launching lib;
    PsGamePtr game = lib.usbGame();
    game->ssFolder += "/";   // as the database sometimes has it; the script must not get the slash

    lib.service->launch(game, EmuMode::Pcsx, -1);

    const FakeProcessRunner::Call &call = lib.runner.only();
    CHECK(call.exe == lib.rcScript("launch.sh"));
    CHECK(call.args == vector<string>{lib.tmp.at("Games/Tekken 3/sstates"),      // ssFolder
                                      lib.tmp.at("Games/Tekken 3/Tekken 3.cue"),  // cdfile
                                      "2",                                        // lang (no lang file loaded)
                                      "2",                                        // region
                                      lib.tmp.at("Games/Tekken 3"),               // gameFolder
                                      "0",                                        // resume
                                      "0",                                        // aspect
                                      "0",                                        // filter
                                      "NA"});                                     // pad
    CHECK(game->ssFolder == lib.tmp.at("Games/Tekken 3/sstates"));   // normalised in place, as always

    CHECK(lib.usbGame()->last_played > 0);                            // the launch is the "last played" time
    CHECK(contains(lib.tmp.readFile("autobleem_cfg.sh"), "AB_SELECTION="));   // written before the run
}

TEST_CASE("the aspect and filter arguments come from config.ini") {
    Launching lib;
    lib.configure("Aspect=true\nMip=true\n");
    PsGamePtr game = lib.usbGame();

    lib.service->launch(game, EmuMode::Pcsx, -1);

    const vector<string> &args = lib.runner.only().args;
    CHECK(args[6] == "1");
    CHECK(args[7] == "1");
}

TEST_CASE("a .pbp or .chd image is handed over as it is; anything else gets .cue") {
    Launching lib;
    PsGamePtr game = lib.usbGame();

    game->base = "Tekken 3.pbp";
    lib.service->launch(game, EmuMode::Pcsx, -1);
    CHECK(lib.runner.calls[0].args[1] == lib.tmp.at("Games/Tekken 3/Tekken 3.pbp"));

    game->base = "Tekken 3.chd";
    lib.service->launch(game, EmuMode::Pcsx, -1);
    CHECK(lib.runner.calls[1].args[1] == lib.tmp.at("Games/Tekken 3/Tekken 3.chd"));

    game->base = "Tekken 3";
    lib.service->launch(game, EmuMode::Pcsx, -1);
    CHECK(lib.runner.calls[2].args[1] == lib.tmp.at("Games/Tekken 3/Tekken 3.cue"));
}

TEST_CASE("resuming a slot hands PCSX the disc that slot was playing, and says so") {
    Launching lib;
    PsGamePtr game = lib.usbGame();
    // what ResumePointService kept for slot 1: the disc image on the first line, the state name on the second
    lib.tmp.writeFile("Games/Tekken 3/sstates/filename.1.txt.res", "/media/Games/Tekken 3/Disc 2.cue\nstate\n");

    lib.service->launch(game, EmuMode::Pcsx, 1);

    const vector<string> &args = lib.runner.only().args;
    CHECK(args[1] == lib.tmp.at("Games/Tekken 3/Disc 2.cue"));
    CHECK(args[5] == "1");
    // the slot's disc note is also left where PCSX reads the "last disc" from
    CHECK(contains(lib.tmp.readFile("Games/Tekken 3/sstates/lastcdimg.txt"), lib.tmp.at("Games/Tekken 3/Disc 2.cue")));
}

TEST_CASE("the config PCSX writes on exit is copied back where it is read from, with the Bios line reset") {
    Launching lib;
    PsGamePtr game = lib.usbGame();
    lib.runner.whileRunning = [&] {
        lib.tmp.writeFile("Games/Tekken 3/sstates/autobleem.cfg", "Bios = /tmp/scph1001.bin\nGpu3 = builtin_gpu\n");
    };

    lib.service->launch(game, EmuMode::Pcsx, -1);

    CHECK_FALSE(ableem::DirEntry::exists(lib.tmp.at("Games/Tekken 3/sstates/autobleem.cfg")));
    for (const char *copy : {"Games/Tekken 3/sstates/pcsx.cfg", "Games/Tekken 3/pcsx.cfg"}) {
        string cfg = lib.tmp.readFile(copy);
        CHECK(contains(cfg, "Bios = SET_BY_PCSX"));
        CHECK(contains(cfg, "Gpu3 = builtin_gpu"));
    }
}

TEST_CASE("an internal game's config only goes back under !SaveStates - there is no game folder to write") {
    Launching lib;
    PsGamePtr game = lib.internalGame();
    lib.tmp.makeSubDir("Games/!SaveStates/10");
    lib.runner.whileRunning = [&] {
        lib.tmp.writeFile("Games/!SaveStates/10/autobleem.cfg", "Bios = /tmp/scph1001.bin\n");
    };

    lib.service->launch(game, EmuMode::Pcsx, -1);

    CHECK(lib.runner.only().args[0] == lib.tmp.at("Games/!SaveStates/10"));
    CHECK(contains(lib.tmp.readFile("Games/!SaveStates/10/pcsx.cfg"), "Bios = SET_BY_PCSX"));
    CHECK(lib.internalGame()->last_played > 0);
}

TEST_CASE("the game's memory-card set is in play while PCSX runs and is written back afterwards") {
    Launching lib;
    PsGamePtr game = lib.usbGame();
    lib.memcards->createCard("Fighting");
    lib.memcards->setCardForGame(*game, "Fighting");
    lib.tmp.writeFile("Games/!MemCards/Fighting/card1.mcd", "fighting saves");
    lib.tmp.writeFile("Games/Tekken 3/sstates/memcards/card1.mcd", "the game's own");

    string inPlay;
    lib.runner.whileRunning = [&] {
        inPlay = lib.tmp.readFile("Games/Tekken 3/sstates/memcards/card1.mcd");
        lib.tmp.writeFile("Games/Tekken 3/sstates/memcards/card1.mcd", "fighting saves + this run");
    };

    lib.service->launch(game, EmuMode::Pcsx, -1);

    CHECK(inPlay == "fighting saves");
    CHECK(lib.tmp.readFile("Games/!MemCards/Fighting/card1.mcd") == "fighting saves + this run");
    CHECK(lib.tmp.readFile("Games/Tekken 3/sstates/memcards/card1.mcd") == "the game's own");
}

// --- RetroArch ---

TEST_CASE("RetroArch is started through rc/launch_rb.sh with the image and the core for the game's GPU plugin") {
    Launching lib;
    lib.configure("Raconfig=false\n");
    PsGamePtr game = lib.usbGame();

    SUBCASE("no pcsx.cfg, or the built-in GPU: the NEON core") {
        lib.service->launch(game, EmuMode::RetroArch, -1);
        const FakeProcessRunner::Call &call = lib.runner.only();
        CHECK(call.exe == lib.rcScript("launch_rb.sh"));
        CHECK(call.args == vector<string>{lib.tmp.at("Games/Tekken 3/Tekken 3.cue"), "NEON"});
    }
    SUBCASE("the P.E.Op.S. plugin: the PEOPS core") {
        lib.tmp.writeFile("Games/Tekken 3/pcsx.cfg", "Gpu3 = gpu_peops.so\n");
        lib.service->launch(game, EmuMode::RetroArch, -1);
        CHECK(lib.runner.only().args[1] == "PEOPS");
    }
    SUBCASE("a multi-disc .m3u next to the image is preferred") {
        lib.tmp.writeFile("Games/Tekken 3/Tekken 3.m3u", "Tekken 3.cue\n");
        lib.service->launch(game, EmuMode::RetroArch, -1);
        CHECK(lib.runner.only().args[0] == lib.tmp.at("Games/Tekken 3/Tekken 3.m3u"));
    }

    // Pinned, not endorsed: a RetroArch launch never recorded a "last played" time - only PCSX and Apps did.
    CHECK(lib.usbGame()->last_played == 0);
}

TEST_CASE("a foreign RetroArch game is its playlist image and its own core") {
    Launching lib;
    lib.configure("Raconfig=false\n");
    PsGamePtr game = lib.foreignGame(false);

    lib.service->launch(game, EmuMode::RetroArch, -1);

    CHECK(lib.runner.only().args == vector<string>{"/media/roms/snes/rom.sfc",
                                                   "/media/retroarch/cores/snes9x_libretro.so"});
}

TEST_CASE("the game's card1.mcd is RetroArch's .srm for the run, and what RetroArch saved comes back") {
    Launching lib;
    lib.configure("Raconfig=false\n");
    PsGamePtr game = lib.usbGame();
    lib.tmp.writeFile("Games/Tekken 3/sstates/memcards/card1.mcd", "the game's own");
    lib.tmp.writeFile("retroarch/saves/Tekken 3.srm", "whatever RetroArch had");

    string srmInPlay, srmBackup;
    lib.runner.whileRunning = [&] {
        srmInPlay = lib.tmp.readFile("retroarch/saves/Tekken 3.srm");
        srmBackup = lib.tmp.readFile("retroarch/saves/Tekken 3.srm.bak");
        lib.tmp.writeFile("retroarch/saves/Tekken 3.srm", "the game's own + this run");
    };

    lib.service->launch(game, EmuMode::RetroArch, -1);

    CHECK(srmInPlay == "the game's own");
    CHECK(srmBackup == "whatever RetroArch had");
    CHECK(lib.tmp.readFile("Games/Tekken 3/sstates/memcards/card1.mcd") == "the game's own + this run");
    CHECK(lib.tmp.readFile("retroarch/saves/Tekken 3.srm") == "whatever RetroArch had");
    CHECK_FALSE(ableem::DirEntry::exists(lib.tmp.at("retroarch/saves/Tekken 3.srm.bak")));
}

TEST_CASE("with raconfig on, the game's pcsx.cfg settings are RetroArch's for the run and are put back after") {
    Launching lib;
    lib.configure("Raconfig=true\nAspect=true\nMip=false\n");
    PsGamePtr game = lib.usbGame();
    lib.tmp.writeFile("Games/Tekken 3/pcsx.cfg",
                      "gpu_neon.enhancement_enable = 1\n"
                      "gpu_neon.enhancement_no_main = 0\n"
                      "psx_clock = 39\n"                    // 0x39 = 57
                      "gpu_peops.iUseDither = 1\n"
                      "spu_config.iUseInterpolation = 2\n"
                      "scanlines = 1\n"
                      "scanline_level = 32\n"               // 0x32 = 50 -> opacity 0.5
                      "frameskip3 = 1\n");
    const string coreOptionsBefore =
        "pcsx_rearmed_neon_enhancement_enable = \"disabled\"\n"
        "pcsx_rearmed_neon_enhancement_no_main = \"enabled\"\n"
        "pcsx_rearmed_dithering = \"disabled\"\n"
        "pcsx_rearmed_psxclock = \"50\"\n"
        "pcsx_rearmed_spu_interpolation = \"off\"\n"
        "pcsx_rearmed_frameskip = \"0\"\n"
        "pcsx_rearmed_show_bios_bootlogo = \"disabled\"\n"
        "pcsx_rearmed_nocdaudio = \"disabled\"\n";
    const string raConfigBefore =
        "input_overlay = \"\"\n"
        "input_overlay_enable = \"false\"\n"
        "input_overlay_opacity = \"1.0\"\n"
        "custom_viewport_width = \"960\"\n"
        "custom_viewport_height = \"720\"\n"
        "custom_viewport_x = \"160\"\n"
        "custom_viewport_y = \"0\"\n"
        "aspect_ratio_index = \"0\"\n"
        "video_smooth = \"false\"\n";
    lib.tmp.writeFile("retroarch/config/retroarch-core-options.cfg", coreOptionsBefore);
    lib.tmp.writeFile("retroarch/retroarch.cfg", raConfigBefore);

    string coreOptionsInPlay, raConfigInPlay;
    lib.runner.whileRunning = [&] {
        coreOptionsInPlay = lib.tmp.readFile("retroarch/config/retroarch-core-options.cfg");
        raConfigInPlay = lib.tmp.readFile("retroarch/retroarch.cfg");
    };

    lib.service->launch(game, EmuMode::RetroArch, -1);

    // the core options, from the game's pcsx.cfg
    CHECK(contains(coreOptionsInPlay, "pcsx_rearmed_neon_enhancement_enable = \"enabled\""));
    CHECK(contains(coreOptionsInPlay, "pcsx_rearmed_neon_enhancement_no_main = \"disabled\""));
    CHECK(contains(coreOptionsInPlay, "pcsx_rearmed_dithering = \"enabled\""));
    CHECK(contains(coreOptionsInPlay, "pcsx_rearmed_psxclock = \"57\""));
    CHECK(contains(coreOptionsInPlay, "pcsx_rearmed_spu_interpolation = \"gaussian\""));
    CHECK(contains(coreOptionsInPlay, "pcsx_rearmed_frameskip  = \"1\""));
    CHECK(contains(coreOptionsInPlay, "pcsx_rearmed_show_bios_bootlogo  = \"enabled\""));
    CHECK(contains(coreOptionsInPlay, "pcsx_rearmed_nocdaudio  = \"enabled\""));
    // retroarch.cfg: the scanline overlay from pcsx.cfg, the viewport and filter from config.ini
    // "input_overlay" is a prefix of the next two keys; each line must be matched as a whole key or the
    // first replacement clobbers the other two (it did, until 2026-09-16)
    CHECK(contains(raConfigInPlay, "input_overlay  = \":/overlay/scanlines.cfg\""));
    CHECK(contains(raConfigInPlay, "input_overlay_enable  = \"true\""));
    CHECK(contains(raConfigInPlay, "input_overlay_opacity  = \"0.500000\""));
    CHECK(contains(raConfigInPlay, "custom_viewport_width  = \"1280\""));
    CHECK(contains(raConfigInPlay, "custom_viewport_x  = \"0\""));
    CHECK(contains(raConfigInPlay, "aspect_ratio_index  = \"23\""));
    CHECK(contains(raConfigInPlay, "video_smooth  = \"true\""));   // mip=false means smooth on; it always has

    // and afterwards both are exactly what they were
    CHECK(lib.tmp.readFile("retroarch/config/retroarch-core-options.cfg") == coreOptionsBefore);
    CHECK(lib.tmp.readFile("retroarch/retroarch.cfg") == raConfigBefore);
    CHECK_FALSE(ableem::DirEntry::exists(lib.tmp.at("retroarch/retroarch.cfg.bak")));
}

TEST_CASE("a foreign game with raconfig on still gets the viewport and filter, but no core options") {
    Launching lib;
    lib.configure("Raconfig=true\nAspect=false\nMip=true\n");
    PsGamePtr game = lib.foreignGame(false);
    lib.tmp.writeFile("retroarch/config/retroarch-core-options.cfg", "pcsx_rearmed_psxclock = \"50\"\n");
    lib.tmp.writeFile("retroarch/retroarch.cfg", "custom_viewport_width = \"0\"\nvideo_smooth = \"true\"\n");

    string coreOptionsInPlay, raConfigInPlay;
    lib.runner.whileRunning = [&] {
        coreOptionsInPlay = lib.tmp.readFile("retroarch/config/retroarch-core-options.cfg");
        raConfigInPlay = lib.tmp.readFile("retroarch/retroarch.cfg");
    };

    lib.service->launch(game, EmuMode::RetroArch, -1);

    CHECK(coreOptionsInPlay == "pcsx_rearmed_psxclock = \"50\"\n");
    CHECK(contains(raConfigInPlay, "custom_viewport_width  = \"960\""));
    CHECK(contains(raConfigInPlay, "video_smooth  = \"false\""));
}

// --- Apps ---

TEST_CASE("an App is its own startup script, run with no arguments and no memory cards") {
    Launching lib;
    PsGamePtr game = lib.foreignGame(true);

    lib.service->launch(game, EmuMode::Launcher, -1);

    const FakeProcessRunner::Call &call = lib.runner.only();
    CHECK(call.exe == "/media/Apps/SomeApp/run.sh");
    CHECK(call.args.empty());
}

TEST_CASE("which launcher runs is decided by the game first and the mode second") {
    Launching lib;
    lib.configure("Raconfig=false\n");

    PsGamePtr usb = lib.usbGame();
    lib.service->launch(usb, EmuMode::Pcsx, -1);
    lib.service->launch(usb, EmuMode::RetroArch, -1);
    PsGamePtr rom = lib.foreignGame(false);
    lib.service->launch(rom, EmuMode::Pcsx, -1);       // a playlist entry can only go to RetroArch
    PsGamePtr app = lib.foreignGame(true);
    lib.service->launch(app, EmuMode::Pcsx, -1);       // and an App only to its own script

    REQUIRE(lib.runner.calls.size() == 4);
    CHECK(lib.runner.calls[0].exe == lib.rcScript("launch.sh"));
    CHECK(lib.runner.calls[1].exe == lib.rcScript("launch_rb.sh"));
    CHECK(lib.runner.calls[2].exe == lib.rcScript("launch_rb.sh"));
    CHECK(lib.runner.calls[3].exe == "/media/Apps/SomeApp/run.sh");
}
