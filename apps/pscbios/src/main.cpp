//
// PSC-Bios: the console's hardware configuration tool - WiFi, timezone, gamepad mapping - started by the
// launcher's Hardware Information item from Apps/pscbios/run.sh. Draws with the main GUI's theme, in its
// language; on a dev host `pscbios <usb root>` runs it against a fake console.
//
#include "core/console_backend.h"
#include "core/services/environment.h"
#include "core/services/environment_setup.h"
#include "core/version.h"
#include "pscbios_app.h"

#include <ableem/engine/log.h>
#include <ableem/ui/platform.h>

#include <cstdlib>
#include <iostream>
#include <memory>

using namespace std;

//*******************************
// runPscBios
//*******************************
static int runPscBios(int argc, char *argv[]) {
    cout.setf(ios::unitbuf);
    cerr.setf(ios::unitbuf);
    ableem::Log::initConsoleOnly();
    atexit(ableem::Platform::shutdownSDL);

    if (!EnvironmentSetup::forTool(argc, argv, "pscbios"))
        return EXIT_FAILURE;
    DirEntry::createDir(Env::getPathToLogsDir());
    ableem::Log::addFile(Env::getPathToLogsDir() + sep + "pscbios.log");
    PLOG_INFO << "PSC-Bios " << Version::FULL_VERSION << ", built " << Version::BUILD_TIMESTAMP << " UTC, "
              << Env::platformName() << ", app dir " << Env::getAppDir();

    // the console's abnet/settime scripts, or a fake with canned answers on a dev host
#ifdef AB_DEBUG_HOST
    unique_ptr<ConsoleBackend> console = make_unique<FakeBackend>();
#else
    unique_ptr<ConsoleBackend> console = make_unique<AbnetBackend>();
#endif
    if (!console->kernelInstalled()) {
        // the 2020 tool showed "Custom Firmware Kernel Not Found !" for five seconds and left; the screens
        // now show what they can without it, so the pads can still be tested and mapped
        PLOG_WARNING << "No AutoBleem kernel (" << AbnetBackend::Abnet << " missing): the network rows are off";
    }
    PscBios app(std::move(console));
    return app.run();
}

//*******************************
// main
//*******************************
int main(int argc, char *argv[]) {
    try {
        return runPscBios(argc, argv);
    } catch (const std::exception &e) {
        PLOG_ERROR << "FATAL: unhandled exception: " << e.what();
    } catch (...) {
        PLOG_ERROR << "FATAL: unhandled exception of unknown type";
    }
    return EXIT_FAILURE;
}
