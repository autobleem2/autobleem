//
// Created by screemer on 2019-01-24.
//
#pragma once

#include "../gui_screen.h"
#include "../starfx.h"
#include "../surprise_game.h"
#include "../gui_font.h"
#include <ableem/ui/texture.h>

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

private:
    // the "Surprise" easter egg: Start swaps the credits for a small shoot-em-up over the same starfield
    bool surpriseMode = false;
    bool crossHeld = false;
    SurpriseGame game;
    SurpriseSprites sprites;
    int savedHighScore = 0;   // mirrors config.ini's "surprisehighscore"; written back only when beaten
    void renderSurprise();
};
