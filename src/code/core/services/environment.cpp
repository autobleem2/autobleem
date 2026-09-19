#include "environment.h"
#include "../main.h"

using namespace std;

bool Env::autobleemKernel = false;
bool Env::hiddenMenuEnabled = false;

namespace {
vector<string> retroArchBinaries_{"retroarch"};
string downloadCommand_;
} // namespace

//*******************************
// Env::platformName
//*******************************
const char *Env::platformName() {
#if defined(AB_PLATFORM_RPI)
    return "rpi";
#elif defined(AB_DEBUG_HOST)
    return "pc";
#else
    return "psc";
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
