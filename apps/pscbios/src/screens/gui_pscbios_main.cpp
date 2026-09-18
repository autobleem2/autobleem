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

#include <ableem/ui/joystick.h>

#include <ctime>

using namespace std;

//*******************************
// GuiPscBiosMain::init
//*******************************
void GuiPscBiosMain::init() {
    kernel = PscBios::get().console().kernelInstalled();
    refresh();
}

//*******************************
// GuiPscBiosMain::refresh
//*******************************
void GuiPscBiosMain::refresh() {
    lastRefresh = gui->platform().ticks();
    if (kernel)
        status.refresh(PscBios::get().console());
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
// GuiPscBiosMain::render
//*******************************
void GuiPscBiosMain::render() {
    renderer.clear();
    gui->renderBackground();
    gui->renderTextBar();
    int offset = gui->renderLogo(true);
    TextRenderer &text = gui->text();
    text.renderTextLine("-=" + _("Playstation Classic Hardware Information") + "=-", 0, offset, XALIGN_CENTER);

    int controllerLine = 1;
    if (kernel) {
        controllerLine = 15;
        text.renderTextLine(_("Current Time:") + "  " + clockText() + "    (" + status.timezone + ")", 1, offset);
        text.renderTextLine(_("WiFi information:"), 3, offset);
        text.renderTextLine("   " + _("Dongle status:") + "  " +
                                NetworkStatus::dongleStatus(status.wirelessFound, status.wirelessActive),
                            4, offset);
        text.renderTextLine("   " + _("IP configuration:") + "  " +
                                NetworkStatus::addressText(status.wirelessFound, status.wirelessAddr),
                            5, offset);
        text.renderTextLine(_("Ethernet information:"), 7, offset);
        text.renderTextLine("   " + _("Dongle status:") + "  " +
                                NetworkStatus::dongleStatus(status.ethFound, status.ethActive),
                            8, offset);
        text.renderTextLine("   " + _("IP configuration:") + "  " +
                                NetworkStatus::addressText(status.ethFound, status.ethAddr),
                            9, offset);
        text.renderTextLine(_("Bluetooth information:"), 11, offset);
        text.renderTextLine("   " + _("Dongle status:") + "  " +
                                NetworkStatus::dongleStatus(status.btActive, status.btActive),
                            12, offset);
        text.renderTextLine("   " + _("Interface details:") + "  " + status.btName, 13, offset);
    }

    const int joysticks = ableem::Joystick::count();
    text.renderTextLine(_("Game Controller information:"), controllerLine, offset);
    text.renderTextLine("   " + _("Game Controllers number:") + " " + to_string(gui->input().activePadCount()) + "/" +
                            to_string(joysticks),
                        controllerLine + 1, offset);
    for (int i = 0; i < joysticks && i < 4; i++) {
        string name = ableem::Joystick::nameForIndex(i);
        if (ableem::Joystick::isGameControllerAtIndex(i))
            name += _(" - Mapping (Available):") + ableem::Joystick::controllerNameForIndex(i);
        else
            name += _(" - Mapping (Not found)");
        text.renderTextLine("     #" + to_string(i) + " " + name, controllerLine + 2 + i, offset);
    }
    if (joysticks > 4)
        text.renderTextLine("..", controllerLine + 6, offset);

    string menu;
    if (status.wirelessFound && kernel)
        menu += "|@Select| " + _("WiFi Settings") + "   ";
    menu += "|@S| " + _("Setup Gamepads") + "   ";
    menu += "|@T| " + _("About") + "   ";
    menu += "|@O| " + _("Cancel") + "   ";
    gui->renderStatus(menu);
    renderer.present();
}

//*******************************
// GuiPscBiosMain::open*
//*******************************
void GuiPscBiosMain::openNetworkMenu() {
    app.audio().cursor.play();
    GuiNetworkMenu menu(*gui);
    menu.show();
    refresh();
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
    about.show();
}

//*******************************
// GuiPscBiosMain::loop
//*******************************
void GuiPscBiosMain::loop() {
    menuVisible = true;
    while (menuVisible) {
        if (gui->platform().ticks() - lastRefresh >= RefreshInterval)
            refresh();
        render();

        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit)
                menuVisible = false;
            if (e.type != Event::Type::ButtonDown)
                continue;
            switch (e.button) {
            case Button::Circle:
                app.audio().cancel.play();
                menuVisible = false;
                break;
            case Button::Square:
                openGamepadMenu();
                break;
            case Button::Triangle:
                openAbout();
                break;
            case Button::Select:
                if (kernel)
                    openNetworkMenu();
                break;
            default:
                break;
            }
        }
    }
}
