//
// EnvironmentSetup: the layouts a program can be started with.
//
#include "environment_setup.h"
#include "../main.h"
#include "environment.h"
#include "platform_config.h"
#ifdef AB_PLATFORM_WIN
#include "windows_host.h"
#endif

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
    Env::setThemesDir(root + sep + "Themes");
    Env::setCoversDbDir(root + sep + "Autobleem/bin/db");
#ifndef AB_ROOT_RELATIVE_LAYOUT
    Env::setKernelConfigDir("/etc/autobleem");
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
#else
    Env::setInternalDbFile("/media/System/Databases/internal.db");
#endif
    // the working path stays the current dir (Env::getWorkingPath() falls back to getcwd)
    Env::setThemesDir(Env::getWorkingPath() + sep + "Themes");
    Env::setCoversDbDir("../db");
    applyPlatformConfig();
}

//*******************************
// EnvironmentSetup::fromWindowsInstall
//*******************************
const char *const EnvironmentSetup::DefaultDataFolder = "AutoBleem";

namespace {
// a shallow-recursive copy of a shipped theme folder (theme.json and its images, one level of sub-dirs)
void copyTree(const string &from, const string &to) {
    DirEntry::createDirs(to);
    for (const DirEntry &e : DirEntry::diru(from)) {
        const string src = from + sep + e.name, dst = to + sep + e.name;
        if (e.isDir)
            copyTree(src, dst);
        else if (!DirEntry::exists(dst))
            DirEntry::copyFile(src, dst);
    }
}
} // namespace

string EnvironmentSetup::fromWindowsInstall(const HostFacts &facts) {
    string root;
    const char *from = "";
    if (!facts.registryDataRoot.empty()) {
        root = facts.registryDataRoot;
        from = "the registry";
    } else if (!facts.pointerFileDataRoot.empty()) {
        root = facts.pointerFileDataRoot;
        from = "dataroot.txt";
    } else if (!facts.documentsDir.empty()) {
        root = facts.documentsDir + sep + DefaultDataFolder;
        from = "the Documents folder";
    } else {
        PLOG_ERROR << "no data root: nothing in the registry, no dataroot.txt, and no Documents folder";
        return "";
    }
    while (root.size() > 1 && (root.back() == '/' || root.back() == '\\'))
        root.pop_back();
    PLOG_INFO << "Data root " << root << " (from " << from << "); program in " << facts.programDir;

    for (const char *d : {"Games", "System/Databases", "System/Logs", "System/Bios", "System/Updates", "Themes",
                          "RetroArch/roms", "Apps"}) {
        if (!DirEntry::createDirs(root + sep + d)) {
            PLOG_ERROR << "cannot make " << root + sep + d;
            return "";
        }
    }
    // the shipped themes, once: the launcher reads them from the data tree (a user drops their own next
    // to these), the program folder keeps the copy an update refreshes
    const string shippedThemes = facts.programDir + sep + "Themes";
    if (DirEntry::isDirectory(shippedThemes)) {
        for (const DirEntry &t : DirEntry::diru_DirsOnly(shippedThemes)) {
            if (!DirEntry::exists(root + sep + "Themes" + sep + t.name))
                copyTree(shippedThemes + sep + t.name, root + sep + "Themes" + sep + t.name);
        }
    }

    Env::setUsbRoot(root);
    Env::setGamesDir(root + sep + "Games");
    Env::setRegionalDbFile(root + sep + "System/Databases/regional.db");
    Env::setInternalDbFile(root + sep + "System/Databases/internal.db");
    Env::setWorkingPath(facts.programDir);
    Env::setStateDir(root + sep + "System");
    Env::setThemesDir(root + sep + "Themes");
    Env::setCoversDbDir(root + sep + "System/Databases");
    applyPlatformConfig();
    return root;
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
#ifdef AB_PLATFORM_WIN
    if (argc == 1)
        return !fromWindowsInstall(WindowsHost::facts()).empty();
#endif
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
