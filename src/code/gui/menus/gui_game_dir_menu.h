#pragma once

#include "gui_string_menu.h"
#include <vector>
#include <string>

//*******************************
// class GuiGameDirMenu
//*******************************
class GuiGameDirMenu : public GuiStringMenu {
public:
    GuiGameDirMenu(ableem::GuiBase &_gui) : GuiStringMenu(_gui) {}

    virtual std::string getTitle() override { return "-=" + _("Select PS1 Game Category") + "=-"; }
    virtual std::string getStatusLine() override { return GuiStringMenu::getStatusLine(); }

    void doEnter() { doCross_Pressed(); }
    void doEscape() { doCircle_Pressed(); }
};
