//
// UpdateRoms: the console's way to a scanned, named, box-arted RetroArch library without a network of its
// own. Run from the USB stick (or a Pi's SD card) in a PC: it does what the launcher's own ROM scan does
// - the playlists from roms/<system>/, the names from RetroArch's databases - with the PC's network
// bringing the databases and the box art in, and writes the playlists with the paths the target will
// see. The console then boots and finds everything in place.
//
//   UpdateRoms.exe                  the stick it sits on (the first parent folder with Autobleem/bin/autobleem)
//   UpdateRoms.exe E:\              that stick
//   UpdateRoms.exe E:\ --quiet      no window: the same lines on stdout, for scripts
//   UpdateRoms.exe E:\ --target rpi  a Pi card whose kind could not be told (see UpdateRomsJob::detect)
//
#include "core/services/environment.h"
#include "core/update_roms_job.h"
#include "core/version.h"
#include "updateroms_app.h"

#include <ableem/engine/log.h>
#include <ableem/ui/platform.h>

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

} // namespace

//*******************************
// runUpdateRoms
//*******************************
static int runUpdateRoms(int argc, char *argv[]) {
    cout.setf(ios::unitbuf);
    cerr.setf(ios::unitbuf);
    ableem::Log::initConsoleOnly();
    atexit(ableem::Platform::shutdownSDL);

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
        else {
            cout << "USAGE: UpdateRoms [<usb root>] [--quiet] [--target psc|rpi]" << endl;
            return EXIT_FAILURE;
        }
    }
    if (!target.empty() && target != "psc" && target != "rpi") {
        cout << "--target is psc or rpi" << endl;
        return EXIT_FAILURE;
    }
    // the tool's own folder (its lang/ is there), pinned before anything can chdir
    string self = argv[0];
    size_t slash = self.find_last_of("/\\");
    Env::setAppDir(slash == string::npos ? Env::getAppDir() : self.substr(0, slash));
    if (root.empty())
        root = UpdateRomsJob::rootFromProgramPath(argv[0]);
    if (root.empty()) {
        cout << "This is not on an AutoBleem stick: run it from the stick's UpdateRoms folder, or name the stick's "
                "root: UpdateRoms E:\\"
             << endl;
        return EXIT_FAILURE;
    }

    UpdateRomsJob::Setup setup;
    string error;
    if (!UpdateRomsJob::detect(root, setup, error, target)) {
        cout << error << endl;
        return EXIT_FAILURE;
    }
    DirEntry::createDir(Env::getPathToLogsDir());
    ableem::Log::addFile(Env::getPathToLogsDir() + sep + "updateroms.log");
    PLOG_INFO << "UpdateRoms " << Version::FULL_VERSION << ", built " << Version::BUILD_TIMESTAMP << " UTC, stick "
              << setup.root << " for " << setup.target;

    if (quiet) {
        QuietListener listener;
        UpdateRomsJob::Report report =
            UpdateRomsJob::run(setup, &listener, nullptr, [](const string &line) { cout << line << endl; });
        return report.systems > 0 || report.games == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    UpdateRomsApp app(setup);
    return app.run();
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
