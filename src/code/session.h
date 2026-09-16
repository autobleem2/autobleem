//
// the launcher/main-loop state that used to live directly on the Gui singleton.
//
#pragma once

#include <string>
#include "core/model/game_set.h"
#include "launcher/ps_game.h"

//******************
// menu / launcher selection constants
//******************
// which top-level screen App::run()'s loop should show next
enum MenuOption { MENU_OPTION_SCAN = 1, MENU_OPTION_RUN, MENU_OPTION_SONY, MENU_OPTION_RETRO, MENU_OPTION_START };

// which emulator/launcher path to use for the game about to start
enum class EmuMode { Pcsx, RetroArch, Launcher };

//******************
// Session
//******************
// Everything that describes "where we are" across one run of the app: what App::run()'s outer loop should do
// next, what game (if any) was asked to start, and where the EvolutionUI carousel was so Start can bring it
// back. Owned by App; reached as `app.session()` from screens (via the GuiScreen shim) or `App::get().session()`
// from the few places that are not screens (the launch interceptors, PsGame, UtilTime).
struct Session {
    MenuOption menuOption = MENU_OPTION_SCAN;
    bool forceScan = false;    // true when the games changed and a rescan is needed before showing the menu

    // what GuiLauncher asked App::run()'s loop to do
    bool startingGame = false;
    PsGamePtr runningGame;
    EmuMode emuMode = EmuMode::Pcsx;
    int resumePoint = -1;
    bool resumingGui = false;   // true right after a game exits: skip the classic menu and reopen the carousel

    // where the EvolutionUI carousel was, so Start brings it back to the same place.
    // GuiLauncher holds a copy of this and writes it back through GuiLauncher::rememberSelection().
    GameSetSelection launcher;
};
