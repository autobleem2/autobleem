#pragma once

#include "gui_menuBase.h"

//*******************************
// class GuiStringMenu
//*******************************
class GuiStringMenu : public GuiMenuBase<std::string> {
public:
    GuiStringMenu(ableem::GuiBase &_gui) : GuiMenuBase(_gui) {}

    virtual std::string getTitle() override { return GuiMenuBase::getTitle(); }
    virtual std::string getStatusLine() override { return GuiMenuBase::getStatusLine(); }

    virtual void renderLineIndexOnRow(int index, int row) override {
        gui->renderTextLine(lines[index], row, yoffset, XALIGN_LEFT, 0, font);
    }
};
