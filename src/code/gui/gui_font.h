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
    // the bold font at whatever other sizes were asked for - see boldAtSize()
    std::map<int, ableem::Font> boldBySize;
    ableem::Renderer *renderer = nullptr;
public:
    Fonts();
    // use operator [] to get or set the shared font
    ableem::Font & operator [] (FontEnum size) { return fonts[size]; }
    static ableem::Font openNewSharedCachedFont(const std::string &filename, int fontSize, ableem::Renderer &renderer);
    // The bold font at an arbitrary size, opened once and kept: for text that has to shrink to fit (a
    // long game title). Opening a font builds its glyph cache, far too slow to do per frame - which is
    // what the meta panel did before this (AutoBleem-NG's SizesOfBoldThemeFont, e520f2c1).
    ableem::Font &boldAtSize(int fontSize);
    // (re)opens every size from these two ttf files - the theme's launcher fonts, or the console's own
    void openAllFonts(const std::string &mediumTtf, const std::string &boldTtf, ableem::Renderer &renderer);
    // drops every font - their glyph textures belong to the renderer, so before the display is released
    void closeAll() { fonts.clear(); boldBySize.clear(); }
};
