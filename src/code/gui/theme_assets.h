//
// ThemeAssets: the current theme's textures and fonts, loaded once per theme change.
//
#pragma once

#include "gui_font.h"
#include "../core/services/config.h"
#include "../core/services/theme.h"

#include <ableem/ableem.h>

#include <map>
#include <string>

//********************
// ThemeAssets
//********************
// Was the asset half of Gui: the background, logo and jewel-case textures, the button-marker textures the
// text renderer draws for "|@X|", the theme's font at the theme's size, and the two font sets (the theme's
// and the console's own). load() reads them for whichever theme config.ini names, falling back to
// themes/default for anything missing, and is what an Options-menu theme change calls. Screens reach it as
// gui->assets().
class ThemeAssets {
public:
    ThemeAssets(ableem::Renderer &renderer, Theme &theme, Config &config);

    // (re)loads theme.ini and everything below for the theme it names; the previous textures are released first
    void load();
    // drops every texture and font: they belong to the renderer and must be gone before Gui::releaseDisplay()
    // destroys it. load() brings them back once the display is acquired again.
    void unload();

    Fonts themeFonts;
    Fonts sonyFonts;
    ableem::Font themeFont;
    // the classic font (the file themeFont was opened from - theme, user or CJK) at another size, for a screen
    // whose rows will not fit at the theme's
    ableem::Font classicFontAtSize(int size);

    ableem::Rect backgroundRect;
    ableem::Rect logoRect;

    ableem::Texture backgroundImg;
    ableem::Texture logo;
    ableem::Texture cdJewel;
    // the edge of a printed cardboard box, a 9-slice (evoimg/bigbox.png, see tools/make_bigbox_frame.py):
    // what a RetroArch game's or an App's cover is composed into, where a PS1 game gets the jewel case
    ableem::Texture bigBoxFrame;
    std::map<std::string, ableem::Texture> buttonTextureMap; // "X", "O", "Start", "Check", ... -> its texture

private:
    // the theme's own file for `texname`, or the default theme's when it has none
    ableem::Texture loadThemeTexture(const std::string &themePath, const std::string &defaultPath,
                                     const std::string &texname);

    ableem::Renderer &renderer_;
    Theme &theme_;
    Config &config_;
    std::string classicFontFile_;
};
