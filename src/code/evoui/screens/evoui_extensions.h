//
// GuiExtensions: the System menu's Extensions list - every folder in Extensions/ with its icon, name and
// description, or why it cannot run (docs/extensions-plan.md). Cross picks one to run, Triangle turns one
// on or off; the running is the caller's (GuiLauncher::loop_openExtensions).
//
#pragma once

#include "core/services/extension_catalog.h"
#include "gui/gui_screen.h"
#include "gui/panel_style.h"

#include <string>
#include <vector>

//******************
// GuiExtensions
//******************
// A compact panel over the launcher's dimmed background, like the System menu, one row per extension.
// A row that cannot run (not built for this system, a different AutoBleem, disabled, offline for one that
// needs the network) is greyed but stays selectable, so its reason can be read. Ours (Author=AutoBleem team
// - the Store, PSC-Bios, the SDK's sample) come first, the third-party ones after a heading.
class GuiExtensions : public GuiScreen {
public:
    GuiExtensions(ableem::GuiBase &gui, ExtensionCatalog &catalog, bool networkUp)
        : GuiScreen(gui), catalog(catalog), networkUp(networkUp) {}

    void init() override;
    void render() override;
    void loop() override;

    ableem::Texture background; // the launcher's frame, drawn dimmed under the panel
    std::string chosen;         // the extension to run; "" = none (Circle)

    // why an extension cannot run now, as the row shows it; "" = it can
    static std::string reasonFor(const ExtensionInfo &extension, bool networkUp);

private:
    ExtensionCatalog &catalog;
    bool networkUp;
    std::vector<ableem::Texture> icons; // one per catalog entry, invalid when it has none
    std::vector<int> rows;              // the list as shown: catalog indices, HeadingRow for the heading
    static const int HeadingRow = -1;
    int selected = 0; // an index into rows, never the heading
    int firstVisible = 0;
    int rowHeight(int row) const;
    int visibleRows() const; // how many rows from firstVisible fit
    int bodyHeight() const;  // the rows' part of the panel
    void moveSelection(int step);
    int count() const { return static_cast<int>(rows.size()); }
    const ExtensionInfo &extensionAt(int row) const { return catalog.extensions()[rows[row]]; }

    PanelStyle style;
};
