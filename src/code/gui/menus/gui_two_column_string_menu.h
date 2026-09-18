#pragma once

#include "gui_menu_base.h"

//*******************************
// struct TwoColumnsOfText
//*******************************
struct TwoColumnsOfText {
    std::string line_L;
    std::string line_R;
    TwoColumnsOfText(std::string left, std::string right) : line_L(left), line_R(right) {}
};

//*******************************
// class GuiTwoColumnStringMenu
//*******************************
class GuiTwoColumnStringMenu : public GuiMenuBase<TwoColumnsOfText> {
public:
    explicit GuiTwoColumnStringMenu(ableem::GuiBase &_gui) : GuiMenuBase(_gui) {}

    int xoffset_L = 0;
    int xoffset_R = 500;

    std::string getTitle() override { return GuiMenuBase::getTitle(); }
    std::string getStatusLine() override { return GuiMenuBase::getStatusLine(); }

    void renderLineIndexOnRow(int index, int row) override {
        gui->text().renderTextLineToColumns(lines[index].line_L, lines[index].line_R, xoffset_L, xoffset_R, row,
                                            yoffset, font);
    }
};
