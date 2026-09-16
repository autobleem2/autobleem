//
// AutoBleem: the program. App (the model, in ab_ui) plus the outer loop main() hands control to.
//
#pragma once

#include "app.h"

//******************
// AutoBleem
//******************
// One instance is created in main() and its run() is the whole program from there on: open the databases,
// restore the memory cards, decide whether a rescan is due, then loop the classic menu -> scan / launch a
// game -> back to the menu. It is also where the ProcessRunner is chosen: the fork on the console, a splash
// on a dev host where there is nothing to fork.
class AutoBleem : public App {
public:
    AutoBleem();

    int run();

private:
    static std::unique_ptr<ProcessRunner> makeProcessRunner();
    bool openLibrary();                                                   // covers dir + regional.db + internal.db
    void rescan(GamesHierarchy &gamesHierarchy, const std::string &prevPath);   // a Re/Scan menu selection
    void launchGame();                                                    // the MENU_OPTION_START handling
};
