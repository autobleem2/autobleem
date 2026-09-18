//
// SsidConfig: ssid.cfg in and out.
//
#include "ssid_config.h"
#include "core/main.h"
#include "core/services/environment.h"

#include <fstream>

using namespace std;

namespace {
string lineWithoutCr(istream &in) {
    string line;
    getline(in, line);
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
        line.pop_back();
    return line;
}
} // namespace

//*******************************
// SsidConfig::load
//*******************************
bool SsidConfig::load(const string &path) {
    ifstream in(path, ios::binary);
    if (!in)
        return false;
    ssid = lineWithoutCr(in);
    password = lineWithoutCr(in);
    driverMode = lineWithoutCr(in);
    if (driverMode.empty())
        driverMode = "wext";
    return true;
}

//*******************************
// SsidConfig::save
//*******************************
bool SsidConfig::save(const string &path) const {
    if (ssid.empty() || password.empty())
        return false;
    ofstream out(path, ios::binary | ios::trunc);
    if (!DirEntry::checkWritable(out, path))
        return false;
    out << ssid << "\n" << password << "\n" << driverMode << "\n";
    return out.good();
}

//*******************************
// SsidConfig::ssidFromWpaSupplicant
//*******************************
string SsidConfig::ssidFromWpaSupplicant(const string &path) {
    ifstream in(path, ios::binary);
    if (!in)
        return "";
    string line;
    while (getline(in, line)) {
        line = Strings::trim(line);
        size_t pos = line.find("ssid=");
        if (line.empty() || pos == string::npos)
            continue;
        string value = line.substr(pos + 5);
        value.erase(remove(value.begin(), value.end(), '"'), value.end());
        value = Strings::trim(value);
        return value == "1" ? "" : value; // "1" is what an old setup wrote without an ssid.cfg
    }
    return "";
}

//*******************************
// SsidConfig::defaultPath / wpaSupplicantPath
//*******************************
string SsidConfig::defaultPath() {
    string kernelDir = Env::getPathToKernelConfigDir();
    return (kernelDir.empty() ? Env::getAppDir() : kernelDir) + sep + "ssid.cfg";
}

string SsidConfig::wpaSupplicantPath() {
    return Env::getPathToKernelConfigDir().empty() ? Env::getAppDir() + sep + "wpa_supplicant.conf"
                                                   : "/etc/wpa_supplicant.conf";
}
