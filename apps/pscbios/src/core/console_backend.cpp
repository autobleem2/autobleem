//
// ConsoleBackend: the abnet/settime calls, and the dev host's stand-in.
//
#include "console_backend.h"
#include "core/main.h"
#include "core/services/system.h"

#include <ableem/engine/log.h>

#include <algorithm>

using namespace std;

const char *const AbnetBackend::Abnet = "/bin/abnet";
const char *const AbnetBackend::Settime = "/bin/settime";

//*******************************
// AbnetBackend
//*******************************
namespace {
string runViaSystem(const string &cmd) {
    return System::execUnixCommand(cmd.c_str());
}
vector<string> runLinesViaSystem(const string &cmd) {
    return System::execUnixCommandLines(cmd);
}
// a shell argument in double quotes, with what the shell would otherwise interpret inside them escaped
string quoted(const string &value) {
    string out = "\"";
    for (char c : value) {
        if (c == '"' || c == '\\' || c == '$' || c == 0x60) // 0x60: the backquote
            out += '\\';
        out += c;
    }
    return out + "\"";
}
} // namespace

AbnetBackend::AbnetBackend() : run_(runViaSystem), runLines_(runLinesViaSystem), kernel_(DirEntry::exists(Abnet)) {}

AbnetBackend::AbnetBackend(Runner run, LinesRunner runLines, bool kernel)
    : run_(run), runLines_(runLines), kernel_(kernel) {}

bool AbnetBackend::interfaceFound(const string &iface) {
    // "list_ifaces" answers with the names on one line
    return run_(string(Abnet) + " list_ifaces").find(iface) != string::npos;
}

bool AbnetBackend::wlanOn() {
    return run_(string(Abnet) + " wlan_on") == "yes";
}

bool AbnetBackend::isUp(const string &iface) {
    return run_(string(Abnet) + " is_up " + iface) == "yes";
}

string AbnetBackend::ipOf(const string &iface) {
    if (!interfaceFound(iface))
        return "";
    return run_(string(Abnet) + " show_ip " + iface);
}

vector<string> AbnetBackend::scanSsids() {
    if (!interfaceFound("wlan0") || !isUp("wlan0"))
        return {};
    vector<string> ssids = runLines_(string(Abnet) + " scan");
    sort(ssids.begin(), ssids.end());
    ssids.erase(unique(ssids.begin(), ssids.end()), ssids.end());
    return ssids;
}

void AbnetBackend::configureWifi(const string &ssid, const string &password, const string &driverMode) {
    run_(string(Abnet) + " configure " + quoted(ssid) + " " + quoted(password));
    run_(string(Abnet) + " driver_mode " + driverMode);
}

void AbnetBackend::restartNetwork() {
    run_(string(Abnet) + " restart");
}

bool AbnetBackend::btUp() {
    return run_(string(Abnet) + " bt_up ") == "yes";
}

string AbnetBackend::btName() {
    return run_(string(Abnet) + " bt_name ");
}

string AbnetBackend::timezone() {
    return run_(string(Settime) + " tz");
}

void AbnetBackend::setTimezone(const string &zone) {
    run_(string(Settime) + " tzone " + quoted(zone));
}

vector<string> AbnetBackend::listTimezones() {
    vector<string> zones = runLines_("timedatectl list-timezones");
    sort(zones.begin(), zones.end());
    zones.erase(unique(zones.begin(), zones.end()), zones.end());
    return zones;
}

//*******************************
// FakeBackend
//*******************************
void FakeBackend::configureWifi(const string &ssid, const string &password, const string &driverMode) {
    PLOG_INFO << "fake abnet configure \"" << ssid << "\" (password of " << password.size() << " chars), driver "
              << driverMode;
    configuredSsid = ssid;
    configuredPassword = password;
    configuredDriverMode = driverMode;
}

void FakeBackend::restartNetwork() {
    PLOG_INFO << "fake abnet restart";
    restarts++;
}

void FakeBackend::setTimezone(const string &zone) {
    PLOG_INFO << "fake settime tzone " << zone;
    timezone_ = zone;
}

vector<string> FakeBackend::listTimezones() {
    return {"Africa/Cairo",      "America/Chicago", "America/Los_Angeles", "America/New_York",
            "America/Sao_Paulo", "Asia/Tokyo",      "Asia/Shanghai",       "Australia/Sydney",
            "Europe/Berlin",     "Europe/London",   "Europe/Madrid",       "Europe/Paris",
            "Europe/Rome",       "Europe/Warsaw",   "Pacific/Auckland",    "UTC"};
}
