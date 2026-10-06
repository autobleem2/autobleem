//
// Created by screemer on 2019-02-21.
//

#pragma once

#include "evoui_obj.h"
#include "gui/gui_font.h"

//******************
// PsCenterLabel
//******************
class PsCenterLabel : public PsObj {
public:
    std::string text;
    ableem::Font font;
    ableem::Color textColor;
    ableem::Size textSize;
    // centred on the canvas (-1), or on this x (the 4:3 layout: under the menu row's selected icon), and a text wider
    // than maxWidth (0: the 1280 canvas less 20) drawn in the largest medium size from fitMax down to fitMin that fits
    int centreX = -1;
    int maxWidth = 0;
    int fitMax = 28, fitMin = 14;

    void render() override;

    void setText(const std::string &_text, ableem::Color _textColor);

    explicit PsCenterLabel(const std::string &name1, const std::string &texPath = "");
    ~PsCenterLabel() override;
};
