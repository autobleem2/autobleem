//
// GuiFlashKitMain: ABFlashKit's one screen - a GuiActionMenu of the three actions (Flash kernel, Full
// backup, Restore mode; Circle quits) over the theme's background, and the FlashUi the actions report
// through while they run: every status line is the spinner's message over the dimmed menu, a question
// is a confirm dialog, a pause keeps the spinner turning.
//
#pragma once

#include "gui/gui_screen.h"
#include "gui/screens/gui_action_menu.h"
#include "core/flash_actions.h"

//********************
// GuiFlashKitMain
//********************
class GuiFlashKitMain : public GuiScreen, public FlashUi {
public:
    explicit GuiFlashKitMain(ableem::GuiBase &_gui) : GuiScreen(_gui), menu(_gui) {}

    void init() override;
    void render() override;
    void loop() override;

    // FlashUi
    void status(const std::string &text) override;
    bool confirm(const std::string &question) override;
    void wait(int ms) override;

private:
    GuiActionMenu menu;
    bool busy = false; // the spinner is up (an action is running)
    void run(FlashKitActions::Outcome (FlashKitActions::*action)());
    std::string lastStatus;
};
