//
// Created by screemer on 2019-01-24.
//
#pragma once

#include "gui_screen.h"
#include "starfx.h"
#include "gui_font.h"
#include <ableem/texture.h>

//********************
// GuiAbout
//********************
class GuiAbout : public GuiScreen {
public:
    StarFx fx;
    void init();
    void render();
    void loop();
    ableem::Texture logo;
    ableem::Font font;
    using GuiScreen::GuiScreen;
};
