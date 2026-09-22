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
#include <ableem/engine/md5.h>

#include <fstream>

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

// what a shipped theme folder holds - every file's relative name, size and MD5 - as one line, so the data
// tree's copy can tell whether the program's copy changed under it
void digestTree(const string &dir, const string &prefix, ableem::Md5 &md5) {
    for (const DirEntry &e : DirEntry::diru(dir)) {
        if (e.isDir) {
            digestTree(dir + sep + e.name, prefix + e.name + "/", md5);
            continue;
        }
        const string path = dir + sep + e.name;
        const string line =
            prefix + e.name + ":" + to_string(DirEntry::fileSize(path)) + ":" + ableem::Md5::ofFile(path) + "\n";
        md5.update(reinterpret_cast<const unsigned char *>(line.data()), line.size());
    }
}

string themeDigest(const string &dir) {
    ableem::Md5 md5;
    digestTree(dir, "", md5);
    return md5.hexDigest();
}

const char *const ShippedStamp = ".shipped"; // in the data tree's copy: the digest it was copied from

string readStamp(const string &themeDir) {
    ifstream is(themeDir + sep + ShippedStamp);
    string line;
    getline(is, line);
    return line;
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
    // The shipped themes: the launcher reads them from the data tree (a user drops their own next to
    // these), the program folder keeps the copy an update refreshes - and since 2026-09-22 the data
    // tree's copy follows it: a shipped theme is copied again whenever the program's copy differs from
    // what the data tree's copy was made from (its .shipped stamp - a copy without one, from before, is
    // refreshed once). Until then an installed launcher kept reading the first install's themes and a
    // theme change in a release never reached it (ab2's resume-slot glow). A user's edit of a shipped
    // theme lasts until the next release changes that theme; their own themes are their own.
    const string shippedThemes = facts.programDir + sep + "Themes";
    if (DirEntry::isDirectory(shippedThemes)) {
        for (const DirEntry &t : DirEntry::diru_DirsOnly(shippedThemes)) {
            const string from = shippedThemes + sep + t.name, to = root + sep + "Themes" + sep + t.name;
            const string digest = themeDigest(from);
            if (DirEntry::exists(to)) {
                if (readStamp(to) == digest)
                    continue;
                PLOG_INFO << "Theme " << t.name << " changed with the program - refreshing the data tree's copy";
                DirEntry::removeDirAndContents(to);
            }
            copyTree(from, to);
            ofstream os(to + sep + ShippedStamp);
            os << digest << endl;
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
