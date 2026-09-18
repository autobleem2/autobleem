//
// Created by screemer on 2019-03-02.
//

#pragma once

#include "../../gui/gui_screen.h"
#include <ableem/ui/texture.h>

//******************
// GuiBtnGuide
//******************
class GuiBtnGuide : public GuiScreen {
public:
    void render() override;

    void loop() override;

    ableem::Texture backgroundImg;

    using GuiScreen::GuiScreen;
};
