#include "app.h"
#include "gui/extension_host_base.h"
#include "core/services/environment.h"
#include "core/services/retroarch_version.h"
#include "core/services/system.h"
#include "core/version.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <utility>

using namespace std;

namespace {
//******************
// LauncherExtensionHost
//******************
// What the launcher adds to an extension's host: its scan, and requests the launcher screen acts on in its
// own frame (App::takeExtensionRequests) - the Apps set, config.ini, the notification bubble.
class LauncherExtensionHost : public ExtensionHostBase {
public:
    LauncherExtensionHost(App &app, const ExtensionInfo &extension)
        : ExtensionHostBase(app, extension, Env::getPathToExtensionsStateDir()), launcher(app) {}

    void requestRescan(ScanScope scope) override {
        PLOG_INFO << "[" << name() << "] asked for a rescan, scope " << scope;
        launcher.scans().requestScan(scope);
    }
    void reloadApps() override { launcher.extensionRequests().reloadApps = true; }
    void reloadConfig() override { launcher.extensionRequests().reloadConfig = true; }
    void notify(const string &title, const string &detail, uint64_t done, uint64_t total) override {
        App::ExtensionRequests &r = launcher.extensionRequests();
        r.bubbleChanged = true;
        r.bubbleVisible = true;
        r.bubbleTitle = title;
        r.bubbleDetail = detail;
        r.bubbleDone = done;
        r.bubbleTotal = total;
    }
    void clearNotification() override {
        App::ExtensionRequests &r = launcher.extensionRequests();
        r.bubbleChanged = true;
        r.bubbleVisible = false;
    }

private:
    App &launcher;
};
} // namespace

//*******************************
// App::App
//*******************************
App::App(std::unique_ptr<ProcessRunner> runner)
    : AppBase("AutoBleem"), runner_(std::move(runner)),
      extensionCatalog_(Env::getPathToExtensionsDir(), Env::getPathToExtensionsStateDir(), Env::appPlatformKeys(),
                        AppManifest::pluginExtension(), Env::getPathToRuntimeDir()),
      extensions_(extensionCatalog_, pluginLoader_, [this](const ExtensionInfo &extension) {
          return unique_ptr<ExtensionHost>(new LauncherExtensionHost(*this, extension));
      }) {
    gameQuery_.setRetroArchGames(&retroArch_);
    gameQuery_.setLightguns(&lightguns_);
    gameQuery_.setPackages(&packages_);
    scans_.setPackages(&packages_);
    launcher_.setRaOptions(&raOptions_);
}

//*******************************
// App::startDriveWatcher
//*******************************
void App::startDriveWatcher() {
#ifndef _WIN32
    if (driveWatcher_)
        return;
    // dev only, for the VM that cannot set the environment: a disc image laid in as an extension folder
    // (Extensions/disc-fake*/fake.cue, any folder name starting so) stands for the drive's disc, as AB_FAKE_DRIVE does
    if (DriveWatcher::fakeCue().empty()) {
        const string extensions = Env::getPathToUSBRoot() + sep + "Extensions";
        DirEntries folders = DirEntry::diru_DirsOnly(extensions);
        sort(folders.begin(), folders.end(), DirEntry::sortDirEntryByName);
        for (const DirEntry &folder : folders) {
            if (folder.name.compare(0, 9, "disc-fake") != 0)
                continue;
            const string &name = folder.name;
            const string cue = extensions + sep + name + sep + "fake.cue";
            if (std::ifstream(cue).good()) {
                setenv("AB_FAKE_DRIVE", cue.c_str(), 1);
                const string eject = extensions + sep + name + sep + "eject";
                setenv("AB_FAKE_DRIVE_EJECT", eject.c_str(), 1);
                break;
            }
        }
    }
    driveWatcher_ = std::make_unique<DriveWatcher>(DriveWatcher::defaultBackend());
    driveWatcher_->start();
#endif
}

//*******************************
// App::fetchSbiFor
//*******************************
void App::fetchSbiFor(const string &serial, const string &saveFolder) {
#ifndef _WIN32
    if (serial.empty() || saveFolder.empty())
        return;
    if (!sbi_) {
        // the same command and site as the update check (its own fetch command when the platform has one)
        string command = Env::downloadCommand();
        if (command.empty()) {
            command = Env::updateDownloadCommand();
            Strings::replaceAll(command, "%r", Env::getWorkingPath());
        }
        if (command.empty() || Env::repoUrl().empty()) {
            PLOG_INFO << "SBI: no download here, " << serial << " plays without";
            return;
        }
        sbiDownloader_ = std::make_unique<Downloader>(command);
        // scratch in RAM: the launcher's runtime dir (AB_RUNTIME_DIR), not the stick
        const string scratch = Env::getPathToRuntimeDir() + sep + "sbi";
        sbi_ = std::make_unique<SbiService>(*sbiDownloader_, Env::repoUrl(), scratch);
    }
    try {
        const SbiService::Result result = sbi_->fetchFor(serial, saveFolder);
        PLOG_INFO << "SBI for " << serial << ": " << SbiService::describe(result);
    } catch (const std::exception &e) {
        PLOG_WARNING << "SBI for " << serial << " failed: " << e.what();
    }
#else
    (void)serial;
    (void)saveFolder;
#endif
}

