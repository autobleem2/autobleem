//
// ConsoleBackend: everything PSC-Bios asks the console for - the network interfaces, WiFi, Bluetooth, the
// timezone - through the AutoBleem kernel's /bin/abnet and /bin/settime scripts. The interface is what the
// screens use; AbnetBackend is the console, FakeBackend a dev host with canned answers (the ProcessRunner
// pattern: main() picks one, nothing else knows the difference).
//
#pragma once

#include <string>
#include <vector>

//******************
// BtDevice
//******************
// a Bluetooth device the pairing screen lists: its address, its name, and whether it is already paired
// (and, if so, currently connected).
struct BtDevice {
    std::string mac;  // "AA:BB:CC:DD:EE:FF"
    std::string name; // "Wireless Controller" (may be the mac again when the name is unknown)
    bool paired = false;
    bool connected = false;
};

//******************
// ConsoleBackend
//******************
class ConsoleBackend {
public:
    virtual ~ConsoleBackend() = default;

    virtual bool kernelInstalled() = 0; // the AutoBleem kernel, which brings abnet/settime along

    // network
    virtual bool interfaceFound(const std::string &iface) = 0; // "eth0", "wlan0"
    virtual bool wlanOn() = 0;                                 // a WiFi dongle is there and up
    virtual bool isUp(const std::string &iface) = 0;
    virtual std::string ipOf(const std::string &iface) = 0; // "" while there is none
    virtual std::vector<std::string> scanSsids() = 0;       // sorted, unique; empty without WiFi
    virtual void configureWifi(const std::string &ssid, const std::string &password, const std::string &driverMode) = 0;
    virtual void restartNetwork() = 0;

    // bluetooth
    virtual bool btUp() = 0; // a Bluetooth adapter (usually a USB dongle) is present and up
    virtual std::string btName() = 0;

    // bluetooth controller pairing (needs btUp(); the kernel's abnet bt_* subcommands). DualShock 3 pairs
    // over USB through the sixaxis plugin and is not listed here - this is for DualShock 4 and other
    // standard Bluetooth gamepads.
    virtual std::vector<BtDevice> btScan() = 0;          // discover nearby devices (blocks a few seconds)
    virtual std::vector<BtDevice> btPairedDevices() = 0; // devices already paired/known
    virtual bool btPair(const std::string &mac) = 0;     // pair + trust + connect; true on success
    virtual bool btRemove(const std::string &mac) = 0;   // unpair/forget; true on success

    // time
    virtual std::string timezone() = 0;
    virtual void setTimezone(const std::string &zone) = 0;
    virtual std::vector<std::string> listTimezones() = 0; // sorted, unique
};

//******************
// AbnetBackend
//******************
// the console: each call is one popen of the kernel's scripts. `run(cmd)` and `runLines(cmd)` are what it
// shells through - System::execUnixCommand by default, replaced by the tests with a scripted stand-in.
class AbnetBackend : public ConsoleBackend {
public:
    AbnetBackend();
    using Runner = std::string (*)(const std::string &cmd);
    using LinesRunner = std::vector<std::string> (*)(const std::string &cmd);
    // btHelper is the path to the shipped `bt` bluetoothctl wrapper (defaults to the app dir's copy);
    // the tests pass a fixed name so the command strings are predictable.
    AbnetBackend(Runner run, LinesRunner runLines, bool kernel, std::string btHelper = "bt");

    bool kernelInstalled() override { return kernel_; }
    bool interfaceFound(const std::string &iface) override;
    bool wlanOn() override;
    bool isUp(const std::string &iface) override;
    std::string ipOf(const std::string &iface) override;
    std::vector<std::string> scanSsids() override;
    void configureWifi(const std::string &ssid, const std::string &password, const std::string &driverMode) override;
    void restartNetwork() override;
    bool btUp() override;
    std::string btName() override;
    std::vector<BtDevice> btScan() override;
    std::vector<BtDevice> btPairedDevices() override;
    bool btPair(const std::string &mac) override;
    bool btRemove(const std::string &mac) override;
    std::string timezone() override;
    void setTimezone(const std::string &zone) override;
    std::vector<std::string> listTimezones() override;

    static const char *const Abnet;   // "/bin/abnet"
    static const char *const Settime; // "/bin/settime"

private:
    Runner run_;
    LinesRunner runLines_;
    bool kernel_;
    std::string btHelper_; // the shipped `bt` bluetoothctl wrapper (app dir), for pairing on any kernel
};

//******************
// FakeBackend
//******************
// a dev host: a WiFi dongle on 192.168.1.23, no ethernet, a Bluetooth dongle, three networks to pick
// from, Europe/Warsaw and a short timezone list; configure/restart/setTimezone only log and update what
// the fake then reports (so the screens show their effect)
class FakeBackend : public ConsoleBackend {
public:
    bool kernelInstalled() override { return true; }
    bool interfaceFound(const std::string &iface) override { return iface == "wlan0"; }
    bool wlanOn() override { return true; }
    bool isUp(const std::string &iface) override { return iface == "wlan0"; }
    std::string ipOf(const std::string &iface) override { return iface == "wlan0" ? "192.168.1.23" : ""; }
    std::vector<std::string> scanSsids() override { return {"Cafe Corner", "Home Network", "Neighbour 5G"}; }
    void configureWifi(const std::string &ssid, const std::string &password, const std::string &driverMode) override;
    void restartNetwork() override;
    bool btUp() override { return btAdapter_; }
    std::string btName() override { return btAdapter_ ? "hci0 PSC-BT 00:11:22:33:44:55" : ""; }
    std::vector<BtDevice> btScan() override;
    std::vector<BtDevice> btPairedDevices() override;
    bool btPair(const std::string &mac) override;
    bool btRemove(const std::string &mac) override;
    std::string timezone() override { return timezone_; }
    void setTimezone(const std::string &zone) override;
    std::vector<std::string> listTimezones() override;

    std::string configuredSsid, configuredPassword, configuredDriverMode; // what configureWifi() was given
    int restarts = 0;
    bool btAdapter_ = true; // a test can clear this to exercise the "no adapter" path

private:
    std::string timezone_ = "Europe/Warsaw";
    // the fake's nearby devices; btScan reveals them, btPair/btRemove flip `paired`
    std::vector<BtDevice> btDevices_ = {
        {"00:1B:DC:0F:11:22", "Wireless Controller", false, false}, // a DS4 in pairing mode
        {"E4:17:D8:AA:BB:CC", "8BitDo Pro 2", false, false},
        {"A0:AB:51:33:44:55", "Xbox Wireless Controller", true, true} // already paired from a past session
    };
};
