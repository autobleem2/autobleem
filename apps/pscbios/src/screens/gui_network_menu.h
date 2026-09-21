//
// GuiNetworkMenu: the WiFi settings - the SSID (typed, or picked from a scan), the password, the driver
// mode, writing them for the kernel and restarting the network, the address it got, and the timezone.
//
#pragma once

#include "gui/menus/gui_string_menu.h"
#include "core/network_status.h"
#include "core/ssid_config.h"

#include <string>

//********************
// GuiNetworkMenu
//********************
class GuiNetworkMenu : public GuiStringMenu {
public:
    explicit GuiNetworkMenu(ableem::GuiBase &_gui) : GuiStringMenu(_gui) {}

    void init() override;
    void render() override;
    std::string getTitle() override { return _("Edit Network WPA WiFi Credentials"); }
    std::string getStatusLine() override;
    bool skipSelectingThisLineWhenMovingByOne(int index) override { return lines[index].empty(); }

    void doCircle_Pressed() override;
    void doCross_Pressed() override;
    void doTriangle_Pressed() override;

    bool displayAsterisksInsteadOfPassword = false;

    static const unsigned int RefreshInterval = 2000; // ms between re-reads of the address and timezone

private:
    enum Row { Text = 0, Ssid, Password, DriverMode, Blank1, WriteFile, Blank2, InitNetwork, IpAddress, TimeZone };
    SsidConfig config;
    std::string ipAddress, timezone;
    unsigned int lastRefresh = 0;

    void refresh(); // the address and the timezone from the console
    void fill();    // the rows from the current values
    void writeConfig();
    void restartNetwork();
    void editSsid();
    void editPassword();
    void scanSsid();
    void pickTimezone();
};
