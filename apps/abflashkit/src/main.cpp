//
// ABFlashKit: the console's kernel installer - a recovery backup of the partitions to the USB stick, the
// AutoBleem kernel and its payload flashed, or the console sent into recovery to restore that backup.
// Started from Apps/abflashkit/run.sh. On a dev host `abflashkit <usb root>` runs the same screens against
// a fake flasher that writes a small pretend backup and touches no device.
//
#include "abflashkit_app.h"
#include "core/flasher.h"
#include "core/led.h"
#include "core/services/environment.h"
#include "core/services/environment_setup.h"
#include "core/version.h"

#include <ableem/engine/log.h>
#include <ableem/ui/platform.h>

#include <cstdlib>
#include <iostream>
#include <memory>

using namespace std;

//*******************************
// runAbFlashKit
//*******************************
static int runAbFlashKit(int argc, char *argv[]) {
    cout.setf(ios::unitbuf);
    cerr.setf(ios::unitbuf);
    ableem::Log::initConsoleOnly();
    atexit(ableem::Platform::shutdownSDL);

    if (!EnvironmentSetup::forTool(argc, argv, "abflashkit"))
        return EXIT_FAILURE;
    DirEntry::createDir(Env::getPathToLogsDir());
    ableem::Log::addFile(Env::getPathToLogsDir() + sep + "abflashkit.log");
    PLOG_INFO << "ABFlashKit " << Version::FULL_VERSION << ", built " << Version::BUILD_TIMESTAMP << " UTC, "
              << Env::platformName() << ", app dir " << Env::getAppDir();

#ifdef AB_DEBUG_HOST
    // the fake: the "partitions" are small files under <app dir>/fake, the backup is unpacked there too
    string fakeDir = Env::getAppDir() + sep + "fake";
    auto fake = make_unique<FakeFlasher>();
    fake->fakePartitionsDir = fakeDir + sep + "partitions";
    unique_ptr<Flasher> flasher = std::move(fake);
    unique_ptr<Led> led = make_unique<NullLed>();
    string scratchDir = fakeDir + sep + "lbootzip";
#else
    unique_ptr<Flasher> flasher = make_unique<ConsoleFlasher>();
    unique_ptr<Led> led = make_unique<SysfsLed>();
    string scratchDir = "/tmp/lbootzip";
#endif
    AbFlashKit app(std::move(flasher), std::move(led), scratchDir);
    return app.run();
}

//*******************************
// main
//*******************************
int main(int argc, char *argv[]) {
    try {
        return runAbFlashKit(argc, argv);
    } catch (const std::exception &e) {
        PLOG_ERROR << "FATAL: unhandled exception: " << e.what();
    } catch (...) {
        PLOG_ERROR << "FATAL: unhandled exception of unknown type";
    }
    return EXIT_FAILURE;
}
