//
// Created by screemer on 2018-12-19.
//
#pragma once

#include "../core/main.h"
#include <ableem/ableem.h>
#include <string>
#include <memory>
#include "../engine/scanner.h"
#include "../core/model/ps_game.h"
#include "../core/util.h"
#include "gui_font.h"
#include "text_renderer.h"
#include "../core/environment.h"
#include "../core/model/session.h"

using namespace std;

#define SCREEN_WIDTH  ableem::GuiBase::ScreenWidth
#define SCREEN_HEIGHT ableem::GuiBase::ScreenHeight

//********************
// Gui
//********************
// All SDL access lives in lib_ableem; Gui derives from ableem::GuiBase (window/renderer/input/audio) and adds
// the theme's fonts and textures, the text renderer, and the classic UI's main menu loop (menuSelection(),
// the last piece here that is not drawing - it should become a screen).
class Gui : public ableem::GuiBase {
private:

    Gui();

public:

    Fonts themeFonts;
    Fonts sonyFonts;

    void loadAssets(bool reloadMusic = true);

    void display(bool resume);

    void hideMouseCursor();

    void finish();


    static void splash(const std::string & message);

    void menuSelection();

    void criticalException(const std::string &text);

    ableem::Texture loadThemeTexture(const string& themePath, const string& defaultPath, const string& texname);

    ableem::Rect backgroundRect;
    ableem::Rect logoRect;

    ableem::Texture backgroundImg;
    ableem::Texture logo;
    ableem::Texture cdJewel;
    std::map<std::string, ableem::Texture> buttonTextureMap;

    ableem::Font themeFont;

    Gui(Gui const &) = delete;

    Gui &operator=(Gui const &) = delete;

    static std::shared_ptr<Gui> getInstance() {
        static std::shared_ptr<Gui> s{new Gui};
        return s;
    }

    static bool sortByTitle(const PsGamePtr &i, const PsGamePtr &j) { return lessCaseInsensitive(i->title, j->title); }

    // the text drawing: lines, columns, option rows with their check icons, the |@X| button markers
    TextRenderer &text() { return text_; }

    void renderFreeSpace();

    void renderBackground();

    int renderLogo(bool small);

    void renderStatus(const std::string & text, int pos=-1);

    void renderTextBar();

    void drawText(const std::string &text, const string &topLine="");

private:
    // after themeFont and buttonTextureMap: it holds references to both
    TextRenderer text_;
};
