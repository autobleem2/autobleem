//
// PlatformConfig - see the header.
//
#include "platform_config.h"
#include "environment.h"
#include "../main.h"
#include <iostream>
#include <map>

using namespace std;

namespace {
    // a relative path is taken from `base`; an absolute one stands
    string under(const string &base, const string &path) {
        if (path.empty()) return base;
        if (path[0] == '/' || (path.size() > 1 && path[1] == ':')) return path;   // absolute (a Windows drive too)
        return base + sep + path;
    }
}

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
        if (end == string::npos) end = value.size();
        string item = Strings::trim(value.substr(start, end - start));
        if (!item.empty()) out.push_back(item);
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
        cout << "No platform config " << iniPath << " - using the console's layout" << endl;
        return cfg;
    }
    IniFile ini;
    ini.load(iniPath);   // keys are lower-cased on load, but keep the whitespace before '=': trim them here
    cout << "Platform config: " << iniPath << endl;

    map<string, string> values;
    for (const auto &kv : ini.values) values[Strings::trim(kv.first)] = Strings::trim(kv.second);
    auto value = [&values](const char *key) { return values[key]; };
    if (!value("retroarch_dir").empty())    cfg.retroarchDir = value("retroarch_dir");
    if (!value("retroarch_core").empty())   cfg.retroarchCore = value("retroarch_core");
    if (!value("retroarch_binary").empty()) cfg.retroarchBinaries = splitList(value("retroarch_binary"));
    return cfg;
}

//*******************************
// PlatformConfig::apply
//*******************************
void PlatformConfig::apply() const {
    string raDir = under(Env::getPathToUSBRoot(), retroarchDir);
    Env::setRetroarchDir(raDir);
    Env::setRetroarchCoreFile(under(raDir, retroarchCore));

    vector<string> binaries;
    for (const string &b : retroarchBinaries) binaries.push_back(under(raDir, b));
    Env::setRetroArchBinaries(binaries);
}
