//
// Created by screemer on 2018-12-19.
//
#pragma once

#include "../core/main.h"
#include <ableem/ableem.h>
#include <string>
#include <memory>
#include "../core/services/system.h"
#include "gui_font.h"
#include "text_renderer.h"
#include "theme_assets.h"
#include "../core/services/environment.h"

using namespace std;

#define SCREEN_WIDTH ableem::GuiBase::ScreenWidth
#define SCREEN_HEIGHT ableem::GuiBase::ScreenHeight

//********************
// Gui
//********************
// All SDL access lives in lib_ableem; Gui derives from ableem::GuiBase (window/renderer/input/audio) and adds
// the theme's assets, the text renderer, and the few drawing helpers that combine the two (background, logo,
// status bar). Nothing here decides anything: the screens do, and the program's run() shows them.
class Gui : public ableem::GuiBase {
private:
    Gui();
    static float outputScale();
    static int multisampleSamples();
    static bool fullscreen();
    static std::string windowTitle_;

public:
    // the window's title: the program's name. Set by AppBase before the first getInstance() - it cannot
    // change once the window exists
    static void setWindowTitle(const std::string &title) { windowTitle_ = title; }
    // (re)loads the theme's textures and fonts, and its music unless told not to
    void loadAssets(bool reloadMusic = true);

    void display(bool resume);

    void hideMouseCursor();

    void finish();

    static void splash(const std::string &message);

    void criticalException(const std::string &text);

    Gui(Gui const &) = delete;

    Gui &operator=(Gui const &) = delete;

    static std::shared_ptr<Gui> getInstance() {
        static std::shared_ptr<Gui> s{new Gui};
        return s;
    }

    // the theme's textures and fonts
    ThemeAssets &assets() { return assets_; }

    // Hands the display to an emulator: drops every texture and font, then the renderer and the window (on
    // a Pi with no compositor the window is the DRM master and the emulator cannot open the display while it
    // exists). display(true) afterwards notices the window is gone, brings it back and reloads the assets.
    void releaseDisplay();
    // The desktop's alternative: the window minimised for the run and raised again after it - nothing is
    // dropped or reloaded (a WinProcessRunner launch, see ProcessRunner::minimisesLauncherWindow()).
    void minimizeWindow() { platform().minimizeWindow(); }
    void restoreWindow() { platform().restoreWindow(); }
    // the text drawing: lines, columns, option rows with their check icons, the |@X| button markers
    TextRenderer &text() { return text_; }

    void renderFreeSpace();

    void renderBackground();

    int renderLogo(bool small);

    void renderStatus(const std::string &text, int pos = -1);

    void renderTextBar();

    void drawText(const std::string &text, const string &topLine = "");

private:
    ThemeAssets assets_;
    TextRenderer text_; // after assets_: it holds references to the theme font and the button textures
};
