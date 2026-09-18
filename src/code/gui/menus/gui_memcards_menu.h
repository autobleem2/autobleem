#pragma once

#include "gui_string_menu.h"
#include <vector>
#include <string>

//********************
// GuiMemcards
//********************
class GuiMemcards : public GuiStringMenu {
public:
    explicit GuiMemcards(ableem::GuiBase &_gui) : GuiStringMenu(_gui) {}

    void init() override;

    std::string getTitle() override { return "-=" + _("Custom Memory Cards") + "=-"; }
    std::string getStatusLine() override; // returns the status line at the bottom

    void doCircle_Pressed() override;
    void doSquare_Pressed() override;
    void doTriangle_Pressed() override;
    void doCross_Pressed() override;

    void doEnter() override { doCross_Pressed(); }
    void doEscape() override { doCircle_Pressed(); }
    void doDelete() override { doSquare_Pressed(); }
};
