#include "gui_font.h"
#include <iostream>
#include "../util.h"
#include <cassert>
#include "../main.h"
#include "gui.h"
#include "../app.h"

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
// Fonts::openSpecificSharedCachedFont
// low level open shared font.  filename is the full path to the ttf file.  fontSize is the font point size.
//********************
ableem::Font Fonts::openSpecificSharedCachedFont(FontType type, int fontSize) {
    auto gui = Gui::getInstance();

    string rootPath = App::get().theme().fontPath();
    string fontPath;
    if (type == FONT_MED)
        fontPath = rootPath + sep + "SST-Medium.ttf";
    else
        fontPath = rootPath + sep + "SST-Bold.ttf";

    return openNewSharedCachedFont(fontPath, fontSize, gui->renderer());
}

//********************
// Fonts::openAllFonts
//********************
void Fonts::openAllFonts(const std::string &_rootPath, ableem::Renderer &renderer) {
    fonts.clear();
    rootPath = _rootPath;
    medPath = rootPath + sep + "SST-Medium.ttf";
    boldPath = rootPath + sep + "SST-Bold.ttf";

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
