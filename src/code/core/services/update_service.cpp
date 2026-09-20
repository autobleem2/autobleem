#include "system.h"
#include "update_service.h"

#include <ableem/engine/filesystem.h>
#include <ableem/engine/log.h>
#include <ableem/engine/sha256.h>
#include <ableem/engine/strings.h>

#include <cstdlib>
#include <fstream>
#include <sstream>

using namespace std;
using namespace ableem;

namespace {

bool createDirs(const string &dir) {
    if (dir.empty() || DirEntry::isDirectory(dir))
        return true;
    size_t slash = dir.find_last_of('/');
    if (slash != string::npos && slash > 0 && !createDirs(dir.substr(0, slash)))
        return false;
    DirEntry::createDir(dir);
    return DirEntry::isDirectory(dir);
}

string readText(const string &path) {
    ifstream in(path, ios::binary);
    if (!in)
        return "";
    stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

string baseName(const string &url) {
    size_t slash = url.find_last_of('/');
    return slash == string::npos ? url : url.substr(slash + 1);
}

} // namespace

//*******************************
// UpdateService::UpdateService
//*******************************
UpdateService::UpdateService(CommandRunner runner) : runner_(std::move(runner)) {
    if (!runner_)
        runner_ = [](const string &commandLine) { return System::runShellCommand(commandLine); };
}

UpdateService::~UpdateService() {
    if (worker_.joinable())
        worker_.join();
}

//*******************************
// UpdateService::configure
//*******************************
void UpdateService::configure(const Config &config) {
    config_ = config;
    state_ = UpdateState();
    if (!config_.stateFile.empty())
        state_.load(config_.stateFile);
}

void UpdateService::saveState() {
    if (config_.stateFile.empty())
        return;
    size_t slash = config_.stateFile.find_last_of('/');
    if (slash != string::npos)
        createDirs(config_.stateFile.substr(0, slash));
    state_.save(config_.stateFile);
}

bool UpdateService::enabled() const {
    return config_.channel != "off" && !config_.channel.empty() && !config_.repoUrl.empty() &&
           !config_.fetchCommand.empty() && !config_.platformKey.empty();
}

//*******************************
// UpdateService::channelFile
//*******************************
string UpdateService::channelFile(const string &channel) {
    return channel == "latest" ? "releases/unstable.json" : "releases/latest.json";
}

//*******************************
// UpdateService::commandFor
//*******************************
string UpdateService::commandFor(const string &commandTemplate, const string &url, const string &outPath) {
    string cmd = commandTemplate;
    Strings::replaceAll(cmd, "%u", url);
    Strings::replaceAll(cmd, "%o", outPath);
    return cmd;
}

//*******************************
// UpdateService::compare
//*******************************
// What is newer than what is installed. AutoBleem: the site's version against the tag-hash (latest) or
// the tag (stable) - a pre-release build on the stable channel sees the stable release as an update, which
// is the point of switching channels. RetroArch: only where one is installed (a stamp to compare with) and
// the catalog has a build for this architecture.
UpdateInfo UpdateService::compare(const Config &config, const ReleaseCatalog *release,
                                  const RetroArchCatalog *retroarch) {
    UpdateInfo info;
    if (release != nullptr && !release->version.empty()) {
        const string installed = config.channel == "latest" ? config.installedVersion : config.installedStable;
        const UpdateFile *file = release->fileFor(config.platformKey);
        if (file != nullptr && release->version != installed) {
            info.autobleemVersion = release->version;
            info.autobleem = *file;
        }
    }
    if (retroarch != nullptr && !retroarch->version.empty() && !config.installedRetroArch.empty() &&
        !config.arch.empty()) {
        const UpdateFile *file = retroarch->fileFor(config.arch);
        if (file != nullptr && retroarch->version != config.installedRetroArch) {
            info.retroarchVersion = retroarch->version;
            info.retroarch = *file;
        }
    }
    return info;
}

//*******************************
// UpdateService::checkDue
//*******************************
bool UpdateService::checkDue(int64_t now) const {
    if (!enabled() || worker_.joinable())
        return false;
    if (status_.phase == Phase::Downloading || status_.phase == Phase::Downloaded)
        return false;
    return state_.lastCheck == 0 || now - state_.lastCheck >= CheckInterval || now < state_.lastCheck;
}

//*******************************
// UpdateService::startCheck
//*******************************
void UpdateService::startCheck(int64_t now) {
    if (!enabled() || worker_.joinable())
        return;
    state_.lastCheck = now;
    saveState();
    status_.phase = Phase::Checking;
    status_.error.clear();
    status_.checkedThisPoll = false;
    workerDone_ = false;
    workerOk_ = false;
    worker_ = thread([this] { checkThread(); });
}

//*******************************
// UpdateService::fetchText
//*******************************
bool UpdateService::fetchText(const string &url, const string &outPath, string &text) {
    createDirs(config_.updatesDir);
    DirEntry::removeFile(outPath);
    const int status = runner_(commandFor(config_.fetchCommand, url, outPath));
    if (status != 0 || DirEntry::fileSize(outPath) <= 0) {
        DirEntry::removeFile(outPath);
        return false;
    }
    text = readText(outPath);
    DirEntry::removeFile(outPath);
    return !text.empty();
}

//*******************************
// UpdateService::checkThread
//*******************************
void UpdateService::checkThread() {
    UpdateInfo info;
    string error;
    string text;
    ReleaseCatalog release;
    RetroArchCatalog retroarch;
    bool haveRelease = false, haveRetroArch = false;

    const string base = config_.repoUrl + "/";
    const string scratch = config_.updatesDir + sep + "check.json";
    if (fetchText(base + channelFile(config_.channel), scratch, text) && release.parse(text)) {
        haveRelease = true;
    } else if (config_.channel == "latest" && fetchText(base + channelFile("stable"), scratch, text) &&
               release.parse(text)) {
        haveRelease = true; // no pre-release on the site: the newest stable is the latest there is
    } else {
        error = "cannot read the release list";
    }
    if (!config_.installedRetroArch.empty() && !config_.arch.empty() && !config_.retroarchCatalog.empty()) {
        if (fetchText(base + config_.retroarchCatalog, scratch, text) && retroarch.parse(text))
            haveRetroArch = true;
    }
    if (haveRelease || haveRetroArch) {
        info = compare(config_, haveRelease ? &release : nullptr, haveRetroArch ? &retroarch : nullptr);
        if (haveRelease)
            error.clear();
    }

    lock_guard<mutex> lock(mutex_);
    workerInfo_ = info;
    workerError_ = error;
    workerOk_ = haveRelease || haveRetroArch;
    workerDone_ = true;
}

//*******************************
// UpdateService::shouldPrompt
//*******************************
bool UpdateService::shouldPrompt(int64_t now) const {
    if (!status_.info.any())
        return false;
    if (now < state_.postponedUntil)
        return false;
    const bool abSkipped =
        status_.info.autobleemVersion.empty() || status_.info.autobleemVersion == state_.skippedVersion;
    const bool raSkipped =
        status_.info.retroarchVersion.empty() || status_.info.retroarchVersion == state_.skippedRetroArch;
    return !(abSkipped && raSkipped);
}

//*******************************
// UpdateService::skip / postpone
//*******************************
void UpdateService::skip(int64_t now) {
    (void)now;
    if (!status_.info.autobleemVersion.empty())
        state_.skippedVersion = status_.info.autobleemVersion;
    if (!status_.info.retroarchVersion.empty())
        state_.skippedRetroArch = status_.info.retroarchVersion;
    saveState();
}

void UpdateService::postpone(int64_t now) {
    state_.postponedUntil = now + CheckInterval;
    saveState();
}

//*******************************
// UpdateService::startDownload
//*******************************
void UpdateService::startDownload() {
    if (worker_.joinable() || !status_.info.any())
        return;
    status_.phase = Phase::Downloading;
    status_.error.clear();
    status_.bytesDone = 0;
    status_.bytesTotal = status_.info.autobleem.size + status_.info.retroarch.size;
    workerInfo_ = status_.info; // the worker's own copy: poll() keeps writing status_ meanwhile
    workerDone_ = false;
    workerOk_ = false;
    worker_ = thread([this] { downloadThread(); });
}

//*******************************
// UpdateService::downloadFile
//*******************************
bool UpdateService::downloadFile(const UpdateFile &file, string &outName) {
    outName = file.name.empty() ? baseName(file.url) : file.name;
    const string target = config_.updatesDir + sep + outName;
    const string part = target + ".part";
    {
        lock_guard<mutex> lock(mutex_);
        workerFile_ = outName;
        workerTotal_ = file.size;
        workerOutPath_ = part;
    }
    // already there from an earlier attempt, and right: no download
    if (DirEntry::fileSize(target) == static_cast<long long>(file.size) && Sha256::ofFile(target) == file.sha256)
        return true;
    DirEntry::removeFile(part);
    const int status = runner_(commandFor(config_.downloadCommand, file.url, part));
    if (status != 0 || DirEntry::fileSize(part) <= 0) {
        PLOG_WARNING << "Update download failed (" << status << "): " << file.url;
        DirEntry::removeFile(part);
        return false;
    }
    const string sum = Sha256::ofFile(part);
    if (sum != file.sha256) {
        PLOG_WARNING << "Update download " << outName << " has sha256 " << sum << ", the site says " << file.sha256;
        DirEntry::removeFile(part);
        return false;
    }
    DirEntry::removeFile(target);
    return DirEntry::renameFile(part, target);
}

//*******************************
// UpdateService::downloadThread
//*******************************
void UpdateService::downloadThread() {
    string error;
    PendingUpdate pending;
    bool ok = createDirs(config_.updatesDir);
    if (!ok)
        error = "cannot create " + config_.updatesDir;
    const UpdateInfo info = workerInfo_;
    if (ok && !info.autobleemVersion.empty()) {
        ok = downloadFile(info.autobleem, pending.autobleemFile);
        if (ok)
            pending.autobleemVersion = info.autobleemVersion;
        else
            error = "the AutoBleem package could not be downloaded";
    }
    if (ok && !info.retroarchVersion.empty()) {
        ok = downloadFile(info.retroarch, pending.retroarchFile);
        if (ok)
            pending.retroarchVersion = info.retroarchVersion;
        else
            error = "the RetroArch build could not be downloaded";
    }
    if (ok && !pending.save(config_.updatesDir + sep + "pending.json")) {
        ok = false;
        error = "cannot write pending.json";
    }
    lock_guard<mutex> lock(mutex_);
    workerError_ = error;
    workerOk_ = ok;
    workerDone_ = true;
}

//*******************************
// UpdateService::finishWorker
//*******************************
void UpdateService::finishWorker() {
    worker_.join();
    lock_guard<mutex> lock(mutex_);
    if (status_.phase == Phase::Checking) {
        status_.info = workerInfo_;
        status_.error = workerError_;
        status_.phase = workerOk_ ? Phase::Checked : Phase::Failed;
        status_.checkedThisPoll = true;
        if (workerOk_) {
            PLOG_INFO << "Update check: " << (status_.info.any() ? "an update is available" : "up to date")
                      << (status_.info.autobleemVersion.empty() ? "" : " AutoBleem " + status_.info.autobleemVersion)
                      << (status_.info.retroarchVersion.empty() ? "" : " RetroArch " + status_.info.retroarchVersion);
        } else {
            PLOG_WARNING << "Update check failed: " << status_.error;
        }
    } else if (status_.phase == Phase::Downloading) {
        status_.error = workerError_;
        status_.phase = workerOk_ ? Phase::Downloaded : Phase::Failed;
        status_.bytesDone = workerOk_ ? status_.bytesTotal : status_.bytesDone;
    }
}

//*******************************
// UpdateService::poll
//*******************************
UpdateService::Status UpdateService::poll() {
    status_.checkedThisPoll = false;
    if (worker_.joinable()) {
        if (workerDone_) {
            finishWorker();
        } else if (status_.phase == Phase::Downloading) {
            // progress: the .part file's size, plus the files already done
            lock_guard<mutex> lock(mutex_);
            status_.currentFile = workerFile_;
            uint64_t done = 0;
            if (!status_.info.autobleemVersion.empty() && workerFile_ != status_.info.autobleem.name &&
                !status_.info.retroarchVersion.empty())
                done += status_.info.autobleem.size; // the first file is finished, the second is running
            const long long partSize = workerOutPath_.empty() ? 0 : DirEntry::fileSize(workerOutPath_);
            if (partSize > 0)
                done += static_cast<uint64_t>(partSize);
            status_.bytesDone = done;
        }
    }
    return status_;
}
