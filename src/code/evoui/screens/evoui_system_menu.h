//
// GuiSystemMenu: the L2+R2 overlay - everything the old classic main menu offered (Re/Scan, RetroArch,
// Memory Cards, Game Manager, Hardware Information, Options, About, Power Off), reached from the launcher
// now that it is the only screen.
//
#pragma once

#include "gui/gui_screen.h"
#include "gui/panel_style.h"

#include <string>
#include <vector>

//******************
// SystemMenuAction
//******************
// None is also what a cancelled menu (Circle/R2) leaves result at - the caller only acts on the others.
enum class SystemMenuAction {
    None,
    RescanGames,
    RetroArch,
    MemoryCards,
    GameManager,
    HardwareInfo,
    Options,
    Extensions,     // the Extensions list (docs/extensions-plan.md), on every target
    SoftwareUpdate, // AB_ONLINE_UPDATE builds only
    About,
    PowerOff
};

//******************
// GuiSystemMenu
//******************
// A dumb picker: it knows nothing about what each item does. The caller (GuiLauncher) fills retroArchLabel,
// scanInProgress and background in before show(), then reads result back and runs the action itself - it
// already owns the reload/close logic every one of these needs afterwards.
//
// The look: a panel over the launcher's dimmed background, in the launcher's fonts and colours, as tall
// as its rows need - more rows than the screen has room for scroll, with a marker at the edge they are
// beyond - and the launcher's button hints in its footer.
class GuiSystemMenu : public GuiScreen {
public:
    void init() override;
    void render() override;
    void loop() override;

    std::string retroArchLabel = "RetroArch"; // "RetroArch" or "EmulationStation", per retroboot.cfg
    bool updateAvailable = false;             // shown as a note on the Software Update row
    bool scanInProgress = false;              // shown as a note on the Re-Scan row, not a disabled state
    ableem::Texture background;               // the launcher's background, drawn dimmed under the panel

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
    int firstVisible = 0; // the first row on screen, when there are more than fit
    int visibleRows() const;
    void moveSelection(int step);

    PanelStyle style; // the shared look, resolved at init
};
