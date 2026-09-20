//
// The harness's own load-bearing piece: EnvFixture is what keeps the suite order-independent, so it gets
// its own test rather than only being trusted.
//
#include "doctest/doctest.h"

#include "../support/env_fixture.h"
#include "../support/temp_dir.h"

TEST_CASE("EnvFixture puts every root back when it goes out of scope") {
    EnvFixture outer;
    outer.setUsbRoot("/before/usb");
    outer.setGamesDir("/before/games");
    outer.setThemesDir("/before/themes");

    {
        EnvFixture inner;
        inner.setUsbRoot("/during/usb");
        inner.setGamesDir("/during/games");
        inner.setThemesDir("/during/themes");

        CHECK(Environment::getPathToUSBRoot() == "/during/usb");
        CHECK(Environment::getPathToGamesDir() == "/during/games");
    }

    CHECK(Environment::getPathToUSBRoot() == "/before/usb");
    CHECK(Environment::getPathToGamesDir() == "/before/games");
    CHECK(Environment::getPathToThemesDir() == "/before/themes");
}

TEST_CASE("Environment derives the sub-paths from the roots a fixture sets") {
    EnvFixture env;
    env.setUsbRoot("/media");
    env.setGamesDir("/media/Games");

    CHECK(Environment::getPathToAutobleemDir() == "/media/Autobleem");
    CHECK(Environment::getPathToRCDir() == "/media/Autobleem/rc");
    CHECK(Environment::getPathToSystemDir() == "/media/System");
    CHECK(Environment::getPathToMemCardsDir() == "/media/Games/!MemCards");
    CHECK(Environment::getPathToSaveStatesDir() == "/media/Games/!SaveStates");
}

TEST_CASE("Environment's RetroArch dir is derived from the USB root unless set explicitly") {
    EnvFixture env;
    env.setUsbRoot("/media");
    CHECK(Environment::getPathToRetroarchDir() == "/media/RetroArch/bin");
    CHECK(Environment::getPathToRetroarchPlaylistsDir() == "/media/RetroArch/bin/playlists");
    CHECK(Environment::getPathToRetroarchRomsDir() == "/media/RetroArch/roms");
    CHECK(Environment::getPathToRetroarchBiosDir() == "/media/RetroArch/bios");

    // the Raspberry Pi keeps RetroArch's standard tree under RetroArch/ on its data partition
    env.setRetroarchDir("/media/autobleem/RetroArch");
    CHECK(Environment::getPathToRetroarchDir() == "/media/autobleem/RetroArch");
    CHECK(Environment::getPathToRetroarchPlaylistsDir() == "/media/autobleem/RetroArch/playlists");

    // and "" goes back to deriving it
    env.setRetroarchDir("");
    CHECK(Environment::getPathToRetroarchDir() == "/media/RetroArch/bin");
}

TEST_CASE("EnvFixture restores an explicit RetroArch dir and a derived one alike") {
    {
        EnvFixture outer;
        outer.setUsbRoot("/before");
        {
            EnvFixture inner;
            inner.setRetroarchDir("/elsewhere/RetroArch");
            CHECK(Environment::getPathToRetroarchDir() == "/elsewhere/RetroArch");
        }
        CHECK(Environment::getPathToRetroarchDir() == "/before/RetroArch/bin");

        outer.setRetroarchDir("/explicit/RetroArch");
        {
            EnvFixture inner;
            inner.setUsbRoot("/during");
            CHECK(Environment::getPathToRetroarchDir() == "/explicit/RetroArch");
        }
        CHECK(Environment::getPathToRetroarchDir() == "/explicit/RetroArch");
    }
}

TEST_CASE("TempDir creates a tree and removes it again") {
    std::string path;
    {
        TempDir tmp("selftest");
        path = tmp.path();
        CHECK(ableem::DirEntry::exists(path));

        tmp.makeSubDir("themes/default");
        CHECK(ableem::DirEntry::exists(tmp.at("themes/default")));

        tmp.writeFile("themes/default/theme.ini", "Background=bg.png\n");
        CHECK(tmp.readFile("themes/default/theme.ini") == "Background=bg.png\n");
    }
    CHECK_FALSE(ableem::DirEntry::exists(path));
}

TEST_CASE("Two TempDirs with the same label do not collide") {
    TempDir a("same");
    TempDir b("same");
    CHECK(a.path() != b.path());
}
