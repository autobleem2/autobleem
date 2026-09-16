//
// Created by screemer on 2019-02-21.
//

#pragma once

#include "evoui_obj.h"
#include "../../gui/gui_font.h"

//******************
// PsCenterLabel
//******************
class PsCenterLabel : public PsObj {
public:
    std::string text;
    ableem::Font font;
    ableem::Color textColor;
    ableem::Size textSize;

    void render();

    void setText(const std::string & _text, ableem::Color _textColor);

    PsCenterLabel(const std::string & name1, const std::string & texPath = "");
    ~PsCenterLabel();
};
