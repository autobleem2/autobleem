//
// pscbios_core: the console backend's command lines and parsing, ssid.cfg, gamecontrollerdb.txt editing
// and the mapping wizard's logic - everything PSC-Bios does that is not a screen.
//
#include "doctest/doctest.h"

#include "../support/env_fixture.h"
#include "../support/temp_dir.h"

#include "core/console_backend.h"
#include "core/game_controller_db.h"
#include "core/network_status.h"
#include "core/pad_mapping.h"
#include "core/ssid_config.h"

#include <map>
#include <string>
#include <vector>

using std::string;
using std::vector;

//*******************************
// a scripted shell for AbnetBackend
//*******************************
namespace {
std::map<string, string> answers;          // command -> one-line answer
std::map<string, vector<string>> listings; // command -> lines
vector<string> ran;                        // every command, in order

string scriptedRun(const string &cmd) {
    ran.push_back(cmd);
    auto it = answers.find(cmd);
    return it == answers.end() ? "" : it->second;
}
vector<string> scriptedRunLines(const string &cmd) {
    ran.push_back(cmd);
    auto it = listings.find(cmd);
    return it == listings.end() ? vector<string>() : it->second;
}
struct ScriptedShell {
    ScriptedShell() {
        answers.clear();
        listings.clear();
        ran.clear();
    }
};
} // namespace

TEST_CASE("AbnetBackend asks the kernel's scripts the way the 2020 tool did") {
    ScriptedShell shell;
    answers["/bin/abnet list_ifaces"] = "lo eth0 wlan0";
    answers["/bin/abnet wlan_on"] = "yes";
    answers["/bin/abnet is_up wlan0"] = "yes";
    answers["/bin/abnet is_up eth0"] = "no";
    answers["/bin/abnet show_ip wlan0"] = "192.168.68.144";
    answers["/bin/abnet bt_up "] = "yes";
    answers["/bin/abnet bt_name "] = "hci0 00:11:22:33:44:55";
    answers["/bin/settime tz"] = "Europe/Warsaw";
    listings["/bin/abnet scan"] = {"Zeta", "Alpha", "Zeta", "Mid"};
    listings["timedatectl list-timezones"] = {"UTC", "Europe/Warsaw", "Africa/Cairo", "UTC"};
    AbnetBackend backend(scriptedRun, scriptedRunLines, true);

    CHECK(backend.kernelInstalled());
    CHECK(backend.interfaceFound("wlan0"));
    CHECK(backend.interfaceFound("eth0"));
    CHECK_FALSE(backend.interfaceFound("usb0"));
    CHECK(backend.wlanOn());
    CHECK(backend.isUp("wlan0"));
    CHECK_FALSE(backend.isUp("eth0"));
    CHECK(backend.ipOf("wlan0") == "192.168.68.144");
    CHECK(backend.ipOf("usb0") == ""); // no such interface: show_ip is never asked
    CHECK(backend.btUp());
    CHECK(backend.btName() == "hci0 00:11:22:33:44:55");
    CHECK(backend.timezone() == "Europe/Warsaw");
    CHECK(backend.scanSsids() == vector<string>{"Alpha", "Mid", "Zeta"}); // sorted, unique
    CHECK(backend.listTimezones() == vector<string>{"Africa/Cairo", "Europe/Warsaw", "UTC"});

    ran.clear();
    backend.configureWifi("My \"Net\"", "pa$s", "nl80211");
    REQUIRE(ran.size() == 2);
    CHECK(ran[0] == "/bin/abnet configure \"My \\\"Net\\\"\" \"pa\\$s\"");
    CHECK(ran[1] == "/bin/abnet driver_mode nl80211");
    ran.clear();
    backend.setTimezone("Europe/Warsaw");
    CHECK(ran == vector<string>{"/bin/settime tzone \"Europe/Warsaw\""});
    ran.clear();
    backend.restartNetwork();
    CHECK(ran == vector<string>{"/bin/abnet restart"});
}

TEST_CASE("AbnetBackend does not scan without a WiFi interface that is up") {
    ScriptedShell shell;
    answers["/bin/abnet list_ifaces"] = "lo eth0";
    listings["/bin/abnet scan"] = {"Alpha"};
    AbnetBackend backend(scriptedRun, scriptedRunLines, true);
    CHECK(backend.scanSsids().empty());
    for (const string &cmd : ran)
        CHECK(cmd != "/bin/abnet scan");
}

