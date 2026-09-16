//
// Created by screemer on 2019-02-21.
//

#include "evoui_centerlabel.h"

using namespace std;

//*******************************
// PsCenterLabel::PsCenterLabel
//*******************************
PsCenterLabel::PsCenterLabel(const string & name1, const string & texPath) : PsObj(name1, "")
{
}

//*******************************
// PsCenterLabel::setText
//*******************************
void PsCenterLabel::setText(const string & _text, ableem::Color _textColor)
{
    text = _text;
    textColor = _textColor;
    textColor.a = 255; // if you're rendering with a different color you need this or it will be transparent

    textSize = gui->text().getFontTextSize(font, text);
    x = gui->text().align_xPosition(XALIGN_CENTER, x, textSize.w);
}

//*******************************
// PsCenterLabel::~PsCenterLabel
//*******************************
PsCenterLabel::~PsCenterLabel()
{
}

//*******************************
// PsCenterLabel::render
//*******************************
void PsCenterLabel::render()
{
    if (visible) {
        gui->text().renderText_WithColor(font, text, x, y, textColor, XALIGN_CENTER);
    }
}
