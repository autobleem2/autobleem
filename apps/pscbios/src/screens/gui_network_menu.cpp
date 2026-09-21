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
    values.clear();
    auto row = [&](const string &label, const string &value) {
        lines.push_back(label);
        values.push_back(value);
    };
    row(_("SSID:"), config.ssid);
    row(_("Password:"), displayAsterisksInsteadOfPassword ? string(config.password.size(), '*') : config.password);
    row(_("Driver mode:"), config.driverMode);
    row(_("Timezone:"), timezone);
    row(_("IP Address:"), ipAddress);
    row(_("Write Configuration/Restart Network"), "");
    row(_("Restart Network"), "");
}

//*******************************
// GuiNetworkMenu::renderLineIndexOnRow
//*******************************
// the label at the left, the value at the row's right edge
void GuiNetworkMenu::renderLineIndexOnRow(int index, int row) {
    gui->text().renderTextLine(lines[index], row, yoffset, XALIGN_LEFT, 0, font);
    if (!values[index].empty())
        gui->text().renderRowValue(values[index], row, yoffset, 0, font);
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
        return "|@X| " + _("Edit SSID") + "   |@T| " + _("Scan SSID") + "   |@O| " + _("Back");
    case Password:
        return "|@X| " + _("Edit Password") + "   |@O| " + _("Back");
    case WriteFile:
        return "|@X| " + _("Write Config/Restart Network") + "   |@O| " + _("Back");
    case InitNetwork:
        return "|@X| " + _("Restart Network") + "   |@O| " + _("Back");
    case DriverMode:
    case TimeZone:
        return "|@X| " + _("Change") + "   |@O| " + _("Back");
    default:
        return "|@O| " + _("Back");
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

// the two messages are the only feedback abnet gives: the spinner over this screen, with each in turn
void GuiNetworkMenu::restartNetwork() {
    showBusy(_("Reinitializing Network"), 2000);
    PscBios::get().console().restartNetwork();
    showBusy(_("Restarted Wi-Fi  With SSID:") + " " + config.ssid, 2000);
    refresh();
}

void GuiNetworkMenu::showBusy(const string &message, int ms) {
    gui->beginBusy(message, [this]() { render(); });
    const unsigned int until = gui->platform().ticks() + ms;
    while (gui->platform().ticks() < until) {
        gui->busyTick();
        gui->platform().delay(20);
    }
    gui->endBusy();
}

void GuiNetworkMenu::pickTimezone() {
    GuiTimezoneSelect zones(*gui);
    zones.show();
    if (!zones.cancelled) {
        PscBios::get().console().setTimezone(zones.newTimezone);
        refresh();
    }
}
