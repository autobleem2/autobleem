//
// GameDetailPane: the right-hand side of the game editors and the Game Manager - the cover, the screenshot
// when there is one, and the game's facts as label/value pairs under them - drawn in PanelStyle's look
// inside the classic panel's content rect, with a rule between it and the rows on the left. The rows stop
// at rowsRight().
//
#pragma once

#include <ableem/ui/texture.h>
#include <ableem/ui/types.h>

#include <string>
#include <utility>
#include <vector>

class Gui;

class GameDetailPane {
public:
    static const int Width = 360;     // the pane, rule included
    static const int Inset = 20;      // the cover and the facts from the pane's edges
    static const int CoverSize = 236; // the cover's box, aspect-fit
    static const int SnapHeight = 140;

    ableem::Texture cover;
    ableem::Texture snap;
    std::vector<std::pair<std::string, std::string>> facts; // label, value - a value is elided to the pane

    // where the pane sits: the right of the classic content rect
    static ableem::Rect rect(Gui &gui);
    // the x the rows on the left stop at (their values are right-aligned to it)
    static int rowsRight(Gui &gui);

    void render(Gui &gui) const;
};
