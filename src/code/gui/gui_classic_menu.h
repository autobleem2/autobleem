//
// ClassicMenuScreen: the classic UI's main menu - the status-bar menu App::run() shows between everything else.
//
#pragma once

#include "gui_screen.h"

#include <string>

//********************
// ClassicMenuScreen
//********************
// Was Gui::menuSelection(). It draws the main menu (or the "games changed, press X" prompt) in the status bar
// and waits for a button: the ones that hand a decision back to App::run() set app.session().menuOption and
// close; the ones that open another screen show it and come back to the menu. When the launcher asked for a
// game to start, or a game has just exited and the launcher should reopen, the session says so and the
// screen acts on that before reading any input.
//
// menuSelection() used to recurse into itself after every sub-screen; the screen restarts itself instead
// (restart()), which is the same thing without the stack growing by a frame per visit.
class ClassicMenuScreen : public GuiScreen {
public:
    using GuiScreen::GuiScreen;

    void init() override;
    void render() override;
    void loop() override;

private:
    // back to the top: recompute the menu for the session as it is now and draw it
    void restart();
    void drawMenu();
    void showLauncher();
    void powerOff();

    bool forceScan = false;
    bool otherMenuShift = false;   // L1 held: the second menu row is showing
    bool powerOffShift = false;    // L2 held: R2 powers off
    std::string mainMenu;
    std::string forceScanMenu;
    std::string otherMenu;
    std::string gamepadNotice;
};
