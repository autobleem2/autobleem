//
// Created by screemer on 2019-02-21.
//

#include "evoui_centerlabel.h"

using namespace std;

//*******************************
// PsCenterLabel::PsCenterLabel
//*******************************
PsCenterLabel::PsCenterLabel(const string &name1, const string &texPath) : PsObj(name1, "") {}

//*******************************
// PsCenterLabel::setText
//*******************************
void PsCenterLabel::setText(const string &_text, ableem::Color _textColor) {
    text = _text;
    textColor = _textColor;
    textColor.a = 255; // if you're rendering with a different color you need this or it will be transparent

    textSize = gui->text().getFontTextSize(font, text);
    x = gui->text().align_xPosition(XALIGN_CENTER, x, textSize.w);
}

//*******************************
// PsCenterLabel::~PsCenterLabel
//*******************************
PsCenterLabel::~PsCenterLabel() = default;

//*******************************
// PsCenterLabel::render
//*******************************
void PsCenterLabel::render() {
    if (visible) {
        // a header or a hint wider than the screen (a long translation) shrinks rather than runs off
        if (textSize.w > SCREEN_WIDTH - 20) {
            gui->text().renderFittedText_WithColor(FONT_MED, 28, 14, text, x, y, SCREEN_WIDTH - 20, textColor,
                                                   XALIGN_CENTER);
        } else {
            gui->text().renderText_WithColor(font, text, x, y, textColor, XALIGN_CENTER);
        }
    }
}
