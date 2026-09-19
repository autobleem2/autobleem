//
// UpdateRoms: the console's way to a scanned, named, box-arted RetroArch library without a network of its
// own. Run from the USB stick (or a Pi's SD card) in a PC: it does what the launcher's own ROM scan does
// - the playlists from roms/<system>/, the names from RetroArch's databases - with the PC's network
// bringing the databases and the box art in, and writes the playlists with the paths the target will
// see. The console then boots and finds everything in place.
//
//   UpdateRoms.exe                  the stick it sits on (the first parent folder with Autobleem/bin/autobleem)
//   UpdateRoms.exe E:\              that stick
//   UpdateRoms.exe E:\ --quiet      no window: the same lines on the console it was started from, for scripts
//   UpdateRoms.exe E:\ --target rpi  a Pi card whose kind could not be told (see UpdateRomsJob::detect)
//
// On Windows a plain Win32 window (win32_window.cpp), statically linked: one file, nothing to install.
// Elsewhere (a Linux or macOS build) there is the --quiet path only.
//
#include "core/main.h"
#include "core/services/environment.h"
#include "core/update_roms_job.h"
#include "core/version.h"
#include "win32_window.h"

#include <ableem/engine/log.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

namespace {

//*******************************
// QuietListener
//*******************************
// --quiet: the scanner's progress on stdout, one line per folder and per cover
class QuietListener : public ableem::ScanProgressListener {
public:
    void onScanProgress(ableem::ScanStage stage, const string &detail, int done, int total) override {
        if (stage == ableem::ScanStage::ScanningRoms)
            cout << "[" << done << "/" << total << "] " << detail << endl;
        else if (stage == ableem::ScanStage::FetchingBoxArt)
            cout << "  box art " << done << "/" << total << ": " << detail << endl;
    }
};

// what a usage mistake or a stick that is not one gets: the console when there is one, a box otherwise
int fail(const string &message, bool quiet) {
#ifdef _WIN32
    if (!quiet)
        return runUpdateRomsWindow(UpdateRomsJob::Setup(), message);
#endif
    (void)quiet;
    cout << message << endl;
    return EXIT_FAILURE;
}

} // namespace

//*******************************
// runUpdateRoms
//*******************************
static int runUpdateRoms(int argc, char *argv[]) {
    bool quiet = false;
    string root, target;
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--quiet")
            quiet = true;
        else if (arg == "--target" && i + 1 < argc)
            target = argv[++i];
        else if (root.empty() && arg.rfind("--", 0) != 0)
            root = arg;
        else
            return fail("USAGE: UpdateRoms [<usb root>] [--quiet] [--target psc|rpi]", quiet);
    }
#ifdef _WIN32
    if (quiet)
        attachParentConsole();
#endif
    cout.setf(ios::unitbuf);
    cerr.setf(ios::unitbuf);
    ableem::Log::initConsoleOnly();

    if (!target.empty() && target != "psc" && target != "rpi")
        return fail("--target is psc or rpi", quiet);

    // the tool's own folder, pinned before anything can chdir
    string self = argv[0];
    size_t slash = self.find_last_of("/\\");
    Env::setAppDir(slash == string::npos ? Env::getAppDir() : self.substr(0, slash));
    if (root.empty())
        root = UpdateRomsJob::rootFromProgramPath(argv[0]);
    if (root.empty()) {
        return fail("This is not on an AutoBleem stick: run it from the stick's UpdateRoms folder, or name the "
                    "stick's root: UpdateRoms E:\\",
                    quiet);
    }

    UpdateRomsJob::Setup setup;
    string error;
    if (!UpdateRomsJob::detect(root, setup, error, target))
        return fail(error, quiet);
    DirEntry::createDir(Env::getPathToLogsDir());
    ableem::Log::addFile(Env::getPathToLogsDir() + sep + "updateroms.log");
    PLOG_INFO << "UpdateRoms " << Version::FULL_VERSION << ", built " << Version::BUILD_TIMESTAMP << " UTC, stick "
              << setup.root << " for " << setup.target;

#ifdef _WIN32
    if (!quiet)
        return runUpdateRomsWindow(setup, "");
#endif
    QuietListener listener;
    UpdateRomsJob::Report report =
        UpdateRomsJob::run(setup, &listener, nullptr, [](const string &line) { cout << line << endl; });
    return report.systems > 0 || report.games == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

//*******************************
// main
//*******************************
int main(int argc, char *argv[]) {
    try {
        return runUpdateRoms(argc, argv);
    } catch (const std::exception &e) {
        PLOG_ERROR << "FATAL: unhandled exception: " << e.what();
    } catch (...) {
        PLOG_ERROR << "FATAL: unhandled exception of unknown type";
    }
    return EXIT_FAILURE;
}
