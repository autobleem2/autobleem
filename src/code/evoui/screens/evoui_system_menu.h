//
// GuiSystemMenu: the L2+R2 overlay - everything the old classic main menu offered (Re/Scan, RetroArch,
// Memory Cards, Game Manager, Hardware Information, Options, About, Power Off), reached from the launcher
// now that it is the only screen - and the Quick menu (d-pad Up in the Games state, or the gear icon of the
// game's icon row): the few things a player reaches for from the carousel, the System menu last.
//
#pragma once

#include "gui/gui_screen.h"
#include "gui/panel_style.h"

#include <string>
#include <vector>

//******************
// SystemMenuAction
//******************
// None is also what a cancelled menu (Circle) leaves result at - the caller only acts on the others.
enum class SystemMenuAction {
    None,
    RescanGames,
    RetroArch,
    MemoryCards,
    GameManager,
    HardwareInfo,
    Options,
    Extensions,     // the Extensions list (docs/extensions-plan.md), on every target
    Processors,     // the scanner processors' sequences (docs/scanner-processors-plan.md)
    SoftwareUpdate, // AB_ONLINE_UPDATE builds only
    About,
    PowerOff,
    Network,    // Network & Controllers: the extension providing the "network" entry, only when one does
    Store,      // the Quick menu's: the store extension, run directly
    SystemMenu, // the Quick menu's last row: the System menu itself
};

//******************
// GuiSystemMenu
//******************
// A dumb picker: it knows nothing about what each item does. The caller (GuiLauncher) fills kind, the
// labels, the flags and background in before show(), then reads result back and runs the action itself -
// it already owns the reload/close logic every one of these needs afterwards.
//
// The look: a compact panel over the launcher's dimmed background, in the launcher's fonts and colours, as
// tall as its rows need. The System menu groups its items under heading rows the cursor skips (Library,
// System, Leave), one line each; the selected item's description is in a strip above the footer, a status
// ("scan running", "update available") at the row's right edge. More rows than fit scroll, with a marker
// at the edge they are beyond.
class GuiSystemMenu : public GuiScreen {
public:
    enum class Kind { System, Quick };

    void init() override;
    void render() override;
    void loop() override;

    Kind kind = Kind::System;
    std::string retroArchLabel = "RetroArch"; // "RetroArch" or "EmulationStation", per retroboot.cfg
    bool updateAvailable = false;             // shown as a note on the Software Update row
    bool scanInProgress = false;              // shown as a note on the Re-Scan row, not a disabled state
    bool networkProvided = false;             // an extension provides "network": the Network & Controllers row
    // with networkProvided: why the extension providing it cannot run ("PSC-Bios is switched off - ..."); set,
    // the row is greyed with this as its description, still selectable (the caller opens the Extensions list)
    std::string networkUnavailable;
    ableem::Texture background; // the launcher's background, drawn dimmed under the panel

    SystemMenuAction result = SystemMenuAction::None;

    using GuiScreen::GuiScreen;

private:
    struct Row {
        bool heading = false;
        bool greyed = false; // drawn over with the disabled-row style, still selectable
        SystemMenuAction action = SystemMenuAction::None;
        std::string key;   // the untranslated title: what tools/ab_drive.py's `menu "<title>"` matches
        std::string title; // translated
        std::string description;
        std::string note; // a status at the row's right edge
    };
    std::vector<Row> rows;
    int selected = 0;     // a row index, never a heading's
    int firstVisible = 0; // the first row on screen, when there are more than fit

    // key and title are the same words, untranslated and translated: the literal _("...") at every call is
    // what tools/lang_tools.py extracts
    void addItem(SystemMenuAction action, const std::string &key, const std::string &title,
                 const std::string &description, const std::string &note = "");
    void addHeading(const std::string &title);
    void publishItems() const; // the items' keys to the DebugDriver, in cursor order
    int rowHeight(const Row &row) const;
    int roomForRows() const;
    int visibleRowCount() const; // from firstVisible, as many as fit in roomForRows()
    int visibleHeight() const;   // their height
    void keepSelectedVisible();
    void moveSelection(int step);

    PanelStyle style; // the shared look, resolved at init
};
