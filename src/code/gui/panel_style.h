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

    // the launcher theme's colours, resolved the way GuiLauncher resolves them (white / grey where the theme
    // says nothing, the hint colour falling back to the secondary one)
    static PanelStyle fromTheme(const ableem::LauncherTheme &theme);

    ableem::Color text{255, 255, 255, 255};
    ableem::Color secondary{100, 100, 100, 255};
    ableem::Color hint{100, 100, 100, 255};
    bool textShadow = true; // the launcher's halo under every text on the panel

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
    // a footer hint: one or more button icons ("X", "O", "T", "S", "Start", "Select", "L1", "R1", "L2", "R2",
    // "Esc", "Enter", "Tab" - the launcher's hint icons for the first three, the theme's button textures
    // for the rest) and its label
    struct HintItem {
        std::vector<std::string> icons;
        std::string label;
    };
    // the status-line protocol every screen writes - "Card 1/12   |@L1|/|@R1| Page  |@X| Rename  |@O| Go back |"
    // - taken apart: the text before the first marker is the status (a counter, drawn at the footer's right
    // edge), each marker and the text up to the next one is a hint, a marker whose text is empty or a
    // separator ("/", "|") joins the next hint's icons
    static std::vector<HintItem> parseHints(const std::string &line, std::string &status);
    // the footer: the rule along the top of `footer` (FooterHeight tall, the panel's width), the hints from
    // the left inset in the largest of the launcher's fonts they fit in, the status at the right edge. The
    // hints are drawn in the one order every screen shares, whatever order they were given in: Cross,
    // Circle, Triangle, Square, Start, Select, L1/R1, L2/R2, then the keyboard's keys - so a footer reads
    // "what Cross does, how to get out, then the rest" on every screen
    void footer(Gui &gui, const ableem::Rect &footer, const std::vector<HintItem> &hints,
                const std::string &status = "", bool withRule = true) const;
    // the same from the protocol string
    void footer(Gui &gui, const ableem::Rect &footer, const std::string &line, bool withRule = true) const;

    // one button as the footer draws it at (x, y), `height` tall: the face buttons (X, O, T, S) as the
    // theme's 30 px images, every named button (Start, Select, L1..R2, Esc, Enter, Tab, or any word such as
    // RESET) as a chip - a small dark box with a light edge and the name in small bold capitals - so a
    // "START" reads at the size of the icons next to it. Returns the width drawn.
    int button(Gui &gui, const std::string &key, int x, int y, int height = 30) const;
    // a marker string as the button guide writes it - "|@L2| + |@Select|", "|@X| / |@O|", "RESET" - drawn as
    // icons, chips and the text between them; returns the width
    int buttons(Gui &gui, const std::string &markers, int x, int y, int height = 30) const;
    // the width the two above would draw, without drawing - for laying a row out first
    int buttonWidth(Gui &gui, const std::string &key, int height = 30) const;
    int buttonsWidth(Gui &gui, const std::string &markers, int height = 30) const;

private:
    int layoutButtons(Gui &gui, const std::string &markers, int x, int y, int height, bool draw) const;
};
