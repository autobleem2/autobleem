//
// Created by screemer on 2019-01-24.
//
#pragma once

#include "../gui_screen.h"
#include "../starfx.h"
#include "../surprise_game.h"
#include "../gui_font.h"
#include <ableem/ui/texture.h>
#include <ableem/ui/audio.h>

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
    int savedHighScore = 0; // mirrors config.ini's "surprisehighscore"; written back only when beaten

    // the game always has some music: the theme's track is ducked to 50% if it was already playing, or -
    // when the theme/config has no music at all (a silent theme, or "nomusic") - this bundled track takes
    // over for the duration so Surprise mode is never silent, then playMusic() sorts out what should resume
    ableem::Music surpriseMusic;
    bool duckedThemeMusic = false;
    bool playingFallbackMusic = false;

    void renderSurprise();
};
