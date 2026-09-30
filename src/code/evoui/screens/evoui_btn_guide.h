//
// Created by screemer on 2019-03-02.
//

#pragma once

#include "gui/gui_screen.h"
#include <ableem/ui/texture.h>

//******************
// GuiBtnGuide
//******************
class GuiBtnGuide : public GuiScreen {
public:
    void draw() override; // the frame's picture: the stack clears before and presents after

    void loop() override;

    ableem::Texture backgroundImg;

    using GuiScreen::GuiScreen;
};
