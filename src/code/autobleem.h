//
// AutoBleem: the program. App (the model, in ab_ui) plus the outer loop main() hands control to.
//
#pragma once

#include "app.h"
#include "core/services/carousel_session.h"
#include "core/services/output_mode.h"
#include <functional>

//******************
// AutoBleem
//******************
// One instance is created in main() and its run() is the whole program from there on: open the databases,
// restore the memory cards, start the background scan (ScanService - a rescan is requested up front when
// the games directory looks like it changed since the last one), show the splash, then loop the launcher ->
// launch a game -> back to the launcher, until something asks to exit to RetroArch/EmulationStation. It is
// also where the ProcessRunner is chosen: the fork on the console, a splash on a dev host where there is
// nothing to fork.
class AutoBleem : public App {
public:
    AutoBleem();

    int run();

private:
    static std::unique_ptr<ProcessRunner> makeProcessRunner();
    bool openLibrary(); // covers dir + regional.db + internal.db
    // everything around a program run in front of the launcher - the mixer and the pads released, the
    // display given up (a Pi) or kept with a picture on it (Windows), `body` run, the way back to the
    // carousel; `retroArch` picks the picture and how long the machine gets to settle after
    void runOutside(bool retroArch, const std::function<void()> &body);
    void launchGame();       // the MENU_OPTION_START handling
    void takeAppMessage();   // what an App left in <runtime>/app-message.txt, on the notification line
    void runRetroArchMenu(); // MENU_OPTION_RETRO in direct mode: RetroArch's own menu, then back
    // Options -> Display (OutputMode, MENU_OPTION_DISPLAY) - see autobleem.cpp
    void takeEmulatorOutputMode();
    void switchOutputMode(const OutputMode &mode);
    void tryOutputMode(const std::string &token);
    bool confirmPendingOutputMode();
    // BUG-40: the carousel's place across a launcher that exits for a display change / restart and is started
    // over (core's CarouselSession): saved as it leaves, taken once at the next start
    void saveCarouselSession();
    void restoreCarouselSession();
};
