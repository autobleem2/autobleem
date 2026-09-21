//
// Created by screemer on 2019-01-24.
//
#pragma once

#include <string>
#include "../gui_screen.h"

//********************
// GuiConfirm
//********************
class GuiConfirm : public GuiScreen {
public:
    void render() override;
    void loop() override;

    std::string label = "";
    std::string title;                     // the header; empty = "Please confirm"
    std::string confirmLabel, cancelLabel; // the footer's hints; empty = "Confirm" / "Cancel"
    bool result = false;

    using GuiScreen::GuiScreen;
};
