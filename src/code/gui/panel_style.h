//
// PanelStyle: the one look every menu and dialog shares - a dark sheet with a one-pixel edge in the
// launcher's secondary colour over the dimmed screen it came from, a bold header ruled off from the rows, the
// selected row as a translucent band with a bar at its left edge, and the launcher's button hints in the
// footer. The L2+R2 system menu and the update screens drew it first (2026-09); the classic screens took it
// on 2026-09-21 through Gui::renderTextBar/renderHeader/renderStatus and TextRenderer::renderSelectionBox.
//
#pragma once

#include <ableem/ui/renderer.h>
#include <ableem/ui/texture.h>

#include <string>
#include <utility>
#include <vector>

class Gui;
namespace ableem {
struct LauncherTheme;
}
class ThemeAssets;

class PanelStyle {
public:
    // the geometry the system menu set
    static const int HeaderHeight = 74; // the title's band, the rule 8 px above its end
    static const int FooterHeight = 54; // the hints' band
    static const int RowHeight = 60;    // a title-and-description row
    static const int RowInset = 24;     // the text from the panel's edge
    static const int Margin = 40;       // the panel from the screen's edge
    static const int SelectionBar = 5;  // the bar at the selected row's left edge

    // the launcher theme's colours and hint icons, resolved the way GuiLauncher resolves them (white / grey
    // where the theme says nothing, the hint colour falling back to the secondary one)
    static PanelStyle fromTheme(const ableem::LauncherTheme &theme, const ThemeAssets *assets = nullptr);

    ableem::Color text{255, 255, 255, 255};
    ableem::Color secondary{100, 100, 100, 255};
    ableem::Color hint{100, 100, 100, 255};
    bool textShadow = true; // the launcher's halo under every text on the panel
    ableem::Texture crossIcon, circleIcon, triangleIcon;

    // the screen behind the panel, darkened
    void dim(ableem::Renderer &renderer) const;
    // the sheet and its edge
    void sheet(ableem::Renderer &renderer, const ableem::Rect &panel) const;
    // a one-pixel rule across the panel, inset, at y
    void rule(ableem::Renderer &renderer, const ableem::Rect &panel, int y) const;
    // the bold title at the top of the panel and the rule under it; returns the y the rows start at
    int header(Gui &gui, const ableem::Rect &panel, const std::string &title) const;
    // the selected row: the band and the bar, `rect` being the row's full extent
    void selection(ableem::Renderer &renderer, const ableem::Rect &rect) const;
    // a heading row (a label between the rows): a faint band in the secondary colour
    void label(ableem::Renderer &renderer, const ableem::Rect &rect) const;
    // a small triangle at (cx, cy) pointing up (direction -1) or down (1): more rows that way
    void scrollMarker(ableem::Renderer &renderer, int cx, int cy, int direction) const;
    // the hints, icon then label, from (x, y) rightwards; returns the x after the last one
    using Hint = std::pair<const ableem::Texture *, std::string>;
    int hints(Gui &gui, int x, int y, const std::vector<Hint> &hints) const;
};
