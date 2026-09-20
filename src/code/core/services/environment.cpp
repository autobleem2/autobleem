#include "environment.h"
#include "../main.h"

using namespace std;

bool Env::autobleemKernel = false;
bool Env::hiddenMenuEnabled = false;

namespace {
vector<string> retroArchBinaries_{"retroarch"};
string downloadCommand_;
string repoUrl_;
string updateDownloadCommand_;
string retroArchCatalog_;
bool directLaunch_ = false;
string pcsxDir_;
string pcsxNxtDir_;
} // namespace

//*******************************
// Env::platformName
//*******************************
const char *Env::platformName() {
#if defined(AB_PLATFORM_PSC)
    return "psc";
#elif defined(AB_PLATFORM_RPI)
    return "rpi";
#elif defined(AB_PLATFORM_PCUSB)
    return "pcusb";
#elif defined(AB_PLATFORM_WIN)
    return "win";
#else
    return "pc";
#endif
}

//*******************************
// Env::padMappingFiles
//*******************************
std::vector<std::string> Env::padMappingFiles() {
    std::vector<std::string> files;
    if (!getPathToKernelConfigDir().empty())
        files.push_back(getPathToKernelConfigDir() + sep + "gamecontrollerdb.txt");
    files.push_back(getPathToGameControllerDb());
    return files;
}

//*******************************
// Env:: RetroArch binaries
//*******************************
void Env::setRetroArchBinaries(const vector<string> &paths) {
    retroArchBinaries_ = paths;
}

//*******************************
// Env::setDownloadCommand / downloadCommand
//*******************************
void Env::setDownloadCommand(const string &command) {
    downloadCommand_ = command;
}
const string &Env::downloadCommand() {
    return downloadCommand_;
}
void Env::setUpdateSource(const string &repoUrl, const string &downloadCommand, const string &retroarchCatalog) {
    repoUrl_ = repoUrl;
    while (!repoUrl_.empty() && repoUrl_.back() == '/')
        repoUrl_.pop_back();
    updateDownloadCommand_ = downloadCommand;
    retroArchCatalog_ = retroarchCatalog;
    while (!retroArchCatalog_.empty() && retroArchCatalog_.front() == '/')
        retroArchCatalog_.erase(0, 1);
}
const string &Env::retroArchCatalog() {
    return retroArchCatalog_;
}
void Env::setDirectLaunch(bool direct) {
    directLaunch_ = direct;
}
bool Env::directLaunch() {
    return directLaunch_;
}
void Env::setPcsxDir(const string &path) {
    pcsxDir_ = path;
}
const string &Env::pcsxDir() {
    return pcsxDir_;
}
void Env::setPcsxNxtDir(const string &path) {
    pcsxNxtDir_ = path;
}
const string &Env::pcsxNxtDir() {
    return pcsxNxtDir_;
}
const string &Env::repoUrl() {
    return repoUrl_;
}
const string &Env::updateDownloadCommand() {
    return updateDownloadCommand_;
}
const vector<string> &Env::retroArchBinaries() {
    return retroArchBinaries_;
}

bool Env::retroArchInstalled() {
    for (const string &path : retroArchBinaries_) {
        if (DirEntry::exists(path))
            return true;
    }
    return false;
}
