//
// GuiGamepadMenu: the gamepad section.
//
#include "gui_gamepad_menu.h"
#include "gui_pad_config.h"
#include "pscbios_pages.h"
#include "gui/gui.h"
#include "gui/screens/gui_text_page.h"

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
