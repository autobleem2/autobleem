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
    // the theme's fonts at whatever other sizes were asked for - see atSize()
    std::map<std::pair<int, int>, ableem::Font> bySize;   // (FontType, size)
    ableem::Renderer *renderer = nullptr;
public:
    Fonts();
    // use operator [] to get or set the shared font
    ableem::Font & operator [] (FontEnum size) { return fonts[size]; }
    static ableem::Font openNewSharedCachedFont(const std::string &filename, int fontSize, ableem::Renderer &renderer);
    // The medium or bold font at an arbitrary size, opened once and kept: for text that has to shrink to
    // fit (a long game title - TextRenderer::fittingFont). Opening a font builds its glyph cache, far too
    // slow to do per frame, which is what the meta panel did before this (AutoBleem-NG's
    // SizesOfBoldThemeFont, e520f2c1).
    ableem::Font &atSize(FontType type, int fontSize);
    ableem::Font &boldAtSize(int fontSize) { return atSize(FONT_BOLD, fontSize); }
    // A language the theme's fonts cannot draw (Chinese) gets resources/fonts/NotoSansSC-Regular.otf for
    // everything, medium and bold alike; "" for any other language, or when the font is not shipped.
    static std::string cjkFontFor(const std::string &language);
    // (re)opens every size from these two ttf files - the theme's launcher fonts, or the console's own
    void openAllFonts(const std::string &mediumTtf, const std::string &boldTtf, ableem::Renderer &renderer);
    // drops every font - their glyph textures belong to the renderer, so before the display is released
    void closeAll() { fonts.clear(); bySize.clear(); }
};