TEST_CASE("AbnetBackend drives the kernel's bt_* subcommands and parses the device lines") {
    ScriptedShell shell;
    answers["/bin/abnet bt_up "] = "yes";
    listings["/bin/abnet bt_scan "] = {"00:1B:DC:0F:11:22 Wireless Controller", "E4:17:D8:AA:BB:CC 8BitDo Pro 2",
                                       "not a device line", "AA:BB:CC:DD:EE:FF"};
    listings["/bin/abnet bt_paired "] = {"A0:AB:51:33:44:55 Xbox Wireless Controller"};
    answers["/bin/abnet bt_pair \"00:1B:DC:0F:11:22\""] = "ok";
    answers["/bin/abnet bt_remove \"A0:AB:51:33:44:55\""] = "ok";
    AbnetBackend backend(scriptedRun, scriptedRunLines, true);

    auto scanned = backend.btScan();
    REQUIRE(scanned.size() == 3); // the line without a mac is dropped
    CHECK(scanned[0].mac == "00:1B:DC:0F:11:22");
    CHECK(scanned[0].name == "Wireless Controller");
    CHECK_FALSE(scanned[0].paired);
    CHECK(scanned[2].mac == "AA:BB:CC:DD:EE:FF");
    CHECK(scanned[2].name == "AA:BB:CC:DD:EE:FF"); // a mac with no name keeps the mac as its name

    auto paired = backend.btPairedDevices();
    REQUIRE(paired.size() == 1);
    CHECK(paired[0].paired);
    CHECK(paired[0].name == "Xbox Wireless Controller");

    CHECK(backend.btPair("00:1B:DC:0F:11:22"));
    CHECK(backend.btRemove("A0:AB:51:33:44:55"));
    CHECK_FALSE(backend.btPair("de:ad:be:ef:00:00")); // no scripted "ok" -> failure
}

TEST_CASE("AbnetBackend does not touch Bluetooth without an adapter") {
    ScriptedShell shell;
    answers["/bin/abnet bt_up "] = "no";
    listings["/bin/abnet bt_scan "] = {"00:1B:DC:0F:11:22 Something"};
    AbnetBackend backend(scriptedRun, scriptedRunLines, true);
    CHECK(backend.btScan().empty());
    CHECK(backend.btPairedDevices().empty());
    for (const string &cmd : ran)
        CHECK(cmd != "/bin/abnet bt_scan ");
}

TEST_CASE("FakeBackend pairs and removes Bluetooth controllers, and can report no adapter") {
    FakeBackend fake;
    CHECK(fake.btUp());
    CHECK(fake.btScan().size() == 3);
    REQUIRE(fake.btPairedDevices().size() == 1); // the Xbox pad is already paired
    CHECK(fake.btPairedDevices()[0].name == "Xbox Wireless Controller");

    CHECK(fake.btPair("00:1B:DC:0F:11:22")); // pair a DS4
    CHECK(fake.btPairedDevices().size() == 2);
    CHECK(fake.btRemove("A0:AB:51:33:44:55")); // forget the Xbox pad
    CHECK(fake.btPairedDevices().size() == 1);
    CHECK_FALSE(fake.btPair("no:such:mac"));

    fake.btAdapter_ = false;
    CHECK_FALSE(fake.btUp());
    CHECK(fake.btName().empty());
    CHECK(fake.btScan().size() == 3); // the fake still lists its devices; the screen gates on btUp()
}

TEST_CASE("NetworkStatus gathers the main screen's facts from the backend") {
    FakeBackend fake;
    NetworkStatus status;
    status.refresh(fake);
    CHECK(status.wirelessFound);
    CHECK(status.wirelessActive);
    CHECK(status.wirelessAddr == "192.168.1.23");
    CHECK_FALSE(status.ethFound);
    CHECK(status.btActive);
    CHECK(status.timezone == "Europe/Warsaw");
    CHECK(NetworkStatus::dongleStatus(true, false) == "Found/Not active");
    CHECK(NetworkStatus::addressText(false, "") == "-");
    CHECK(NetworkStatus::addressText(true, "") == "Waiting for IP Address...");
    CHECK(NetworkStatus::addressText(true, "10.0.0.2") == "10.0.0.2");

    fake.setTimezone("UTC");
    fake.configureWifi("Home Network", "secret", "wext");
    CHECK(fake.configuredSsid == "Home Network");
    CHECK(fake.timezone() == "UTC");
}

