//
// Created by screemer on 2019-03-02.
//

#pragma once

#include "../gui/gui_screen.h"
#include <ableem/ui/texture.h>

//******************
// GuiBtnGuide
//******************
class GuiBtnGuide : public GuiScreen {
public:
    void render();

    void loop();

    ableem::Texture backgroundImg;

    using GuiScreen::GuiScreen;
};
