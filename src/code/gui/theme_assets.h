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

    ableem::Rect backgroundRect;
    ableem::Rect logoRect;

    ableem::Texture backgroundImg;
    ableem::Texture logo;
    ableem::Texture cdJewel;
    std::map<std::string, ableem::Texture> buttonTextureMap;   // "X", "O", "Start", "Check", ... -> its texture

private:
    // the theme's own file for `texname`, or the default theme's when it has none
    ableem::Texture loadThemeTexture(const std::string &themePath, const std::string &defaultPath,
                                     const std::string &texname);

    ableem::Renderer &renderer_;
    Theme &theme_;
    Config &config_;
};
