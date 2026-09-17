/*
 * File:   main.cpp
 * Author: screemer
 *
 * Created on 11 Dec 2018, 20:37
 */

#include <cstdlib>
#include <iostream>
#include "autobleem.h"
#include "core/main.h"
#include "core/services/system.h"
#include "core/services/environment.h"

using namespace std;

//*******************************
// setupEnvironment
//*******************************
// Tells ableem::Environment where everything is. This is the one place that knows the difference between the
// console layout and a root given on the command line (a debug host, or the Raspberry Pi port, where that root
// is the mount point of the exFAT data partition - see payload_rpi/).
//
// On a debug host a single arg (the path to the root of a usb drive) is optional instead of two args. In that
// mode, as much as possible, files from the usb drive are used instead of files in the debug build environment
// (the UI theme, the cover dbs, regional.db, the .prev file, the RetroArch playlists, the lang files, config.ini,
// ...), so you can debug the contents of a usb drive someone sent you.
static bool setupEnvironment(int argc, char *argv[]) {
    string usbRoot, gamesDir, regionalDb, internalDb;
    bool singleArg = false;
    if (argc == 1 + 1) {
        // the single arg is the path to the usb drive
        singleArg = true;
        usbRoot = argv[1];
        regionalDb = usbRoot + sep + "System/Databases/regional.db";
        internalDb = usbRoot + sep + "System/Databases/internal.db";
        gamesDir = usbRoot + sep + "Games";
    } else if (argc == 1 + 2) {
        // the two args are the path to the regional.db file and the path to the /Games dir on the usb drive
        regionalDb = argv[1];
#ifdef AB_ROOT_RELATIVE_LAYOUT
        internalDb = "internal.db";   // it's in the same dir as the autobleem-gui app you are debugging
#else
        internalDb = "/media/System/Databases/internal.db";
#endif
        gamesDir = argv[2];
        usbRoot = DirEntry::getDirNameFromPath(gamesDir);
    } else {
        cout << "USAGE: autobleem-gui /path/dbfilename.db /path/to/games" << endl;
        return false;
    }
    Env::setUsbRoot(usbRoot);
    Env::setGamesDir(gamesDir);
    Env::setRegionalDbFile(regionalDb);
    Env::setInternalDbFile(internalDb);

#ifdef AB_ROOT_RELATIVE_LAYOUT
    if (singleArg) {
        Env::setWorkingPath(usbRoot + sep + "Autobleem/bin/autobleem");
        Env::setThemesDir(usbRoot + sep + "themes");
        Env::setCoversDbDir(usbRoot + sep + "Autobleem/bin/db");
    } else {
        // the working path stays the current dir (Env::getWorkingPath() falls back to getcwd)
        Env::setThemesDir(Env::getWorkingPath() + sep + "themes");
        Env::setCoversDbDir("../db");
    }
    Env::setSonyDataPath(Env::getWorkingPath() + sep + "sony");
#else
    (void) singleArg;
    Env::setSonyDataPath("/usr/sony/share/data");
    Env::setThemesDir("/media/themes");
    Env::setCoversDbDir("../db");
#endif
    return true;
}

//*******************************
// runAutobleem
//*******************************
// the whole program. main() below only wraps it so that a stray exception is logged instead of a silent abort().
static int runAutobleem(int argc, char *argv[]) {
    // stdout/stderr go to /media/System/Logs/AB_*.txt (see run.sh). without this they are block buffered and the
    // last lines before a crash never reach the file, which is exactly when they are needed.
    cout.setf(ios::unitbuf);
    cerr.setf(ios::unitbuf);

    // SDL_Init/InitSubSystem/TTF_Init/Mix_Init all happen inside ableem::Platform, constructed the first time
    // the Gui singleton is created (inside App's constructor, below). Registering SDL_Quit here (before that
    // happens) makes it run after the Gui singleton (window, renderer, textures) is destroyed during static
    // destruction, which happens after main() returns.
    atexit(ableem::Platform::shutdownSDL);
    Env::autobleemKernel = DirEntry::exists("/autobleem");

    if (!setupEnvironment(argc, argv)) {
        return EXIT_FAILURE;
    }

    AutoBleem app;
    return app.run();
}

//*******************************
// main
//*******************************
int main(int argc, char *argv[]) {
    try {
        return runAutobleem(argc, argv);
    } catch (const std::exception &e) {
        cerr << "FATAL: unhandled exception: " << e.what() << endl;
    } catch (...) {
        cerr << "FATAL: unhandled exception of unknown type" << endl;
    }
    return EXIT_FAILURE;
}
