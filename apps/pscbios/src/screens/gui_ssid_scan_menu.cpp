//
// GuiSsidScanMenu / GuiTimezoneSelect: the two pickers.
//
#include "gui_ssid_scan_menu.h"
#include "../pscbios_app.h"
#include "gui/gui.h"

using namespace std;

//*******************************
// GuiSsidScanMenu
//*******************************
void GuiSsidScanMenu::init() {
    GuiMenuBase::init();
    gui->drawText(_("Scanning networks"));
    lines = PscBios::get().console().scanSsids();
    if (lines.empty()) {
        gui->drawText(_("No networks found"));
        gui->platform().delay(2000);
    }
}

string GuiSsidScanMenu::getStatusLine() {
    string menu;
    if (!lines.empty())
        menu += "|@X| " + _("Select") + "   ";
    return menu + "|@O| " + _("Back");
}

void GuiSsidScanMenu::doCross_Pressed() {
    if (lines.empty())
        return;
    app.audio().cursor.play();
    newSsid = lines[selected];
    cancelled = false;
    menuVisible = false;
}

void GuiSsidScanMenu::doCircle_Pressed() {
    app.audio().cancel.play();
    cancelled = true;
    menuVisible = false;
}

//*******************************
// GuiTimezoneSelect
//*******************************
void GuiTimezoneSelect::init() {
    GuiMenuBase::init();
    gui->drawText(_("Loading timezones..."));
    lines = PscBios::get().console().listTimezones();
}

void GuiTimezoneSelect::doCross_Pressed() {
    if (lines.empty())
        return;
    app.audio().cursor.play();
    newTimezone = lines[selected];
    cancelled = false;
    menuVisible = false;
}

void GuiTimezoneSelect::doCircle_Pressed() {
    app.audio().cancel.play();
    cancelled = true;
    menuVisible = false;
}
