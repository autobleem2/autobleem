//
// PlatformConfig: resources/platform/<platform>.ini decides where RetroArch is, per target.
//
#include "doctest/doctest.h"

#include "../support/env_fixture.h"
#include "../support/temp_dir.h"
#include "core/services/platform_config.h"
#include "core/services/environment.h"

TEST_CASE("PlatformConfig::load takes the keys from the ini and keeps defaults for the rest") {
    TempDir tmp("platform");
    tmp.makeSubDir("platform");
    tmp.writeFile("platform/rpi.ini", "# the Pi\n"
                                      "retroarch_dir=RetroArch\n"
                                      "retroarch_core = cores/pcsx_rearmed_libretro.so\n"
                                      "retroarch_binary=/usr/local/bin/retroarch; /usr/bin/retroarch\n");

    PlatformConfig cfg = PlatformConfig::load(PlatformConfig::pathFor(tmp.path(), "rpi"));
    CHECK(cfg.retroarchDir == "RetroArch");
    CHECK(cfg.retroarchCore == "cores/pcsx_rearmed_libretro.so");
    REQUIRE(cfg.retroarchBinaries.size() == 2);
    CHECK(cfg.retroarchBinaries[0] == "/usr/local/bin/retroarch");
    CHECK(cfg.retroarchBinaries[1] == "/usr/bin/retroarch");

    // only one key given: the others stay the console's
    tmp.writeFile("platform/odd.ini", "retroarch_dir=elsewhere\n");
    PlatformConfig odd = PlatformConfig::load(PlatformConfig::pathFor(tmp.path(), "odd"));
    CHECK(odd.retroarchDir == "elsewhere");
    CHECK(odd.retroarchCore == "cores/km_pcsx_rearmed_neon_libretro.so");
    REQUIRE(odd.retroarchBinaries.size() == 1);
    CHECK(odd.retroarchBinaries[0] == "retroarch");
}

TEST_CASE("PlatformConfig::load without a file is the console's layout") {
    PlatformConfig cfg = PlatformConfig::load("/nowhere/platform/psc.ini");
    CHECK(cfg.retroarchDir == "retroarch");
    CHECK(cfg.retroarchCore == "cores/km_pcsx_rearmed_neon_libretro.so");
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
    CHECK(Environment::getPathToRetroarchDir() == "/media/retroarch");
    CHECK(Environment::getPathToRetroarchCoreFile() == "/media/retroarch/cores/km_pcsx_rearmed_neon_libretro.so");
    REQUIRE(Environment::retroArchBinaries().size() == 1);
    CHECK(Environment::retroArchBinaries()[0] == "/media/retroarch/retroarch");

    PlatformConfig pi;
    pi.retroarchDir = "RetroArch";
    pi.retroarchCore = "cores/pcsx_rearmed_libretro.so";
    pi.retroarchBinaries = {"/usr/local/bin/retroarch", "/usr/bin/retroarch"};
    pi.apply();
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
    cfg.retroarchBinaries = {"retroarch", "bin/retroarch"}; // both relative to <root>/retroarch
    cfg.apply();
    CHECK_FALSE(Environment::retroArchInstalled());

    tmp.makeSubDir("retroarch/bin");
    tmp.writeFile("retroarch/bin/retroarch", "#!/bin/sh\n");
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
    CHECK(Environment::getPathToRetroarchDir() == "/before/retroarch");
    CHECK(Environment::getPathToRetroarchCoreFile() == "/before/retroarch/cores/km_pcsx_rearmed_neon_libretro.so");
}
