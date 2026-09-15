//
// the launcher/main-loop state that used to live directly on the Gui singleton.
//
#pragma once

#include <string>
#include "launcher/ps_game.h"

//******************
// menu / launcher selection constants
//******************
// which top-level screen App::run()'s loop should show next
enum MenuOption { MENU_OPTION_SCAN = 1, MENU_OPTION_RUN, MENU_OPTION_SONY, MENU_OPTION_RETRO, MENU_OPTION_START };

// which emulator/launcher path to use for the game about to start
enum class EmuMode { Pcsx, RetroArch, Launcher };

// the carousel's "set": which games are shown. if you add a new one also update GuiLauncher::showSetName's setNames.
#define SET_PS1       0
#define SET_RETROARCH 1
#define SET_APPS      2
#define SET_LAST      2

// SET_PS1 select sub states. keep SET_PS1_Games_Subdir last as it's going to be left off the L2+Select menu
enum { SET_PS1_All_Games = 0, SET_PS1_Internal_Only, SET_PS1_Favorites, SET_PS1_History, SET_PS1_Games_Subdir };

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

    // where the EvolutionUI carousel was, so Start brings it back to the same place
    struct LauncherState {
        int set = SET_PS1;                          // SET_PS1 / SET_RETROARCH / SET_APPS
        int ps1SelectState = SET_PS1_All_Games;
        int selIndex = 0;             // index into carouselGames
        int usbGameDirIndex = 0;      // top row in menu = /Games
        int raPlaylistIndex = 0;      // top row in menu = first playlist name
        std::string raPlaylistName;
    } launcher;
};
