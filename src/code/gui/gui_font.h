#pragma once
#include <ableem/ui/font.h>
#include <ableem/ui/renderer.h>
#include <map>
#include <string>

enum FontEnum {
    FONT_15_BOLD,
    FONT_20_BOLD,
    FONT_22_MED,
    FONT_28_BOLD,
};
enum FontType { FONT_MED, FONT_BOLD };

//********************
// Fonts
//********************
// A themed collection of the fixed set of font sizes AutoBleem uses, on top of ableem::Font. Theme knowledge
// (which .ttf file backs FONT_MED vs FONT_BOLD, where the theme's font directory is) stays here in the app;
// lib_ableem only knows how to load one font from one path.
class Fonts {
    std::string rootPath;
    std::string medPath;
    std::string boldPath;
    struct FontInfo {
        FontEnum    fontEnum;
        int         size;
        FontType    fontType;
    };
    static FontInfo allFontInfos[];
    std::map<FontEnum, ableem::Font> fonts;
    std::map<FontEnum, FontInfo> fontInfos;
public:
    Fonts();
    // use operator [] to get or set the shared font
    ableem::Font & operator [] (FontEnum size) { return fonts[size]; }
    static ableem::Font openNewSharedCachedFont(const std::string &filename, int fontSize, ableem::Renderer &renderer);
    // like openNewSharedCachedFont, but looks up the current theme's font path and renderer via Gui::getInstance()
    static ableem::Font openSpecificSharedCachedFont(FontType type, int fontSize);
    // in gui_launcher.cpp this call is used to change all the fonts to use the fonts in the current theme
    void openAllFonts(const std::string &_rootPath, ableem::Renderer &renderer);
};
