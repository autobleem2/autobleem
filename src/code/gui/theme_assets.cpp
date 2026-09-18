//
// ThemeAssets: the asset half of what Gui used to be.
//
#include "theme_assets.h"
#include "../core/services/environment.h"
#include "../core/main.h"

using namespace std;
using ableem::Texture;

//*******************************
// ThemeAssets::ThemeAssets
//*******************************
ThemeAssets::ThemeAssets(ableem::Renderer &renderer, Theme &theme, Config &config)
    : renderer_(renderer), theme_(theme), config_(config) {
    sonyFonts.openAllFonts(Env::getSonyFontPath() + sep + "SST-Medium.ttf",
                           Env::getSonyFontPath() + sep + "SST-Bold.ttf", renderer_);
}

//*******************************
// ThemeAssets::load
//*******************************
void ThemeAssets::unload() {
    themeFonts.closeAll();
    sonyFonts.closeAll();
    themeFont = ableem::Font();
    backgroundImg = Texture();
    logo = Texture();
    cdJewel = Texture();
    buttonTextureMap.clear();
}

//*******************************
// ThemeAssets::load
//*******************************
void ThemeAssets::load() {
    theme_.load(); // (re)reads theme.json, merged over themes/default, every file resolved
    const ClassicTheme &classic = theme_.classic();
    const LauncherTheme &launcher = theme_.launcher();

    backgroundImg = Texture(); // release the previous theme's textures before loading the new ones

    logoRect.x = classic.logo.x;
    logoRect.y = classic.logo.y;
    logoRect.w = classic.logo.w;
    logoRect.h = classic.logo.h;

    backgroundImg = Texture::loadFile(renderer_, classic.background);
    logo = Texture::loadFile(renderer_, classic.logo.file);
    if (config_.inifile.values["jewel"] != "none") {
        if (config_.inifile.values["jewel"] == "default") {
            cdJewel = Texture::loadFile(renderer_, Env::getWorkingPath() + sep + "evoimg/nofilter.png");
        } else {
            cdJewel = Texture::loadFile(renderer_, Env::getWorkingPath() + sep + "evoimg/frames/" +
                                                       config_.inifile.values["jewel"]);
        }
    } else {
        cdJewel = Texture();
    }

    const auto &b = classic.buttons;
    buttonTextureMap["O"] = Texture::loadFile(renderer_, b.circle);
    buttonTextureMap["X"] = Texture::loadFile(renderer_, b.cross);
    buttonTextureMap["T"] = Texture::loadFile(renderer_, b.triangle);
    buttonTextureMap["S"] = Texture::loadFile(renderer_, b.square);
    buttonTextureMap["Select"] = Texture::loadFile(renderer_, b.select);
    buttonTextureMap["Start"] = Texture::loadFile(renderer_, b.start);
    buttonTextureMap["L1"] = Texture::loadFile(renderer_, b.l1);
    buttonTextureMap["R1"] = Texture::loadFile(renderer_, b.r1);
    buttonTextureMap["L2"] = Texture::loadFile(renderer_, b.l2);
    buttonTextureMap["R2"] = Texture::loadFile(renderer_, b.r2);
    buttonTextureMap["Check"] = Texture::loadFile(renderer_, b.check);
    buttonTextureMap["Uncheck"] = Texture::loadFile(renderer_, b.uncheck);
    buttonTextureMap["Esc"] = Texture::loadFile(renderer_, b.esc);
    buttonTextureMap["Enter"] = Texture::loadFile(renderer_, b.enter);
    buttonTextureMap["Tab"] = Texture::loadFile(renderer_, b.tab);

    // a theme without launcher fonts (and a default theme without them either) gets the console's own
    string classicFont = classic.font.file;
    string medium =
        launcher.fonts.medium.empty() ? Env::getSonyFontPath() + sep + "SST-Medium.ttf" : launcher.fonts.medium;
    string bold = launcher.fonts.bold.empty() ? Env::getSonyFontPath() + sep + "SST-Bold.ttf" : launcher.fonts.bold;
    // ...unless the language needs glyphs no theme font has: then the one CJK font draws everything
    string cjk = Fonts::cjkFontFor(config_.inifile.values["language"]);
    if (!cjk.empty()) {
        PLOG_INFO << "Language " << config_.inifile.values["language"] << ": every font is " << cjk;
        classicFont = medium = bold = cjk;
    }
    themeFont = Fonts::openNewSharedCachedFont(classicFont, classic.font.size, renderer_);
    themeFonts.openAllFonts(medium, bold, renderer_);
}