TEST_CASE("SsidConfig reads and writes the three-line ssid.cfg") {
    TempDir tmp("ssid");
    SsidConfig cfg;
    CHECK_FALSE(cfg.load(tmp.at("ssid.cfg")));
    CHECK(cfg.driverMode == "wext");

    tmp.writeFile("ssid.cfg", "Home\r\nsecret\r\n\r\n"); // CRLF and an empty driver line survive
    REQUIRE(cfg.load(tmp.at("ssid.cfg")));
    CHECK(cfg.ssid == "Home");
    CHECK(cfg.password == "secret");
    CHECK(cfg.driverMode == "wext");

    cfg.driverMode = "nl80211";
    REQUIRE(cfg.save(tmp.at("out.cfg")));
    CHECK(tmp.readFile("out.cfg") == "Home\nsecret\nnl80211\n");

    cfg.password.clear();
    CHECK_FALSE(cfg.save(tmp.at("out2.cfg"))); // no password: nothing written
}

TEST_CASE("SsidConfig falls back to the SSID in wpa_supplicant.conf, except the old tool's \"1\"") {
    TempDir tmp("wpa");
    tmp.writeFile("wpa.conf", "network={\n    ssid=\"Cafe\"\n    psk=\"x\"\n}\n");
    CHECK(SsidConfig::ssidFromWpaSupplicant(tmp.at("wpa.conf")) == "Cafe");
    tmp.writeFile("wpa1.conf", "network={\n ssid=\"1\"\n}\n");
    CHECK(SsidConfig::ssidFromWpaSupplicant(tmp.at("wpa1.conf")) == "");
    CHECK(SsidConfig::ssidFromWpaSupplicant(tmp.at("missing.conf")) == "");
}

TEST_CASE("SsidConfig's paths follow the kernel config dir, and the app dir without one") {
    EnvFixture env;
    env.setAppDir("/apps/pscbios");
    ableem::Environment::setKernelConfigDir("");
    CHECK(SsidConfig::defaultPath() == "/apps/pscbios/ssid.cfg");
    CHECK(SsidConfig::wpaSupplicantPath() == "/apps/pscbios/wpa_supplicant.conf");
    ableem::Environment::setKernelConfigDir("/etc/autobleem");
    CHECK(SsidConfig::defaultPath() == "/etc/autobleem/ssid.cfg");
    CHECK(SsidConfig::wpaSupplicantPath() == "/etc/wpa_supplicant.conf");
}

TEST_CASE("GameControllerDb replaces a mapping by (platform, guid) and appends a new one under the marker") {
    TempDir tmp("gcdb");
    tmp.writeFile("db.txt", "# Game Controller DB\n"
                            "\n"
                            "0001,Pad One,a:b0,platform:Linux,\n"
                            "0001,Pad One,a:b1,platform:Windows,\n"
                            "#AutoBleem\n"
                            "0002,Pad Two,a:b2,platform:Linux,\n");
    GameControllerDb db;
    db.load(tmp.at("db.txt"));
    REQUIRE(db.lines().size() == 5); // the blank line is dropped
    CHECK(db.lines()[0].comment);
    CHECK(db.lines()[1].guid == "0001");
    CHECK(db.lines()[1].platform == "Linux");
    CHECK(db.lines()[2].platform == "Windows");

    db.replaceMapping("0001", "Linux", "0001,Pad One Fixed,a:b9,platform:Linux,");
    db.replaceMapping("0003", "Linux", "0003,Pad Three,a:b3,platform:Linux,");
    REQUIRE(db.save(tmp.at("db.txt")));
    CHECK(tmp.readFile("db.txt") == "# Game Controller DB\n"
                                    "0001,Pad One Fixed,a:b9,platform:Linux,\n"
                                    "0001,Pad One,a:b1,platform:Windows,\n"
                                    "#AutoBleem\n"
                                    "0002,Pad Two,a:b2,platform:Linux,\n"
                                    "0003,Pad Three,a:b3,platform:Linux,\n");
}

TEST_CASE("GameControllerDb adds the marker to a file without one, and starts empty without a file") {
    TempDir tmp("gcdb2");
    tmp.writeFile("plain.txt", "0001,Pad,a:b0,platform:Linux,\n");
    GameControllerDb db;
    db.load(tmp.at("plain.txt"));
    REQUIRE(db.lines().size() == 2);
    CHECK(db.lines()[1].text == "#AutoBleem");

    GameControllerDb fresh;
    fresh.load(tmp.at("absent.txt"));
    REQUIRE(fresh.lines().size() == 1);
    CHECK(fresh.lines()[0].text == "#AutoBleem");
    CHECK(GameControllerDb::platformOf("x,y,a:b0,platform:Mac OS X") == "Mac OS X");
    CHECK(GameControllerDb::platformOf("x,y,a:b0") == "");
    CHECK(GameControllerDb::guidOf("abcd,y,a:b0") == "abcd");
}

