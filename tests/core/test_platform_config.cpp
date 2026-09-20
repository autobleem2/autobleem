//
// PlatformConfig: resources/platform/<platform>.ini decides where RetroArch is, per target.
//
#include "doctest/doctest.h"

#include "../support/env_fixture.h"
#include "../support/temp_dir.h"
#include "core/services/platform_config.h"
#include "core/services/environment.h"
#include "core/main.h"

TEST_CASE("PlatformConfig::load takes the keys from the ini and keeps defaults for the rest") {
    TempDir tmp("platform");
    tmp.makeSubDir("platform");
    tmp.writeFile("platform/rpi.ini", "# the Pi\n"
                                      "retroarch_dir=RetroArch\n"
                                      "retroarch_core = cores/pcsx_rearmed_libretro.so\n"
                                      "retroarch_binary=/usr/local/bin/retroarch; /usr/bin/retroarch\n"
                                      "retroarch_roms_dir=RetroArch/roms\n"
                                      "retroarch_bios_dir=RetroArch/system\n");

    PlatformConfig cfg = PlatformConfig::load(PlatformConfig::pathFor(tmp.path(), "rpi"));
    CHECK(cfg.retroarchDir == "RetroArch");
    CHECK(cfg.retroarchCore == "cores/pcsx_rearmed_libretro.so");
    CHECK(cfg.retroarchRomsDir == "RetroArch/roms");
    CHECK(cfg.retroarchBiosDir == "RetroArch/system");
    REQUIRE(cfg.retroarchBinaries.size() == 2);
    CHECK(cfg.retroarchBinaries[0] == "/usr/local/bin/retroarch");
    CHECK(cfg.retroarchBinaries[1] == "/usr/bin/retroarch");

    // only one key given: the others stay the console's
    tmp.writeFile("platform/odd.ini", "retroarch_dir=elsewhere\n");
    PlatformConfig odd = PlatformConfig::load(PlatformConfig::pathFor(tmp.path(), "odd"));
    CHECK(odd.retroarchDir == "elsewhere");
    CHECK(odd.retroarchCore.empty()); // the engine's default, with the platform's core extension
    REQUIRE(odd.retroarchBinaries.size() == 1);
    CHECK(odd.retroarchBinaries[0] == "retroarch");
    CHECK(odd.retroarchRomsDir == "RetroArch/roms");
    CHECK(odd.retroarchBiosDir == "RetroArch/bios");
}

TEST_CASE("PlatformConfig::load without a file is the console's layout") {
    PlatformConfig cfg = PlatformConfig::load("/nowhere/platform/psc.ini");
    CHECK(cfg.retroarchDir == "RetroArch/bin");
    CHECK(cfg.retroarchCore.empty());
    REQUIRE(cfg.retroarchBinaries.size() == 1);
    CHECK(cfg.retroarchBinaries[0] == "retroarch");
}

TEST_CASE("PlatformConfig::splitList") {
    CHECK(PlatformConfig::splitList("a;b; c ;;").size() == 3);
    CHECK(PlatformConfig::splitList("").empty());
    CHECK(PlatformConfig::splitList(" one ").at(0) == "one");
}

TEST_CASE("PlatformConfig::apply resolves relative paths against the USB root and the RetroArch dir") {
    EnvFixture env;
    env.setUsbRoot("/media");

    PlatformConfig console; // the defaults
    console.apply();
    CHECK(Environment::getPathToRetroarchDir() == "/media/RetroArch/bin");
    CHECK(Environment::getPathToRetroarchCoreFile() == "/media/RetroArch/bin/cores/pcsx_rearmed_libretro.so");
    REQUIRE(Environment::retroArchBinaries().size() == 1);
    CHECK(Environment::retroArchBinaries()[0] == "/media/RetroArch/bin/retroarch");
    CHECK(Environment::getPathToRetroarchRomsDir() == "/media/RetroArch/roms");
    CHECK(Environment::getPathToRetroarchBiosDir() == "/media/RetroArch/bios");

    PlatformConfig pi;
    pi.retroarchDir = "RetroArch";
    pi.retroarchCore = "cores/pcsx_rearmed_libretro.so";
    pi.retroarchBinaries = {"/usr/local/bin/retroarch", "/usr/bin/retroarch"};
    pi.retroarchRomsDir = "RetroArch/roms";
    pi.apply();
    CHECK(Environment::getPathToRetroarchRomsDir() == "/media/RetroArch/roms");
    CHECK(Environment::getPathToRetroarchDir() == "/media/RetroArch");
    CHECK(Environment::getPathToRetroarchPlaylistsDir() == "/media/RetroArch/playlists");
    CHECK(Environment::getPathToRetroarchCoreFile() == "/media/RetroArch/cores/pcsx_rearmed_libretro.so");
    CHECK(Environment::retroArchBinaries()[0] == "/usr/local/bin/retroarch"); // absolute: stands as is
}

TEST_CASE("Env::retroArchInstalled is true when any candidate binary exists") {
    TempDir tmp("rabin");
    EnvFixture env;
    env.setUsbRoot(tmp.path());

    PlatformConfig cfg;
    cfg.retroarchBinaries = {"retroarch", "bin/retroarch"}; // both relative to the RetroArch dir
    cfg.apply();
    CHECK_FALSE(Environment::retroArchInstalled());

    // the engine's default is <root>/RetroArch/bin - spelled exactly, the CI runs on a case-sensitive
    // filesystem (a "retroarch/bin" here passed on Windows and failed on Linux)
    tmp.makeSubDir("RetroArch/bin/bin");
    tmp.writeFile("RetroArch/bin/bin/retroarch", "#!/bin/sh\n");
    CHECK(Environment::retroArchInstalled());
}

