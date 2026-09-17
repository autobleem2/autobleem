//
// Config: the defaults it fills in, the obsolete keys it drops, and the save/reload round trip.
//
#include "doctest/doctest.h"

#include "../support/env_fixture.h"
#include "../support/temp_dir.h"

#include "core/services/config.h"

#include <ableem/engine/ini_file.h>

using std::string;

namespace {

// reads the config.ini Config just wrote, so a test can assert on the file rather than on the live object
ableem::IniFile reloadFromDisk(const TempDir &tmp) {
    ableem::IniFile onDisk;
    onDisk.load(tmp.at("config.ini"));
    return onDisk;
}

} // namespace

TEST_CASE("Config fills in a default for every key the UI reads") {
    TempDir tmp("config_defaults");
    EnvFixture env;
    env.setWorkingPath(tmp.path());

    Config config;

    // the UI reads these without checking whether they are there, so all of them must have a value
    CHECK(config.inifile.values["language"] == "English");
    CHECK(config.inifile.values["aspect"] == "false");
    CHECK(config.inifile.values["jewel"] == "default");
    CHECK(config.inifile.values["music"] == "--");
    CHECK(config.inifile.values["showingtimeout"] == "2");
    CHECK(config.inifile.values["raconfig"] == "true");
}

TEST_CASE("Config keeps what the file already says") {
    TempDir tmp("config_existing");
    EnvFixture env;
    env.setWorkingPath(tmp.path());

    tmp.writeFile("config.ini",
                  "[General]\n"
                  "Theme=aergb\n"
                  "Language=Polish\n");

    Config config;

    // keys are lower-cased on load, values are not
    CHECK(config.inifile.values["theme"] == "aergb");
    CHECK(config.inifile.values["language"] == "Polish");

    // and the defaults still fill in around them
    CHECK(config.inifile.values["aspect"] == "false");
}

TEST_CASE("Config drops the keys older AutoBleem versions wrote") {
    TempDir tmp("config_obsolete");
    EnvFixture env;
    env.setWorkingPath(tmp.path());

    tmp.writeFile("config.ini",
                  "Theme=aergb\n"
                  "Stheme=old\n"
                  "Autoregion=1\n"
                  "Quick=1\n"
                  "Quickmenu=1\n"
                  "Delay=3\n"
                  "Adv=1\n"
                  "UI=classic\n");   // the classic UI is gone - see Session::MenuOption / GuiLauncher

    Config config;

    for (const char *gone : {"stheme", "autoregion", "quick", "quickmenu", "delay", "adv", "ui"}) {
        CHECK(config.inifile.values.count(gone) == 0);
    }
    CHECK(config.inifile.values["theme"] == "aergb");

    // they are gone from the file on disk too, not just from the live map
    ableem::IniFile onDisk = reloadFromDisk(tmp);
    for (const char *gone : {"stheme", "autoregion", "quick", "quickmenu", "delay", "adv", "ui"}) {
        CHECK(onDisk.values.count(gone) == 0);
    }
}

TEST_CASE("Config forces pcsx to bleemsync whatever the file says") {
    TempDir tmp("config_pcsx");
    EnvFixture env;
    env.setWorkingPath(tmp.path());

    tmp.writeFile("config.ini", "Pcsx=something-else\n");

    Config config;
    CHECK(config.inifile.values["pcsx"] == "bleemsync");

    config.save();
    CHECK(reloadFromDisk(tmp).values["pcsx"] == "bleemsync");
}

TEST_CASE("Config::save round trips through the file") {
    TempDir tmp("config_roundtrip");
    EnvFixture env;
    env.setWorkingPath(tmp.path());

    {
        Config config;
        config.inifile.values["theme"] = "evolution";
        config.inifile.values["showingtimeout"] = "7";
        config.save();
    }

    Config reloaded;
    CHECK(reloaded.inifile.values["theme"] == "evolution");
    CHECK(reloaded.inifile.values["showingtimeout"] == "7");   // not overwritten by the default
}

TEST_CASE("Config writes config.ini into the working path, not the current directory") {
    TempDir tmp("config_location");
    EnvFixture env;
    env.setWorkingPath(tmp.path());

    Config config;

    CHECK(ableem::DirEntry::exists(tmp.at("config.ini")));
}
