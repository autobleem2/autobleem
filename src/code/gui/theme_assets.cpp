//
// ThemeAssets: the asset half of what Gui used to be.
//
#include "theme_assets.h"
#include "gui.h" // Gui::tickBusy, the spinner between the loads
#include "../core/services/environment.h"
#include "../core/main.h"

using namespace std;
using ableem::Texture;

//*******************************
// ThemeAssets::ThemeAssets
//*******************************
ThemeAssets::ThemeAssets(ableem::Renderer &renderer, Theme &theme, Config &config)
    : renderer_(renderer), theme_(theme), config_(config) {}

//*******************************
// ThemeAssets::load
//*******************************
void ThemeAssets::unload() {
    themeFonts.closeAll();
    themeFont = ableem::Font();
    backgroundImg = Texture();
    logo = Texture();
    cdJewel = Texture();
    bigBoxFrame = Texture();
    buttonTextureMap.clear();
    hintCross = hintCircle = hintTriangle = Texture();
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
    Gui::tickBusy();
    // drawn at its own size from the top-left corner (the theme's is the screen's); the splash used to be
    // the only place setting this, which left every program without a splash with no background at all
    ableem::Size backgroundSize = backgroundImg.size();
    backgroundRect = ableem::Rect(0, 0, backgroundSize.w, backgroundSize.h);
    logo = Texture::loadFile(renderer_, classic.logo.file);
    bigBoxFrame = Texture::loadFile(renderer_, Env::getWorkingPath() + sep + "evoimg/bigbox.png");
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

    Gui::tickBusy();
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
    checkIconRightMargin = 0;
    auto rightMargin = [](const string &path, const Texture &texture) {
        ableem::Rect bounds = Texture::opaqueBounds(path);
        ableem::Size whole = texture.size();
        return (bounds.w > 0 && whole.w > 0) ? whole.w - (bounds.x + bounds.w) : 0;
    };
    checkIconRightMargin =
        max(rightMargin(b.check, buttonTextureMap["Check"]), rightMargin(b.uncheck, buttonTextureMap["Uncheck"]));
    buttonTextureMap["Esc"] = Texture::loadFile(renderer_, b.esc);
    buttonTextureMap["Enter"] = Texture::loadFile(renderer_, b.enter);
    buttonTextureMap["Tab"] = Texture::loadFile(renderer_, b.tab);
    hintCross = Texture::loadFile(renderer_, launcher.hints.cross);
    hintCircle = Texture::loadFile(renderer_, launcher.hints.circle);
    hintTriangle = Texture::loadFile(renderer_, launcher.hints.triangle);

    // a theme without launcher fonts (and a default theme without them either) gets the shipped pair -
    // Open Sans Medium/Bold (OFL), the stand-in for the console's SST since 2026-09-21
    string classicFont = classic.font.file;
    // ...the user's own font for the classic screens, when Options says so (the launcher's fonts stay)
    string userFont =
        Fonts::userFontPath(theme_.path(), config_.inifile.values["themefont"], config_.inifile.values["font"]);
    if (!userFont.empty())
        classicFont = userFont;
    string medium =
        launcher.fonts.medium.empty() ? Env::getPathToFontsDir() + sep + "OpenSans-Medium.ttf" : launcher.fonts.medium;
    string bold =
        launcher.fonts.bold.empty() ? Env::getPathToFontsDir() + sep + "OpenSans-Bold.ttf" : launcher.fonts.bold;
    // ...unless the language needs glyphs no theme font has: then the one CJK font draws everything
    string cjk = Fonts::cjkFontFor(config_.inifile.values["language"]);
    if (!cjk.empty()) {
        PLOG_INFO << "Language " << config_.inifile.values["language"] << ": every font is " << cjk;
        classicFont = medium = bold = cjk;
    }
    classicFontFile_ = classicFont;
    Gui::tickBusy();
    themeFont = Fonts::openNewSharedCachedFont(classicFont, classic.font.size, renderer_);
    themeFonts.openAllFonts(medium, bold, renderer_);
    Gui::tickBusy();
}

//*******************************
// ThemeAssets::classicFontAtSize
//*******************************
ableem::Font ThemeAssets::classicFontAtSize(int size) {
    if (classicFontFile_.empty())
        return themeFont;
    return Fonts::openNewSharedCachedFont(classicFontFile_, size, renderer_);
}
