#include "app.h"
#include "core/services/environment.h"
#include "core/services/system.h"
#include "core/version.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <utility>

using namespace std;

//*******************************
// App::App
//*******************************
App::App(std::unique_ptr<ProcessRunner> runner) : AppBase("AutoBleem"), runner_(std::move(runner)) {
    gameQuery_.setRetroArchGames(&retroArch_);
    gameQuery_.setLightguns(&lightguns_);
}

//*******************************
// App::~App
//*******************************
App::~App() = default;

//*******************************
// App::applyOnlineSetting
//*******************************
void App::applyOnlineSetting() {
    OnlineAssets::Config online;
    online.downloadCommand = Env::downloadCommand();
    scans_.setOnline(cfg_.inifile.values["online"] == "true", online);
}

#ifdef AB_ONLINE_UPDATE
//*******************************
// App::applyUpdateSetting
//*******************************
// What the site's release.json calls this build's package and what is installed here. A Pi: "rpi" or
// "rpi64" by the CPU, RetroArch's version from the stamp install.sh writes; the PC stick is "pcusb" with
// the i386 RetroArch. Where RetroArch's catalog is on the site comes from the platform ini
// (retroarch_catalog, Env::retroArchCatalog()). A dev host tests the flow with AB_UPDATE_PLATFORM (a
// release key such as rpi) and AB_UPDATE_RETROARCH_VERSION in the environment; without them it looks for
// the "win" package and checks no RetroArch.
void App::applyUpdateSetting() {
    UpdateService::Config c;
    c.repoUrl = Env::repoUrl();
    c.channel = cfg_.inifile.values["updates"];
    c.installedStable = Version::VERSION;
    c.installedVersion = string(Version::VERSION) + "-" + Version::GIT_HASH;
    c.fetchCommand = Env::downloadCommand();
    c.downloadCommand = Env::updateDownloadCommand();
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
