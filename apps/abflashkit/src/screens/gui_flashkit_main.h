//
// GuiFlashKitMain: ABFlashKit's one screen - the theme's background and a status bar offering the three
// actions (Cross: Flash Kernel, Square: Full backup, Triangle: Restore Mode, Circle: Quit), and the
// FlashUi the actions report through while they run (status lines, confirmations, pauses).
//
#pragma once

#include "gui/gui_screen.h"
#include "core/flash_actions.h"

//********************
// GuiFlashKitMain
//********************
class GuiFlashKitMain : public GuiScreen, public FlashUi {
public:
    using GuiScreen::GuiScreen;

    void render() override;
    void loop() override;

    // FlashUi
    void status(const std::string &text) override;
    bool confirm(const std::string &question) override;
    void wait(int ms) override;

private:
    void run(FlashKitActions::Outcome (FlashKitActions::*action)());
    std::string lastStatus;
};
