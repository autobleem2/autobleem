//
// GuiGamepadMenu: the gamepad section.
//
#include "gui_gamepad_menu.h"
#include "gui_pad_config.h"
#include "pscbios_pages.h"
#include "gui/gui.h"
#include "gui/screens/gui_text_page.h"

#include <ableem/ui/joystick.h>

using namespace std;

//*******************************
// GuiGamepadMenu::init
//*******************************
void GuiGamepadMenu::init() {
    GuiMenuBase::init();
    lines = {_("Setup/Test Game Controller or update mapping"), _("Connect DualShock3 by Bluetooth (Wireless mode)"),
             _("Connect other gamepad by Bluetooth")};
}

//*******************************
// GuiGamepadMenu::render
//*******************************
void GuiGamepadMenu::render() {
    renderer.clear();
    gui->renderBackground();
    gui->renderTextBar();
    yoffset = gui->renderHeader(getTitle());
    TextRenderer &text = gui->text();
    if (firstRender) {
        computePagePosition();
        firstRender = false;
    }
    renderLines();
    renderSelectionBox();

    const int joysticks = ableem::Joystick::count();
    string mappingFile = gui->input().currentMappingPath();
    text.renderTextLine(_("Game controller DB:") + " " + (mappingFile.empty() ? _("SDL's built-in") : mappingFile), 6,
                        yoffset);
    text.renderTextLine(_("Game controller information:"), 7, yoffset);
    text.renderTextLine("   " + _("Game Controllers number:") + " " + to_string(gui->input().activePadCount()) + "/" +
                            to_string(joysticks),
                        8, yoffset);
    for (int i = 0; i < joysticks; i++) {
        string name = ableem::Joystick::nameForIndex(i);
        if (ableem::Joystick::isGameControllerAtIndex(i))
            name += _(" - Mapping (Available):") + ableem::Joystick::controllerNameForIndex(i);
        else
            name += _(" - Mapping (Not found)");
        text.renderTextLine("     #" + to_string(i) + " " + name, 9 + i, yoffset);
    }

    gui->renderStatus(getStatusLine());
    renderer.present();
}

//*******************************
// GuiGamepadMenu::doCircle_Pressed / doCross_Pressed
//*******************************
void GuiGamepadMenu::doCircle_Pressed() {
    app.audio().cancel.play();
    menuVisible = false;
}

void GuiGamepadMenu::doCross_Pressed() {
    app.audio().cursor.play();
    switch (selected) {
    case Mapping:
        openWizard();
        break;
    case DualShock: {
        GuiTextPage page(*gui);
        page.title = _("DualShock3/SixAxis Wireless Pairing");
        page.lines = dualshock3PairingLines();
        page.show();
        break;
    }
    case BluetoothPairing: {
        GuiTextPage page(*gui);
        page.title = _("Bluetooth controller pairing");
        page.lines = bluetoothPairingLines();
        page.show();
        break;
    }
    default:
        break;
    }
}

//*******************************
// GuiGamepadMenu::openWizard
//*******************************
// the wizard reads the pads raw, so Input lets go of them first and takes them back (with whatever mapping
// the wizard added) after
void GuiGamepadMenu::openWizard() {
    gui->input().flushPads();
    {
        GuiPadConfig wizard(*gui);
        wizard.show();
    }
    gui->input().probePads();
    gui->input().flushEvents();
}
