//
// GuiPackagePicker: the list an engine (an App with Uses=) starts from when more than one game's data fits it
// (autobleem-main docs/packages.md 6) - one row per entry, the game's title and under it the package it comes from.
// Cross picks, Circle goes back and starts nothing. The start and the choice's storing are the caller's
// (GuiLauncher::chooseGameData).
//
#pragma once

#include "core/services/package_service.h"
#include "gui/gui_screen.h"
#include "gui/hold_repeat.h"
#include "gui/panel_style.h"

#include <string>
#include <vector>

// the texts of a package for the player, translated (the services hold the English ids)
std::string packageKindLabel(const std::string &kind);
std::string packageSourceLabel(const std::string &source, bool inApp);
std::string packageLicenceLabel(const std::string &licence);

//******************
// GuiPackagePicker
//******************
// A compact panel over the launcher's dimmed background: the App's title in the header, the heading "Choose game
// data" pinned under it, then two-line rows (the second line a smaller, single, elided line - never wrapped).
class GuiPackagePicker : public GuiScreen {
public:
    GuiPackagePicker(ableem::GuiBase &gui, std::string appTitle, std::vector<PackageEntry> entries, std::string lastId)
        : GuiScreen(gui), appTitle(std::move(appTitle)), entries(std::move(entries)), lastId(std::move(lastId)) {}

    void init() override;
    void draw() override; // the frame's picture: the stack clears before and presents after
    void loop() override;

    int chosen = -1; // the entry picked; -1 = none (Circle)

private:
    std::string appTitle;
    std::vector<PackageEntry> entries;
    std::string lastId;
    int selected = 0;
    int firstVisible = 0;
    DpadHold hold; // Up/Down held: the rows go on at the shared HoldRepeat pace
    PanelStyle style;

    int count() const { return static_cast<int>(entries.size()); }
    int visibleRows() const;
    int bodyHeight() const;
    void moveSelection(int step, bool repeat = false); // a press wraps, a held key's repeat stops at the end
    void publishItems() const;                         // the rows and the cursor to the DebugDriver
};