//*******************************
// App::takeExtensionRequests
//*******************************
App::ExtensionRequests App::takeExtensionRequests() {
    ExtensionRequests taken = extensionRequests_;
    extensionRequests_.reloadApps = false;
    extensionRequests_.reloadConfig = false;
    extensionRequests_.bubbleChanged = false;
    extensionRequests_.message.clear();
    return taken;
}

//*******************************
// App::~App
//*******************************
App::~App() = default;

//*******************************
// App::requestPowerOff
//*******************************
void App::requestPowerOff() {
    PLOG_INFO << "Quit: App::requestPowerOff()";
#ifdef AB_PLATFORM_PSC
    session_.menuOption = MENU_OPTION_POWEROFF;
    gui_->input().requestQuit();
#else
    System::powerOff();
#endif
}

//*******************************
// App::applyOnlineSetting
//*******************************
void App::applyOnlineSetting() {
    OnlineAssets::Config online;
    online.downloadCommand = Env::downloadCommand();
    scans_.setOnline(cfg_.inifile.values["online"] == "true", online);
    scans_.setProcessorLanguage(cfg_.inifile.values["language"]); // the processors' AB_LANGUAGE
}

#ifdef AB_ONLINE_UPDATE
//*******************************
// App::applyUpdateSetting
//*******************************
// What the site's release.json calls this build's package and what is installed here. A Pi: "rpi" or
// "rpi64" by the CPU, RetroArch's version from the stamp install.sh writes; the PC stick is "pcusb" with
// the i386 RetroArch. Where RetroArch's catalog is on the site comes from the platform ini
// (retroarch_catalog, Env::retroArchCatalog()). The console is "psc-fs" (the stick package abupdate lays
// over the stick, and the RetroArch zip it lays over RetroArch/bin), checked only when it has a default route.
// A dev host tests the flow with AB_UPDATE_PLATFORM (a release key such as rpi) and
// AB_UPDATE_RETROARCH_VERSION in the environment; without them it looks for the "win" package and checks no
// RetroArch.
void App::applyUpdateSetting() {
    UpdateService::Config c;
    c.repoUrl = Env::repoUrl();
    c.channel = cfg_.inifile.values["updates"];
    // the package's version (its VERSION file - the site's name for the release or nightly it came from), not
    // this program's describe: a nightly in which only an emulator changed has the same launcher but a new
    // name. A build that was never packaged (a dev host) falls back to the describe
    c.installedVersion = Env::productVersion();
    // the catalogs through the scan's command (with its short timeout) where there is one, else the update's
    // own. %r is the launcher's own folder, where the
    // console's downloader (abfetch) is: the command is run from wherever the launcher happens to be
    string updateCommand = Env::updateDownloadCommand();
    Strings::replaceAll(updateCommand, "%r", Env::getWorkingPath());
    c.fetchCommand = Env::downloadCommand().empty() ? updateCommand : Env::downloadCommand();
    c.downloadCommand = updateCommand;
    c.stateFile = Env::getPathToSystemDir() + sep + "update.json";
    c.updatesDir = Env::getPathToSystemDir() + sep + "Updates";
    c.retroarchCatalog = Env::retroArchCatalog();
#if defined(AB_APPLIANCE)
#if defined(AB_PLATFORM_PCUSB)
    c.platformKey = "pcusb";
    c.arch = "i386";
#elif defined(__aarch64__)
    c.platformKey = "rpi64";
    c.arch = "arm64";
#else
    c.platformKey = "rpi";
    c.arch = "armhf";
#endif
    {
        ifstream stamp("/usr/local/share/autobleem/retroarch.version");
        string version;
        if (stamp && getline(stamp, version))
            c.installedRetroArch = Strings::trim(version);
    }
#elif defined(AB_PLATFORM_WIN)
    // the installer exe (AutoBleemSetup-<v>.exe, the site's "win-setup"); RetroArch is libretro's own
    // there and not ours to update (no arch = no RetroArch check)
    c.platformKey = "win-setup";
#elif defined(AB_PLATFORM_PSC)
    // a stock console has no network at all; the AutoBleem kernel brings WiFi (PSC-Bios sets it up) and its
    // USB network to a PC, which is no way out - only a default route counts
    c.platformKey = "psc-fs";
    c.networkUp = [] { return System::hasDefaultRoute(); };
    // RetroArch: the site lists the console's build as the one "zip" of psc/retroarch/latest.json; what is
    // installed is the stamp the PC installer (and abupdate) writes next to the binary - no stamp, no RetroArch
    // here, no check. abupdate applies the downloaded zip (rc/selection.sh)
    c.arch = "zip";
    // the stamp is a bare version on its first line, or the zip's own key=value file (retroarch_version=,
    // psc_build=) on a stick updated before the zip carried the first line
    c.installedRetroArch = retroarch_version::installed(Env::getPathToUSBRoot());
#else
    const char *platform = getenv("AB_UPDATE_PLATFORM");
    c.platformKey = platform != nullptr && *platform != 0 ? platform : "win";
    const char *raVersion = getenv("AB_UPDATE_RETROARCH_VERSION");
    if (raVersion != nullptr && *raVersion != 0) {
        c.installedRetroArch = raVersion;
        c.arch = c.platformKey == "rpi64" ? "arm64" : c.platformKey == "pcusb" ? "i386" : "armhf";
    }
#endif
    updates_.configure(c);
}
#endif
