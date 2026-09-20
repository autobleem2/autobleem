//
// EnvironmentSetup: the layouts a program can be started with, and what each puts into Environment.
//
#include "doctest/doctest.h"

#include "../support/env_fixture.h"
#include "../support/temp_dir.h"

#include "core/services/environment.h"
#include "core/services/environment_setup.h"

#include <string>
#include <vector>

using std::string;
using std::vector;

namespace {

// argv the way main() gets it, from a list
struct Args {
    explicit Args(const vector<string> &list) : strings(list) {
        for (string &s : strings)
            pointers.push_back(&s[0]);
    }
    int argc() const { return static_cast<int>(pointers.size()); }
    char **argv() { return pointers.data(); }
    vector<string> strings;
    vector<char *> pointers;
};

} // namespace

TEST_CASE("fromRoot derives every path from the one root") {
    EnvFixture env;
    EnvironmentSetup::fromRoot("/usb");
    CHECK(Env::getPathToUSBRoot() == "/usb");
    CHECK(Env::getPathToGamesDir() == "/usb/Games");
    CHECK(Env::getPathToRegionalDBFile() == "/usb/System/Databases/regional.db");
    CHECK(Env::getPathToInternalDBFile() == "/usb/System/Databases/internal.db");
    CHECK(Env::getWorkingPath() == "/usb/Autobleem/bin/autobleem");
    CHECK(Env::getPathToLangDir() == "/usb/Autobleem/bin/autobleem/lang");
    CHECK(Env::getPathToThemesDir() == "/usb/Themes");
    CHECK(Env::getPathToCoversDBDir() == "/usb/Autobleem/bin/db");
    CHECK(Env::getPathToRCDir() == "/usb/Autobleem/rc");
    CHECK(Env::getPathToGameControllerDb() == "/usb/Autobleem/bin/autobleem/gamecontrollerdb.txt");
#ifdef AB_ROOT_RELATIVE_LAYOUT
    CHECK(Env::getSonyPath() == "/usb/Autobleem/bin/autobleem/sony");
    CHECK(Env::getPathToKernelConfigDir().empty());
    CHECK(Env::padMappingFiles() == std::vector<string>{"/usb/Autobleem/bin/autobleem/gamecontrollerdb.txt"});
#else
    CHECK(Env::getSonyPath() == "/usr/sony/share/data");
    CHECK(Env::getPathToKernelConfigDir() == "/etc/autobleem");
    CHECK(Env::padMappingFiles() == std::vector<string>{"/etc/autobleem/gamecontrollerdb.txt",
                                                        "/usb/Autobleem/bin/autobleem/gamecontrollerdb.txt"});
#endif
}

TEST_CASE("fromDbAndGames is the debug layout: the two paths given, the rest from the current dir") {
    EnvFixture env;
    EnvironmentSetup::fromDbAndGames("/tmp/regional.db", "/stick/Games");
    CHECK(Env::getPathToUSBRoot() == "/stick");
    CHECK(Env::getPathToGamesDir() == "/stick/Games");
    CHECK(Env::getPathToRegionalDBFile() == "/tmp/regional.db");
    CHECK(Env::getPathToThemesDir() == Env::getWorkingPath() + "/Themes");
    CHECK(Env::getPathToCoversDBDir() == "../db");
}

TEST_CASE("fromArguments accepts one root or db + games and refuses anything else") {
    EnvFixture env;
    Args none({"autobleem-gui"});
    CHECK_FALSE(EnvironmentSetup::fromArguments(none.argc(), none.argv()));

    Args one({"autobleem-gui", "/usb"});
    CHECK(EnvironmentSetup::fromArguments(one.argc(), one.argv()));
    CHECK(Env::getPathToGamesDir() == "/usb/Games");

    Args two({"autobleem-gui", "/x/regional.db", "/y/Games"});
    CHECK(EnvironmentSetup::fromArguments(two.argc(), two.argv()));
    CHECK(Env::getPathToRegionalDBFile() == "/x/regional.db");
    CHECK(Env::getPathToUSBRoot() == "/y");

    Args three({"autobleem-gui", "a", "b", "c"});
    CHECK_FALSE(EnvironmentSetup::fromArguments(three.argc(), three.argv()));
}

TEST_CASE("forTool takes the root as its one argument and keeps the tool's own folder as the app dir") {
    EnvFixture env;
    const string startedFrom = Env::getAppDir(); // the current directory, nothing has set it
    Args one({"pscbios", "/usb"});
    CHECK(EnvironmentSetup::forTool(one.argc(), one.argv(), "pscbios"));
    CHECK(Env::getPathToUSBRoot() == "/usb");
    CHECK(Env::getWorkingPath() == "/usb/Autobleem/bin/autobleem"); // the main GUI's resources
    CHECK(Env::getAppDir() == startedFrom);
    CHECK(Env::getPathToAppLangDir() == startedFrom + "/lang");

    Args two({"pscbios", "/usb", "extra"});
    CHECK_FALSE(EnvironmentSetup::forTool(two.argc(), two.argv(), "pscbios"));

    Args none({"pscbios"});
#ifdef AB_ROOT_RELATIVE_LAYOUT
    CHECK_FALSE(EnvironmentSetup::forTool(none.argc(), none.argv(), "pscbios")); // a debug host needs the root
#else
    CHECK(EnvironmentSetup::forTool(none.argc(), none.argv(), "pscbios"));
    CHECK(Env::getPathToUSBRoot() == "/media"); // the console's
#endif
}

TEST_CASE("the app dir and the working path are independent") {
    EnvFixture env;
    env.setWorkingPath("/usb/Autobleem/bin/autobleem");
    env.setAppDir("/usb/Apps/pscbios");
    CHECK(Env::getPathToLangDir() == "/usb/Autobleem/bin/autobleem/lang");
    CHECK(Env::getPathToAppLangDir() == "/usb/Apps/pscbios/lang");
}
