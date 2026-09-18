/*
 * File:   main.cpp
 * Author: screemer
 *
 * Created on 11 Dec 2018, 20:37
 */

#include <cstdlib>
#include <iostream>
#include <vector>
#include "autobleem.h"
#include "core/main.h"
#include "core/services/system.h"
#include "core/services/environment.h"
#include "core/services/environment_setup.h"
#include "core/services/system_info.h"
#include "core/version.h"
#include <ableem/engine/log.h>

using namespace std;

//*******************************
// runAutobleem
//*******************************
// the whole program. main() below only wraps it so that a stray exception is logged instead of a silent abort().
static int runAutobleem(int argc, char *argv[]) {
    // stdout/stderr go to /media/System/Logs/AB_*.txt (see run.sh). without this they are block buffered and the
    // last lines before a crash never reach the file, which is exactly when they are needed.
    cout.setf(ios::unitbuf);
    cerr.setf(ios::unitbuf);

    // the console appender first, so a bad command line is reported; the file appender once the environment
    // knows where the logs directory is
    ableem::Log::initConsoleOnly();

    // SDL_Init/InitSubSystem/TTF_Init/Mix_Init all happen inside ableem::Platform, constructed the first time
    // the Gui singleton is created (inside App's constructor, below). Registering SDL_Quit here (before that
    // happens) makes it run after the Gui singleton (window, renderer, textures) is destroyed during static
    // destruction, which happens after main() returns.
    atexit(ableem::Platform::shutdownSDL);
    Env::autobleemKernel = DirEntry::exists("/autobleem");

    // "autobleem-gui <root> --sysinfo": what the Hardware Information screen shows, on stdout, no window -
    // for a bug report, or a machine reached over ssh. Taken out of the arguments before the layout is read.
    vector<char *> args;
    bool sysInfoOnly = false;
    for (int i = 0; i < argc; i++) {
        if (string(argv[i]) == "--sysinfo")
            sysInfoOnly = true;
        else
            args.push_back(argv[i]);
    }
    argc = static_cast<int>(args.size());
    argv = args.data();

    if (!EnvironmentSetup::fromArguments(argc, argv)) {
        PLOG_ERROR << "AutoBleem " << Version::FULL_VERSION << ": cannot start";
        return EXIT_FAILURE;
    }
    if (sysInfoOnly) {
        // this is the command's output, not a log line
        for (const InfoSection &section : SystemInfoService().collect()) {
            cout << "[" << section.title << "]" << endl;
            for (const InfoRow &row : section.rows)
                cout << "  " << row.label << ": " << row.value << endl;
        }
        return EXIT_SUCCESS;
    }
    // the rolling structured log next to AB_out.txt; console lines keep going to stdout as well
    DirEntry::createDir(Env::getPathToLogsDir());
    ableem::Log::addFile(Env::getPathToLogsDir() + sep + "autobleem.log");

    // the first thing in a log anyone sends in: which build this is
    PLOG_INFO << "AutoBleem " << Version::FULL_VERSION << ", built " << Version::BUILD_TIMESTAMP << " UTC, "
              << Env::platformName();
    for (int i = 0; i < argc; i++)
        PLOG_INFO << "  argv[" << i << "] = " << argv[i];

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
        PLOG_ERROR << "FATAL: unhandled exception: " << e.what();
    } catch (...) {
        PLOG_ERROR << "FATAL: unhandled exception of unknown type";
    }
    return EXIT_FAILURE;
}
