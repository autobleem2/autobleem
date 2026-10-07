//
// What the launcher's status panels share (GuiUpdateProgress, GuiRaManager, GuiRaJobProgress): a panel over the
// launcher's dimmed background, as wide as its longest text needs, and the byte counts in "42.1 MB".
//
#pragma once

#include "gui/gui.h"
#include "gui/panel_style.h"
#include "human_bytes.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace evoui_panel {

const int PanelWidth = 800;                     // at least - wider for a long line
const int TextInset = PanelStyle::RowInset + 8; // a line's x in the panel, and its right margin
const int LineHeight = 30;

// "42.1 MB"
inline std::string human(uint64_t bytes) {
    return humanBytes(bytes);
}

// as wide as the widest of `texts` needs (a nightly's name is long: v2.0.0-alpha2-42-g7f26785-nebaa06), from
// PanelWidth up to the screen's margins; a line still too long is elided when drawn
inline int panelWidthFor(Gui &gui, const std::vector<std::pair<const ableem::Font *, std::string>> &texts) {
    int widest = 0;
    for (const auto &t : texts)
        widest = std::max(widest, gui.text().textWidth(*t.first, t.second));
    const int maxPanelWidth = gui.renderer().width() - 2 * PanelStyle::Margin; // the canvas of the frame
    return std::min(maxPanelWidth, std::max(PanelWidth, widest + 2 * TextInset));
}

// the dimmed launcher's snapshot (Gui::renderBackground) under the panel; returns the panel's rect
inline ableem::Rect drawPanel(Gui &gui, const PanelStyle &style, int width, int height) {
    gui.renderBackground();
    style.dim(gui.uiContext());
    ableem::Rect panel{(gui.renderer().width() - width) / 2, (gui.renderer().height() - height) / 2, width, height};
    style.sheet(gui.uiContext(), panel);
    return panel;
}

} // namespace evoui_panel
