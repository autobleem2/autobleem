#include "gui_font.h"
#include <iostream>
#include "../core/services/system.h"
#include <cassert>
#include "../core/main.h"

using namespace std;

//********************
// static Fonts::allFontInfos
//********************
Fonts::FontInfo Fonts::allFontInfos[] = {{FONT_15_BOLD, 15, FONT_BOLD},
                                         {FONT_20_BOLD, 20, FONT_BOLD},
                                         {FONT_22_MED, 22, FONT_MED},
                                         {FONT_28_BOLD, 28, FONT_BOLD}};

//********************
// Fonts::Fonts
//********************
Fonts::Fonts() = default;

//********************
// Fonts::openNewSharedCachedFont
// low level open shared font.  filename is the full path to the ttf file.  fontSize is the font point size.
//********************
ableem::Font Fonts::openNewSharedCachedFont(const string &filename, int fontSize, ableem::Renderer &renderer) {
    ableem::Font font = ableem::Font::load(renderer, filename, fontSize);
    if (!font.valid()) {
        assert(false);
    }
    return font;
}

//********************
// Fonts::cjkFontFor
//********************
std::string Fonts::cjkFontFor(const std::string &language) {
    if (language.find("Chinese") == std::string::npos)
        return "";
    std::string path = Env::getPathToFontsDir() + sep + "NotoSansSC-Regular.otf";
    return DirEntry::exists(path) ? path : "";
}

//********************
// Fonts::atSize
//********************
ableem::Font &Fonts::atSize(FontType type, int fontSize) {
    std::pair<int, int> key(type, fontSize);
    auto found = bySize.find(key);
    if (found != bySize.end())
        return found->second;
    for (const auto &fontInfo : allFontInfos) { // one of the fixed sizes: share it rather than open it again
        if (fontInfo.fontType == type && fontInfo.size == fontSize && fonts.count(fontInfo.fontEnum))
            return bySize[key] = fonts[fontInfo.fontEnum];
    }
    assert(renderer != nullptr); // openAllFonts() first
    return bySize[key] = openNewSharedCachedFont(type == FONT_MED ? medPath : boldPath, fontSize, *renderer);
}

//********************
// Fonts::openAllFonts
//********************
void Fonts::openAllFonts(const std::string &mediumTtf, const std::string &boldTtf, ableem::Renderer &renderer) {
    fonts.clear();
    bySize.clear();
    this->renderer = &renderer;
    medPath = mediumTtf;
    boldPath = boldTtf;

    for (auto fontInfo : allFontInfos) {
        string path;
        if (fontInfo.fontType == FONT_MED)
            path = medPath;
        else
            path = boldPath;
        fonts[fontInfo.fontEnum] = openNewSharedCachedFont(path, fontInfo.size, renderer);
        fontInfos[fontInfo.fontEnum] = fontInfo;
    }
}

//*******************************
// Fonts::userFontDirs
//*******************************
vector<string> Fonts::userFontDirs(const string &themeDir) {
    return {Env::getPathToRetroarchDir() + sep + "fonts", Env::getPathToFontsDir(), themeDir};
}

//*******************************
// Fonts::userFontPath
//*******************************
string Fonts::userFontPath(const string &themeDir, const string &themeFont, const string &font) {
    if (themeFont == "true" || font.empty() || font == "--")
        return "";
    for (const string &dir : userFontDirs(themeDir)) {
        string path = dir + sep + font;
        if (DirEntry::exists(path))
            return path;
    }
    PLOG_WARNING << "Font " << font << " from config.ini is in none of the font folders - using the theme's";
    return "";
}
