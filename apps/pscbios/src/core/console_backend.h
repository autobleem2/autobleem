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
    virtual bool btUp() = 0;
    virtual std::string btName() = 0;

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
    AbnetBackend(Runner run, LinesRunner runLines, bool kernel);

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
    std::string timezone() override;
    void setTimezone(const std::string &zone) override;
    std::vector<std::string> listTimezones() override;

    static const char *const Abnet;   // "/bin/abnet"
    static const char *const Settime; // "/bin/settime"

private:
    Runner run_;
    LinesRunner runLines_;
    bool kernel_;
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
    bool btUp() override { return true; }
    std::string btName() override { return "hci0 PSC-BT 00:11:22:33:44:55"; }
    std::string timezone() override { return timezone_; }
    void setTimezone(const std::string &zone) override;
    std::vector<std::string> listTimezones() override;

    std::string configuredSsid, configuredPassword, configuredDriverMode; // what configureWifi() was given
    int restarts = 0;

private:
    std::string timezone_ = "Europe/Warsaw";
};
