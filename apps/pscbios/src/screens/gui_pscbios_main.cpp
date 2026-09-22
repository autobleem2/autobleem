//
// GuiPscBiosMain: the hardware information screen.
//
#include "gui_pscbios_main.h"
#include "gui_gamepad_menu.h"
#include "gui_network_menu.h"
#include "pscbios_pages.h"
#include "../pscbios_app.h"
#include "gui/gui.h"
#include "gui/screens/gui_about.h"
#include "core/version.h"

#include <ableem/ui/joystick.h>

#include <ctime>

using namespace std;

//*******************************
// GuiPscBiosMain::init
//*******************************
void GuiPscBiosMain::init() {
    kernel = PscBios::get().console().kernelInstalled();
    GuiFactsPage::init();
}

//*******************************
// GuiPscBiosMain::clockText
//*******************************
string GuiPscBiosMain::clockText() {
    time_t t = time(nullptr);
    string clock = asctime(localtime(&t));
    while (!clock.empty() && (clock.back() == '\n' || clock.back() == '\r'))
        clock.pop_back();
    return clock;
}

//*******************************
// GuiPscBiosMain::collect
//*******************************
// the sections; the keys are the 2020 tool's (translated), a trailing colon dropped where the heading
// band or the value column makes it redundant
vector<InfoSection> GuiPscBiosMain::collect() {
    auto plain = [](string text) {
        while (!text.empty() && (text.back() == ':' || text.back() == ' '))
            text.pop_back();
        return text;
    };
    vector<InfoSection> sections;
    sections.push_back({plain(_("AutoBleem")), {{plain(_("Version")), Version::FULL_VERSION}}});
    if (kernel) {
        status.refresh(PscBios::get().console());
        sections.push_back(
            {_("System"), {{plain(_("Current Time:")), clockText()}, {plain(_("Timezone:")), status.timezone}}});
        sections.push_back(
            {plain(_("WiFi information:")),
             {{plain(_("Dongle status:")), NetworkStatus::dongleStatus(status.wirelessFound, status.wirelessActive)},
              {plain(_("IP configuration:")), NetworkStatus::addressText(status.wirelessFound, status.wirelessAddr)}}});
        sections.push_back(
            {plain(_("Ethernet information:")),
             {{plain(_("Dongle status:")), NetworkStatus::dongleStatus(status.ethFound, status.ethActive)},
              {plain(_("IP configuration:")), NetworkStatus::addressText(status.ethFound, status.ethAddr)}}});
        sections.push_back(
            {plain(_("Bluetooth information:")),
             {{plain(_("Dongle status:")), NetworkStatus::dongleStatus(status.btActive, status.btActive)},
              {plain(_("Interface details:")), status.btName}}});
    }

    const int joysticks = ableem::Joystick::count();
    InfoSection pads{plain(_("Game Controller information:")), {}};
    pads.rows.push_back(
        {plain(_("Game Controllers number:")), to_string(gui->input().activePadCount()) + "/" + to_string(joysticks)});
    string mappingFile = gui->input().currentMappingPath();
    pads.rows.push_back({plain(_("Game controller DB:")), mappingFile.empty() ? _("SDL's built-in") : mappingFile});
    for (int i = 0; i < joysticks; i++) {
        string name = ableem::Joystick::nameForIndex(i);
        if (ableem::Joystick::isGameControllerAtIndex(i))
            name += _(" - Mapping (Available):") + " " + ableem::Joystick::controllerNameForIndex(i);
        else
            name += _(" - Mapping (Not found)");
        pads.rows.push_back({_("Controller") + " " + to_string(i + 1), name});
    }
    sections.push_back(pads);
    return sections;
}

//*******************************
// GuiPscBiosMain::extraHints / onButton
//*******************************
string GuiPscBiosMain::extraHints() {
    string menu;
    if (status.wirelessFound && kernel)
        menu += "|@Select| " + _("WiFi Settings") + "   ";
    menu += "|@S| " + _("Setup Gamepads") + "   |@T| " + _("About");
    return menu;
}

bool GuiPscBiosMain::onButton(ableem::Button button) {
    switch (button) {
    case Button::Square:
        openGamepadMenu();
        return true;
    case Button::Triangle:
        openAbout();
        return true;
    case Button::Select:
        if (!kernel)
            return false;
        openNetworkMenu();
        return true;
    default:
        return false;
    }
}

//*******************************
// GuiPscBiosMain::open*
//*******************************
void GuiPscBiosMain::openNetworkMenu() {
    app.audio().cursor.play();
    GuiNetworkMenu menu(*gui);
    menu.show();
}

void GuiPscBiosMain::openGamepadMenu() {
    app.audio().cursor.play();
    GuiGamepadMenu menu(*gui);
    menu.show();
}

void GuiPscBiosMain::openAbout() {
    app.audio().cursor.play();
    GuiAbout about(*gui);
    about.credits = pscbiosCredits();
    about.foot = pscbiosFoot();
    about.show();
}