TEST_CASE("EnvFixture restores the RetroArch core file and binaries") {
    EnvFixture outer;
    outer.setUsbRoot("/before");
    {
        EnvFixture inner;
        PlatformConfig pi;
        pi.retroarchDir = "RetroArch";
        pi.retroarchCore = "cores/x.so";
        pi.retroarchBinaries = {"/usr/bin/retroarch"};
        pi.apply();
        CHECK(Environment::getPathToRetroarchCoreFile() == "/before/RetroArch/cores/x.so");
    }
    CHECK(Environment::getPathToRetroarchDir() == "/before/RetroArch/bin");
    CHECK(Environment::getPathToRetroarchCoreFile() == "/before/RetroArch/bin/cores/pcsx_rearmed_libretro.so");
}

TEST_CASE("PlatformConfig: the update, launch and core-extension keys") {
    TempDir tmp("platform2");
    tmp.makeSubDir("platform");

    SUBCASE("defaults: script launch, .so cores, no catalog, no pcsx dir") {
        PlatformConfig cfg = PlatformConfig::load("/nowhere/platform/psc.ini");
        CHECK(cfg.launchMode == "script");
        CHECK(cfg.coreExtension == ".so");
        CHECK(cfg.retroarchCatalog.empty());
        CHECK(cfg.pcsxDir.empty());
    }

    SUBCASE("the keys are read; a core extension gets its dot; an unknown launch mode is script") {
        tmp.writeFile("platform/win.ini", "launch_mode=direct\n"
                                          "core_extension=dll\n"
                                          "retroarch_catalog=/pc/retroarch/latest.json\n"
                                          "pcsx_dir=emu\n");
        PlatformConfig cfg = PlatformConfig::load(PlatformConfig::pathFor(tmp.path(), "win"));
        CHECK(cfg.launchMode == "direct");
        CHECK(cfg.coreExtension == ".dll");
        CHECK(cfg.retroarchCatalog == "/pc/retroarch/latest.json");
        CHECK(cfg.pcsxDir == "emu");

        tmp.writeFile("platform/odd.ini", "launch_mode=sideways\n");
        CHECK(PlatformConfig::load(PlatformConfig::pathFor(tmp.path(), "odd")).launchMode == "script");
    }

    SUBCASE("apply hands them to Env: the extension names the default core, the catalog loses its slash") {
        EnvFixture env;
        env.setUsbRoot("/data");
        env.setWorkingPath("/app");
        PlatformConfig cfg;
        cfg.launchMode = "direct";
        cfg.coreExtension = ".dll";
        cfg.retroarchCatalog = "/pc/retroarch/latest.json";
        cfg.pcsxDir = "emu";
        cfg.pcsxNxtDir = "emunxt";
        cfg.apply();
        CHECK(Environment::directLaunch());
        CHECK(Environment::getRetroarchCoreExtension() == ".dll");
        CHECK(Environment::getPathToRetroarchCoreFile() == "/data/RetroArch/bin/cores/pcsx_rearmed_libretro.dll");
        CHECK(Environment::retroArchCatalog() == "pc/retroarch/latest.json");
        CHECK(Environment::pcsxDir() == "/app/emu");
        CHECK(Environment::pcsxNxtDir() == "/app/emunxt");

        PlatformConfig plain;
        plain.apply();
        CHECK_FALSE(Environment::directLaunch());
        CHECK(Environment::getPathToRetroarchCoreFile() == "/data/RetroArch/bin/cores/pcsx_rearmed_libretro.so");
        CHECK(Environment::pcsxDir().empty());
        CHECK(Environment::pcsxNxtDir().empty());
    }
}

TEST_CASE("every shipped platform ini loads, and each target's file is there") {
    const std::string dir = std::string(AB_RESOURCES_DIR) + "/platform";
    for (const char *name : {"psc", "rpi", "pcusb", "pc"}) {
        INFO(name);
        const std::string ini = PlatformConfig::pathFor(AB_RESOURCES_DIR, name);
        REQUIRE(DirEntry::exists(ini));
        PlatformConfig cfg = PlatformConfig::load(ini);
        CHECK(cfg.launchMode == "script");
        CHECK(cfg.coreExtension == ".so");
        CHECK_FALSE(cfg.retroarchDir.empty());
        CHECK(DirEntry::exists(dir + "/" + name + ".cores.cfg"));
    }
    // the console never goes online; the appliances update RetroArch from their own catalog
    CHECK(PlatformConfig::load(PlatformConfig::pathFor(AB_RESOURCES_DIR, "psc")).downloadCommand.empty());
    CHECK(PlatformConfig::load(PlatformConfig::pathFor(AB_RESOURCES_DIR, "rpi")).retroarchCatalog ==
          "rpi/retroarch/latest.json");
    CHECK(PlatformConfig::load(PlatformConfig::pathFor(AB_RESOURCES_DIR, "pcusb")).retroarchCatalog ==
          "pc/retroarch/latest.json");
}
