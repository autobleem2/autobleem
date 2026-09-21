//
// GuiNetworkMenu: the WiFi settings - the SSID (typed, or picked from a scan), the password, the driver
// mode, the timezone, the address the console got, and the two actions: writing the settings for the
// kernel and restarting the network, or a restart alone. Option rows in the shared look: the label at
// the left, the value at the row's right edge; a compact panel, being seven rows.
//
#pragma once

#include "gui/menus/gui_string_menu.h"
#include "core/network_status.h"
#include "core/ssid_config.h"

#include <string>
#include <vector>

//********************
// GuiNetworkMenu
//********************
class GuiNetworkMenu : public GuiStringMenu {
public:
    explicit GuiNetworkMenu(ableem::GuiBase &_gui) : GuiStringMenu(_gui) {}

    void init() override;
    void render() override;
    void renderLineIndexOnRow(int index, int row) override;
    std::string getTitle() override { return _("Edit Network WPA WiFi Credentials"); }
    std::string getStatusLine() override;
    bool skipSelectingThisLineWhenMovingByOne(int index) override { return index == IpAddress; }

    void doCircle_Pressed() override;
    void doCross_Pressed() override;
    void doTriangle_Pressed() override;

    bool displayAsterisksInsteadOfPassword = false;

    static const unsigned int RefreshInterval = 2000; // ms between re-reads of the address and timezone

private:
    enum Row { Ssid = 0, Password, DriverMode, TimeZone, IpAddress, WriteFile, InitNetwork };
    SsidConfig config;
    std::string ipAddress, timezone;
    std::vector<std::string> values; // one per row of `lines`, "" for an action row
    unsigned int lastRefresh = 0;

    void refresh(); // the address and the timezone from the console
    void fill();    // the rows from the current values
    void writeConfig();
    void restartNetwork();
    void editSsid();
    void editPassword();
    void scanSsid();
    void pickTimezone();
    void showBusy(const std::string &message, int ms); // the spinner over this screen, for `ms`
};
