//
// the launcher/main-loop state that used to live directly on the Gui singleton.
//
#pragma once

#include <string>
#include "game_set.h"
#include "ps_game.h"

//******************
// menu / launcher selection constants
//******************
// what AutoBleem::run()'s loop does when the launcher closes - written into autobleem_cfg.sh's AB_SELECTION
// for rc/selection.sh to read after the process exits. The numeric values are part of that contract and
// must not change. IDLE is the resting default and also what a plain "close the app" leaves behind -
// selection.sh's own default (start_autobleem, i.e. come straight back here) is the right thing for that.
enum MenuOption { MENU_OPTION_IDLE = 1, MENU_OPTION_RETRO = 4, MENU_OPTION_START = 5 };

// which emulator/launcher path to use for the game about to start
enum class EmuMode { Pcsx, RetroArch, Launcher };

//******************
// Session
//******************
// Everything that describes "where we are" across one run of the app: what App::run()'s outer loop should do
// next, what game (if any) was asked to start, and where the EvolutionUI carousel was so Start can bring it
// back. Owned by App; reached as `app.session()` from screens (via the GuiScreen shim) or `App::get().session()`
// from the few places that are not screens (UtilTime).
struct Session {
    MenuOption menuOption = MENU_OPTION_IDLE;

    // what GuiLauncher asked App::run()'s loop to do
    bool startingGame = false;
    PsGamePtr runningGame;
    EmuMode emuMode = EmuMode::Pcsx;
    int resumePoint = -1;
    bool resumingGui = false; // true right after a game exits: skip the classic menu and reopen the carousel

    // where the EvolutionUI carousel was, so Start brings it back to the same place.
    // GuiLauncher holds a copy of this and writes it back through GuiLauncher::rememberSelection().
    GameSetSelection launcher;
};
