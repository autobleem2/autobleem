//
// GuiGamepadMenu: the gamepad section - the mapping wizard, and the two pages that explain DualShock 3
// and Bluetooth pairing. A compact three-row list; the controllers and the mapping file in use are on
// the opening screen.
//
#pragma once

#include "gui/menus/gui_string_menu.h"

//********************
// GuiGamepadMenu
//********************
class GuiGamepadMenu : public GuiStringMenu {
public:
    explicit GuiGamepadMenu(ableem::GuiBase &_gui) : GuiStringMenu(_gui) {}

    void init() override;
    std::string getTitle() override { return _("Gamepad Support Configuration"); }
    std::string getStatusLine() override { return "|@X| " + _("Select") + "   |@O| " + _("Back"); }

    void doCircle_Pressed() override;
    void doCross_Pressed() override;

private:
    enum Row { Mapping = 0, DualShock, BluetoothPairing };
    void openWizard();
};
