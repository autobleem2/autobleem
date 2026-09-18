//
// GuiTextPage: a titled page of static text in the classic layout (panel, small logo, status bar) - what
// a tool's instructions or an info box are. Circle (or Escape) goes back; the caller sets `title` and
// `lines` and calls show().
//
#pragma once

#include "../gui_screen.h"

#include <string>
#include <vector>

//********************
// GuiTextPage
//********************
class GuiTextPage : public GuiScreen {
public:
    using GuiScreen::GuiScreen;

    void render() override;
    void loop() override;

    std::string title;              // drawn centred on the first row, decorated -=title=-
    std::vector<std::string> lines; // one row each, left aligned; "" is a blank row
    bool centred = false;           // centre every line instead
};
