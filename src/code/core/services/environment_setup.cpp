//
// EnvironmentSetup: the layouts a program can be started with.
//
#include "environment_setup.h"
#include "../main.h"
#include "environment.h"
#include "platform_config.h"

#include <ableem/engine/log.h>

using namespace std;

//*******************************
// EnvironmentSetup::fromRoot
//*******************************
void EnvironmentSetup::fromRoot(const string &root) {
    Env::setUsbRoot(root);
    Env::setGamesDir(root + sep + "Games");
    Env::setRegionalDbFile(root + sep + "System/Databases/regional.db");
    Env::setInternalDbFile(root + sep + "System/Databases/internal.db");
    Env::setWorkingPath(root + sep + "Autobleem/bin/autobleem");
    Env::setThemesDir(root + sep + "themes");
    Env::setCoversDbDir(root + sep + "Autobleem/bin/db");
#ifdef AB_ROOT_RELATIVE_LAYOUT
    Env::setSonyDataPath(Env::getWorkingPath() + sep + "sony");
#else
    Env::setSonyDataPath("/usr/sony/share/data");
#endif
    applyPlatformConfig();
}

//*******************************
// EnvironmentSetup::fromDbAndGames
//*******************************
void EnvironmentSetup::fromDbAndGames(const string &regionalDb, const string &gamesDir) {
    Env::setUsbRoot(DirEntry::getDirNameFromPath(gamesDir));
    Env::setGamesDir(gamesDir);
    Env::setRegionalDbFile(regionalDb);
#ifdef AB_ROOT_RELATIVE_LAYOUT
    Env::setInternalDbFile("internal.db"); // it's in the same dir as the autobleem-gui app you are debugging
    Env::setSonyDataPath(Env::getWorkingPath() + sep + "sony");
#else
    Env::setInternalDbFile("/media/System/Databases/internal.db");
    Env::setSonyDataPath("/usr/sony/share/data");
#endif
    // the working path stays the current dir (Env::getWorkingPath() falls back to getcwd)
    Env::setThemesDir(Env::getWorkingPath() + sep + "themes");
    Env::setCoversDbDir("../db");
    applyPlatformConfig();
}

//*******************************
// EnvironmentSetup::fromArguments
//*******************************
bool EnvironmentSetup::fromArguments(int argc, char *argv[]) {
    if (argc == 1 + 1) {
        fromRoot(argv[1]);
        return true;
    }
    if (argc == 1 + 2) {
        fromDbAndGames(argv[1], argv[2]);
        return true;
    }
    PLOG_INFO << "USAGE: autobleem-gui /path/to/usb-root [--sysinfo]  |  autobleem-gui /path/dbfilename.db "
                 "/path/to/games";
    return false;
}

//*******************************
// EnvironmentSetup::forTool
//*******************************
bool EnvironmentSetup::forTool(int argc, char *argv[], const string &toolName) {
    string root;
    if (argc == 1 + 1) {
        root = argv[1];
    } else if (argc == 1) {
#ifdef AB_ROOT_RELATIVE_LAYOUT
        PLOG_INFO << "USAGE: " << toolName << " /path/to/usb-root";
        return false;
#else
        root = "/media";
#endif
    } else {
        PLOG_INFO << "USAGE: " << toolName << " [/path/to/usb-root]";
        return false;
    }
    Env::setAppDir(Env::getAppDir()); // the current directory, pinned before anything can chdir
    fromRoot(root);
    return true;
}

//*******************************
// EnvironmentSetup::applyPlatformConfig
//*******************************
// where RetroArch is on this platform: the console's RetroBoot tree, the Pi's RetroArch/ on the data
// partition, ... - data in resources/platform/<platform>.ini, not #ifdefs
void EnvironmentSetup::applyPlatformConfig() {
    PlatformConfig::load(PlatformConfig::pathFor(Env::getWorkingPath(), Env::platformName())).apply();
}
