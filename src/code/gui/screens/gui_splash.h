//
// Created by screemer on 2019-01-24.
//
#pragma once

#include "../gui_screen.h"

//********************
// GuiSplash
//********************
class GuiSplash : public GuiScreen {
public:
    void render();
    void loop();

    int alpha = 0;
    int start = 0;

    // fade in, hold at full brightness for SplashHoldDuration, fade back out, then loop() returns and the
    // launcher takes over (its own fade-in - see GuiLauncher - picks up where this leaves off)
    enum class Phase { FadeIn, Hold, FadeOut };
    Phase phase = Phase::FadeIn;
    long holdStart = 0;

    using GuiScreen::GuiScreen;
};

