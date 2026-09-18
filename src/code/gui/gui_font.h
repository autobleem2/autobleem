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
// (which .ttf file backs FONT_MED vs FONT_BOLD - the theme's launcher.fonts.medium/bold) stays here in the
// app; lib_ableem only knows how to load one font from one path.
class Fonts {
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
    // like openNewSharedCachedFont, but uses the current theme's medium/bold font and renderer via Gui::getInstance()
    static ableem::Font openSpecificSharedCachedFont(FontType type, int fontSize);
    // (re)opens every size from these two ttf files - the theme's launcher fonts, or the console's own
    void openAllFonts(const std::string &mediumTtf, const std::string &boldTtf, ableem::Renderer &renderer);
    // drops every font - their glyph textures belong to the renderer, so before the display is released
    void closeAll() { fonts.clear(); }
};
