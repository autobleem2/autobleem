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

#include <string>
#include <vector>

//********************
// GuiAbout
//********************
class GuiAbout : public GuiScreen {
public:
    StarFx fx;
    void init() override;
    void render() override;
    void loop() override;
    ableem::Texture logo;
    ableem::Font font;
    using GuiScreen::GuiScreen;
    // the lines under the logo; AutoBleem's credits when the caller leaves it empty (a tool sets its own)
    std::vector<std::string> credits;
    static std::vector<std::string> autobleemCredits();

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
