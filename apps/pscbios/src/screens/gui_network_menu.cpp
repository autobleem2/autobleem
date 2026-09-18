//
// GuiNetworkMenu: the WiFi settings.
//
#include "gui_network_menu.h"
#include "gui_ssid_scan_menu.h" // and GuiTimezoneSelect
#include "../pscbios_app.h"
#include "gui/gui.h"
#include "gui/screens/gui_confirm.h"
#include "gui/screens/gui_keyboard.h"

using namespace std;

//*******************************
// GuiNetworkMenu::init
//*******************************
void GuiNetworkMenu::init() {
    GuiMenuBase::init();
    // ssid.cfg if there is one, else the SSID an earlier setup left in wpa_supplicant.conf
    if (!config.load(SsidConfig::defaultPath()))
        config.ssid = SsidConfig::ssidFromWpaSupplicant(SsidConfig::wpaSupplicantPath());
    refresh();
    fill();
}

//*******************************
// GuiNetworkMenu::refresh / fill
//*******************************
void GuiNetworkMenu::refresh() {
    lastRefresh = gui->platform().ticks();
    ConsoleBackend &console = PscBios::get().console();
    ipAddress = NetworkStatus::addressText(console.interfaceFound("wlan0"), console.ipOf("wlan0"));
    timezone = console.timezone();
}

void GuiNetworkMenu::fill() {
    lines.clear();
    lines.emplace_back(_("Wi-Fi Connection:"));
    lines.emplace_back(_("SSID:") + "  " + config.ssid);
    lines.emplace_back(_("Password:") + "  " +
                       (displayAsterisksInsteadOfPassword ? string(config.password.size(), '*') : config.password));
    lines.emplace_back(_("Driver mode:") + "  " + config.driverMode);
    lines.emplace_back("");
    lines.emplace_back(_("Write Configuration/Restart Network"));
    lines.emplace_back("");
    lines.emplace_back(_("Restart Network"));
    lines.emplace_back(_("IP Address:") + " " + ipAddress);
    lines.emplace_back(_("Timezone:") + " " + timezone);
}

//*******************************
// GuiNetworkMenu::render
//*******************************
void GuiNetworkMenu::render() {
    if (gui->platform().ticks() - lastRefresh >= RefreshInterval)
        refresh();
    fill();
    GuiStringMenu::render();
}

//*******************************
// GuiNetworkMenu::getStatusLine
//*******************************
string GuiNetworkMenu::getStatusLine() {
    switch (selected) {
    case Ssid:
        return "   |@X| " + _("Edit SSID") + "   |@T| " + _("Scan SSID") + "   |@O| " + _("Cancel") + " |";
    case Password:
        return "   |@X| " + _("Edit Password") + "   |@O| " + _("Cancel") + " |";
    case WriteFile:
        return "   |@X| " + _("Write Config/Restart Network") + "   |@O| " + _("Cancel") + " |";
    case InitNetwork:
        return "   |@X| " + _("Restart Network") + "   |@O| " + _("Cancel") + " |";
    case DriverMode:
    case TimeZone:
        return "   |@X| " + _("Change") + "   |@O| " + _("Cancel") + " |";
    default:
        return "   |@O| " + _("Cancel") + " |";
    }
}

//*******************************
// GuiNetworkMenu::doCircle_Pressed / doTriangle_Pressed
//*******************************
void GuiNetworkMenu::doCircle_Pressed() {
    app.audio().cancel.play();
    menuVisible = false;
}

void GuiNetworkMenu::doTriangle_Pressed() {
    if (selected == Ssid) {
        app.audio().cursor.play();
        scanSsid();
    }
}

//*******************************
// GuiNetworkMenu::doCross_Pressed
//*******************************
void GuiNetworkMenu::doCross_Pressed() {
    app.audio().cursor.play();
    switch (selected) {
    case Ssid:
        editSsid();
        break;
    case Password:
        editPassword();
        break;
    case DriverMode:
        config.driverMode = config.driverMode == "wext" ? "nl80211" : "wext";
        break;
    case WriteFile: {
        writeConfig();
        GuiConfirm confirm(*gui);
        confirm.label = _("Restart Networking Now?");
        confirm.show();
        if (confirm.result)
            restartNetwork();
        break;
    }
    case InitNetwork:
        restartNetwork();
        break;
    case TimeZone:
        pickTimezone();
        break;
    default:
        break;
    }
    fill();
    render();
}

//*******************************
// the actions
//*******************************
void GuiNetworkMenu::editSsid() {
    GuiKeyboard keyboard(*gui);
    keyboard.label = _("Enter SSID");
    keyboard.result = config.ssid;
    keyboard.show();
    if (!keyboard.cancelled)
        config.ssid = keyboard.result;
}

void GuiNetworkMenu::editPassword() {
    GuiKeyboard keyboard(*gui);
    keyboard.label = _("Enter Password");
    keyboard.result = config.password;
    keyboard.displayAsterisksInstead = displayAsterisksInsteadOfPassword;
    keyboard.show();
    if (!keyboard.cancelled)
        config.password = keyboard.result;
}

void GuiNetworkMenu::scanSsid() {
    GuiSsidScanMenu scan(*gui);
    scan.show();
    if (!scan.cancelled)
        config.ssid = scan.newSsid;
    fill();
}

void GuiNetworkMenu::writeConfig() {
    if (!config.save(SsidConfig::defaultPath()))
        return; // an empty SSID or password: nothing to hand the kernel
    PscBios::get().console().configureWifi(config.ssid, config.password, config.driverMode);
}

// what the tool has always shown around a restart: the two splashes are the only feedback abnet gives
void GuiNetworkMenu::restartNetwork() {
    Gui::splash(_("Reinitializing Network"));
    gui->platform().delay(2000);
    PscBios::get().console().restartNetwork();
    Gui::splash(_("Restarted Wi-Fi  With SSID:") + " " + config.ssid);
    gui->platform().delay(2000);
    refresh();
}

void GuiNetworkMenu::pickTimezone() {
    GuiTimezoneSelect zones(*gui);
    zones.show();
    if (!zones.cancelled) {
        PscBios::get().console().setTimezone(zones.newTimezone);
        refresh();
    }
}
