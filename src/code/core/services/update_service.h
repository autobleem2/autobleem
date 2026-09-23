//
// UpdateService: the launcher's online update - is there a newer AutoBleem (or RetroArch, on a Pi) on the
// download repository, and fetching it when the user says yes. Built only with AB_ONLINE_UPDATE (CMake:
// on for the Pi and the dev hosts, off for the console, whose updater is an offline stick affair).
//
// The check: the channel's list on the site - "release" releases/latest.json (the newest stable), "testing"
// releases/unstable.json (the one pre-release; the stable list when there is none), "nightly"
// nightly/latest.json (the newest development build; else testing's, else release's) - and the platform's
// RetroArch list, fetched through the platform's download command in a worker thread. A version that is not
// the installed one is an update: the site names a release folder by its tag and a nightly folder by the
// launcher's git describe, and the installed version is this build's git describe (Version::DESCRIBE), so
// the two are the same string exactly when this is that build (the site keeps one of each, so "different"
// is "newer" - or another channel's, which is what switching channels is for). The old channel names still
// work ("stable" = release, "latest" = testing). Once at start and then every CheckInterval, unless
// config.ini's "updates" is off; "skip this version" / "remind me tomorrow" live in <usb>/System/update.json.
//
// The download: each needed tarball into <usb>/System/Updates/, sha256-checked against the catalog, then
// pending.json for the Pi's autobleem-update script (the launcher exits with MENU_OPTION_UPDATE and the
// session loop runs it - payload_linux/system/autobleem-session.sh). Progress is the growing file's size.
//
#pragma once

#include <ableem/engine/update_catalog.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

//******************
// UpdateInfo
//******************
struct UpdateInfo {
    std::string autobleemVersion; // the site's, when it differs from the installed one; "" = up to date
    ableem::UpdateFile autobleem;
    std::string retroarchVersion; // likewise, "" when up to date or not installed
    ableem::UpdateFile retroarch;
    bool any() const { return !autobleemVersion.empty() || !retroarchVersion.empty(); }
};

//******************
// UpdateService
//******************
class UpdateService {
public:
    struct Config {
        std::string repoUrl;     // "https://autobleem.retromenele.pl"
        std::string channel;     // "off" | "release" | "testing" | "nightly" (or the old "stable" | "latest")
        std::string platformKey; // the release.json files key: "rpi", "rpi64", "win", ...
        std::string arch;        // the retroarch latest.json key: "armhf", "arm64", "i386"; "" = no RetroArch check
        std::string retroarchCatalog; // that latest.json, relative to repoUrl ("rpi/retroarch/latest.json"); "" = none
        std::string installedVersion; // git describe ("v2.0.0-alpha2", "v2.0.0-alpha2-6-gba7365c")
        std::string installedRetroArch; // "" = RetroArch not installed (no check)
        std::string fetchCommand;       // the platform's download command, %u %o (short timeout is fine)
        std::string downloadCommand;    // the same without a timeout, for the tarballs
        std::string stateFile;          // <usb>/System/update.json
        std::string updatesDir;         // <usb>/System/Updates
    };
    using CommandRunner = std::function<int(const std::string &commandLine)>;

    enum class Phase { Idle, Checking, Checked, Downloading, Downloaded, Failed };
    struct Status {
        Phase phase = Phase::Idle;
        UpdateInfo info;
        std::string currentFile; // what is downloading
        uint64_t bytesDone = 0;
        uint64_t bytesTotal = 0;
        std::string error;
        bool checkedThisPoll = false; // the check finished since the last poll - the caller's cue to ask
    };

    static const int64_t CheckInterval = 24 * 60 * 60; // seconds

    explicit UpdateService(CommandRunner runner = CommandRunner());
    ~UpdateService();

    void configure(const Config &config);
    const Config &config() const { return config_; }
    bool enabled() const;

    // the check is wanted now: enabled, nothing in flight, and the last one was a day ago (or never)
    bool checkDue(int64_t now) const;
    void startCheck(int64_t now);
    // the check found something the user has not skipped or postponed
    bool shouldPrompt(int64_t now) const;
    void skip(int64_t now);
    void postpone(int64_t now);
    void startDownload();

    // main thread, once a frame: the worker's state, joined when it is done
    Status poll();
    const Status &status() const { return status_; }
    const ableem::UpdateState &state() const { return state_; }

    // the pure parts, for the tests
    static std::string channelFile(const std::string &channel);               // the channel's own list
    static std::vector<std::string> channelFiles(const std::string &channel); // it, then its fallbacks
    static UpdateInfo compare(const Config &config, const ableem::ReleaseCatalog *release,
                              const ableem::RetroArchCatalog *retroarch);
    static std::string commandFor(const std::string &commandTemplate, const std::string &url,
                                  const std::string &outPath);

private:
    void checkThread();
    void downloadThread();
    bool fetchText(const std::string &url, const std::string &outPath, std::string &text);
    bool downloadFile(const ableem::UpdateFile &file, std::string &outName);
    void finishWorker();
    void saveState();

    Config config_;
    CommandRunner runner_;
    ableem::UpdateState state_;
    Status status_;
    std::thread worker_;
    std::atomic<bool> workerDone_{false};
    std::mutex mutex_; // guards the worker's writes below
    UpdateInfo workerInfo_;
    std::string workerError_;
    std::string workerFile_;
    uint64_t workerTotal_ = 0;
    std::string workerOutPath_; // the .part being written - its size is the progress
    bool workerOk_ = false;
    bool checkPending_ = false; // a check finished, poll() has not reported it yet
};
