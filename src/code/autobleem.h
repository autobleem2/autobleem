//
// AutoBleem: the program. App (the model, in ab_ui) plus the outer loop main() hands control to.
//
#pragma once

#include "app.h"

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
    bool openLibrary();                                                   // covers dir + regional.db + internal.db
    void launchGame();                                                    // the MENU_OPTION_START handling
};
