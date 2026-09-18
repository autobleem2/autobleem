#pragma once

#include "gui_scroll_win.h"

//******************
// GuiPadTest
//******************
class GuiPadTest : public GuiScrollWin {
public:
    void init() override;
    void render() override { GuiScrollWin::render(); }
    void loop() override;

    int joyid = -1; // SDL_JoystickID; kept as a plain int so this header needs no SDL type

    using GuiScrollWin::GuiScrollWin;
};
