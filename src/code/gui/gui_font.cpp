#include "gui_font.h"
#include <iostream>
#include "../core/services/system.h"
#include <cassert>
#include "../core/main.h"

using namespace std;

//********************
// static Fonts::allFontInfos
//********************
Fonts::FontInfo Fonts::allFontInfos[] = {
        { FONT_15_BOLD, 15, FONT_BOLD },
        { FONT_20_BOLD, 20, FONT_BOLD},
        { FONT_22_MED,  22, FONT_MED },
        { FONT_28_BOLD, 28, FONT_BOLD }
};

//********************
// Fonts::Fonts
//********************
Fonts::Fonts() { }

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
// Fonts::boldAtSize
//********************
ableem::Font &Fonts::boldAtSize(int fontSize) {
    auto found = boldBySize.find(fontSize);
    if (found != boldBySize.end())
        return found->second;
    for (const auto &fontInfo : allFontInfos) {   // one of the fixed sizes: share it rather than open it again
        if (fontInfo.fontType == FONT_BOLD && fontInfo.size == fontSize && fonts.count(fontInfo.fontEnum))
            return boldBySize[fontSize] = fonts[fontInfo.fontEnum];
    }
    assert(renderer != nullptr);   // openAllFonts() first
    return boldBySize[fontSize] = openNewSharedCachedFont(boldPath, fontSize, *renderer);
}

//********************
// Fonts::openAllFonts
//********************
void Fonts::openAllFonts(const std::string &mediumTtf, const std::string &boldTtf, ableem::Renderer &renderer) {
    fonts.clear();
    boldBySize.clear();
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
