//
// PlatformConfig - see the header.
//
#include "platform_config.h"
#include "environment.h"
#include "../main.h"
#include <iostream>
#include <map>
#include <ableem/engine/log.h>

using namespace std;

namespace {
// a relative path is taken from `base`; an absolute one stands
string under(const string &base, const string &path) {
    if (path.empty())
        return base;
    if (path[0] == '/' || (path.size() > 1 && path[1] == ':'))
        return path; // absolute (a Windows drive too)
    return base + sep + path;
}
} // namespace

//*******************************
// PlatformConfig::pathFor
//*******************************
string PlatformConfig::pathFor(const string &resourcesDir, const string &platformName) {
    return resourcesDir + sep + "platform" + sep + platformName + ".ini";
}

//*******************************
// PlatformConfig::splitList
//*******************************
vector<string> PlatformConfig::splitList(const string &value) {
    vector<string> out;
    string::size_type start = 0;
    while (start <= value.size()) {
        string::size_type end = value.find(';', start);
        if (end == string::npos)
            end = value.size();
        string item = Strings::trim(value.substr(start, end - start));
        if (!item.empty())
            out.push_back(item);
        start = end + 1;
    }
    return out;
}

//*******************************
// PlatformConfig::load
//*******************************
PlatformConfig PlatformConfig::load(const string &iniPath) {
    PlatformConfig cfg;
    if (!DirEntry::exists(iniPath)) {
        PLOG_INFO << "No platform config " << iniPath << " - using the console's layout";
        return cfg;
    }
    IniFile ini;
    ini.load(iniPath); // keys are lower-cased on load, but keep the whitespace before '=': trim them here
    PLOG_INFO << "Platform config: " << iniPath;

    map<string, string> values;
    for (const auto &kv : ini.values)
        values[Strings::trim(kv.first)] = Strings::trim(kv.second);
    auto value = [&values](const char *key) { return values[key]; };
    if (!value("retroarch_dir").empty())
        cfg.retroarchDir = value("retroarch_dir");
    if (!value("retroarch_core").empty())
        cfg.retroarchCore = value("retroarch_core");
    if (!value("retroarch_binary").empty())
        cfg.retroarchBinaries = splitList(value("retroarch_binary"));
    if (!value("retroarch_roms_dir").empty())
        cfg.retroarchRomsDir = value("retroarch_roms_dir");
    if (!value("retroarch_bios_dir").empty())
        cfg.retroarchBiosDir = value("retroarch_bios_dir");
    cfg.downloadCommand = value("download_command");
    cfg.repoUrl = value("repo_url");
    cfg.updateDownloadCommand = value("update_download_command");
    cfg.retroarchCatalog = value("retroarch_catalog");
    cfg.usbRoot = value("usb_root");
    if (!value("launch_mode").empty()) {
        if (value("launch_mode") == "script" || value("launch_mode") == "direct")
            cfg.launchMode = value("launch_mode");
        else
            PLOG_WARNING << "launch_mode=" << value("launch_mode") << " in " << iniPath
                         << " - script or direct; using script";
    }
    if (!value("core_extension").empty()) {
        cfg.coreExtension = value("core_extension");
        if (cfg.coreExtension[0] != '.')
            cfg.coreExtension = "." + cfg.coreExtension;
    }
    cfg.pcsxDir = value("pcsx_dir");
    cfg.pcsxNxtDir = value("pcsxnxt_dir");
    return cfg;
}

//*******************************
// PlatformConfig::apply
//*******************************
void PlatformConfig::apply() const {
    string raDir = under(Env::getPathToUSBRoot(), retroarchDir);
    Env::setRetroarchDir(raDir);
    Env::setRetroarchCoreFile(retroarchCore.empty() ? "" : under(raDir, retroarchCore));

    vector<string> binaries;
    for (const string &b : retroarchBinaries)
        binaries.push_back(under(raDir, b));
    Env::setRetroArchBinaries(binaries);
    Env::setRetroarchRomsDir(under(Env::getPathToUSBRoot(), retroarchRomsDir));
    Env::setRetroarchBiosDir(under(Env::getPathToUSBRoot(), retroarchBiosDir));
    Env::setDownloadCommand(downloadCommand);
    Env::setUpdateSource(repoUrl, updateDownloadCommand, retroarchCatalog);
    Env::setRetroarchCoreExtension(coreExtension);
    Env::setDirectLaunch(launchMode == "direct");
    Env::setPcsxDir(pcsxDir.empty() ? "" : under(Env::getWorkingPath(), pcsxDir));
    Env::setPcsxNxtDir(pcsxNxtDir.empty() ? "" : under(Env::getWorkingPath(), pcsxNxtDir));
}
