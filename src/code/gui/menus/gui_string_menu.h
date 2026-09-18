#pragma once

#include "gui_menu_base.h"

//*******************************
// class GuiStringMenu
//*******************************
class GuiStringMenu : public GuiMenuBase<std::string> {
public:
    explicit GuiStringMenu(ableem::GuiBase &_gui) : GuiMenuBase(_gui) {}

    std::string getTitle() override { return GuiMenuBase::getTitle(); }
    std::string getStatusLine() override { return GuiMenuBase::getStatusLine(); }

    void renderLineIndexOnRow(int index, int row) override {
        gui->text().renderTextLine(lines[index], row, yoffset, XALIGN_LEFT, 0, font);
    }
};
