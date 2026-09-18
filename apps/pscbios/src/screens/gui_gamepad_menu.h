//
// GuiGamepadMenu: the gamepad section - the mapping wizard, and the two pages that explain DualShock 3
// and Bluetooth pairing; below the menu, which mapping file is in use and every joystick with whether
// SDL has a mapping for it.
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
    void render() override;
    std::string getTitle() override { return "-=" + _("Gamepad Support Configuration") + "=-"; }
    std::string getStatusLine() override { return "|@X|" + _("Select") + "   |@O|" + _("Cancel"); }

    void doCircle_Pressed() override;
    void doCross_Pressed() override;

private:
    enum Row { Mapping = 0, DualShock, BluetoothPairing };
    void openWizard();
};