//*******************************
// PadMapping
//*******************************
namespace {
ableem::JoystickState rest(int axes, int buttons, int hats) {
    ableem::JoystickState s;
    s.axes.assign(axes, 0);
    s.buttons.assign(buttons, false);
    s.hats.assign(hats, ableem::Joystick::HatCentered);
    return s;
}
} // namespace

TEST_CASE("PadMapping asks for the 25 standard inputs in the wizard's order") {
    vector<PadMapping::Element> elements = PadMapping::standardElements();
    REQUIRE(elements.size() == 25);
    CHECK(elements[0].apiName == "a");
    CHECK(elements[0].button == 0);
    CHECK(elements[11].apiName == "lefttrigger");
    CHECK(elements[11].scan == PadMapping::Scan::Trigger);
    CHECK(elements[11].axis == 4);
    CHECK(elements[17].apiName == "-leftx");
    CHECK(elements[17].negative);
    CHECK(elements[24].apiName == "+righty");
    for (const PadMapping::Element &e : elements)
        CHECK(e.value.empty());
}

TEST_CASE("PadMapping::detectChange names the raw input that moved, a button before a hat before an axis") {
    ableem::JoystickState initial = rest(6, 12, 1);
    ableem::JoystickState now = initial;
    vector<PadMapping::Element> taken;
    CHECK(PadMapping::detectChange(initial, now, taken) == "");

    now.buttons[3] = true;
    CHECK(PadMapping::detectChange(initial, now, taken) == "b3");

    now = initial;
    now.hats[0] = ableem::Joystick::HatLeft;
    CHECK(PadMapping::detectChange(initial, now, taken) == "h0.8");
    now.hats[0] = ableem::Joystick::HatUp | ableem::Joystick::HatLeft; // a diagonal is not a mapping
    CHECK(PadMapping::detectChange(initial, now, taken) == "");

    now = initial;
    now.axes[1] = -32700; // a stick pushed up from the middle
    CHECK(PadMapping::detectChange(initial, now, taken) == "-a1");
    now.axes[1] = 32700;
    CHECK(PadMapping::detectChange(initial, now, taken) == "+a1");
    now.axes[1] = 20000; // not far enough
    CHECK(PadMapping::detectChange(initial, now, taken) == "");

    initial.axes[4] = -32768; // a trigger rests at one end
    now = initial;
    now.axes[4] = 32767;
    CHECK(PadMapping::detectChange(initial, now, taken) == "a4");

    // an input already given to an element is not offered again
    PadMapping::Element used;
    used.value = "a4";
    taken.push_back(used);
    CHECK(PadMapping::detectChange(initial, now, taken) == "");
}

TEST_CASE("PadMapping::anythingHeld is what the wizard waits to clear") {
    ableem::JoystickState initial = rest(2, 2, 1);
    ableem::JoystickState now = initial;
    CHECK_FALSE(PadMapping::anythingHeld(initial, now));
    now.axes[0] = 25000;
    CHECK(PadMapping::anythingHeld(initial, now));
    now.axes[0] = 10000;
    CHECK_FALSE(PadMapping::anythingHeld(initial, now));
    now.buttons[1] = true;
    CHECK(PadMapping::anythingHeld(initial, now));
}

TEST_CASE("PadMapping::finalElements merges stick halves, drops the unmapped and adds the platform") {
    vector<PadMapping::Element> scanned = PadMapping::standardElements();
    auto set = [&](const char *name, const char *value) {
        for (PadMapping::Element &e : scanned)
            if (e.apiName == name)
                e.value = value;
    };
    set("a", "b0");
    set("b", "b1");
    set("dpup", "h0.1");
    set("lefttrigger", "a2");
    set("-leftx", "-a0");
    set("+leftx", "+a0"); // a normal stick axis
    set("-lefty", "+a1");
    set("+lefty", "-a1"); // an inverted one
    set("-rightx", "-a3");
    set("+rightx", "b7");  // halves that cannot be merged stay as half keys
    set("-righty", "-a4"); // and a half alone goes

    vector<PadMapping::Element> finals = PadMapping::finalElements(scanned, "Linux");
    string line = PadMapping::mappingLine("0300abcd", "My Pad", finals);
    CHECK(line == "0300abcd,My Pad,a:b0,b:b1,lefttrigger:a2,dpup:h0.1,-rightx:-a3,+rightx:b7,leftx:a0,lefty:a1~,"
                  "platform:Linux,");
}

TEST_CASE("PadMapping::cleanName keeps what SDL accepts in a mapping line") {
    CHECK(PadMapping::cleanName("Sony PLAYSTATION(R)3 Controller, v2") == "Sony PLAYSTATIONR3 Controller v2");
}
