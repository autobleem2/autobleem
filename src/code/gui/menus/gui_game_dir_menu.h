#pragma once

#include "gui_string_menu.h"
#include <vector>
#include <string>

//*******************************
// class GuiGameDirMenu
//*******************************
class GuiGameDirMenu : public GuiStringMenu {
public:
    explicit GuiGameDirMenu(ableem::GuiBase &_gui) : GuiStringMenu(_gui) {}

    std::string getTitle() override { return "-=" + _("Select PS1 Game Category") + "=-"; }
    std::string getStatusLine() override { return GuiStringMenu::getStatusLine(); }

    void doEnter() override { doCross_Pressed(); }
    void doEscape() override { doCircle_Pressed(); }
};
