//
// GuiSystemMenu: the R2 overlay - everything the old classic main menu offered (Re/Scan, RetroArch,
// Memory Cards, Game Manager, Hardware Information, Options, About, Power Off), reached from the launcher
// now that it is the only screen.
//
#pragma once

#include "../../gui/gui_screen.h"

#include <string>
#include <vector>

//******************
// SystemMenuAction
//******************
// None is also what a cancelled menu (Circle/R2) leaves result at - the caller only acts on the others.
enum class SystemMenuAction {
    None, RescanGames, RetroArch, MemoryCards, GameManager, HardwareInfo, Options, About, PowerOff
};

//******************
// GuiSystemMenu
//******************
// A dumb picker: it knows nothing about what each item does. The caller (GuiLauncher) fills retroArchLabel
// and scanInProgress in before show(), then reads result back and runs the action itself - it already owns
// the reload/close logic every one of these needs afterwards.
class GuiSystemMenu : public GuiScreen {
public:
    void init();
    void render();
    void loop();

    std::string retroArchLabel = "RetroArch";   // "RetroArch" or "EmulationStation", per retroboot.cfg
    bool scanInProgress = false;                // shown as a note on the Re-Scan row, not a disabled state

    SystemMenuAction result = SystemMenuAction::None;

    using GuiScreen::GuiScreen;

private:
    struct Item {
        SystemMenuAction action;
        std::string title;
        std::string description;
    };
    std::vector<Item> items;
    int selected = 0;
};
