//
// ThemeAssets: the asset half of what Gui used to be.
//
#include "theme_assets.h"
#include "../core/environment.h"
#include "../core/main.h"

using namespace std;
using ableem::Texture;

//*******************************
// ThemeAssets::ThemeAssets
//*******************************
ThemeAssets::ThemeAssets(ableem::Renderer &renderer, Theme &theme, Config &config)
    : renderer_(renderer), theme_(theme), config_(config) {
    sonyFonts.openAllFonts(Env::getSonyFontPath(), renderer_);
    themeFonts.openAllFonts(theme_.fontPath(), renderer_);
}

//*******************************
// ThemeAssets::loadThemeTexture
//*******************************
Texture ThemeAssets::loadThemeTexture(const string &themePath, const string &defaultPath, const string &texname) {
    Texture tex;
    if (DirEntry::exists(themePath + theme_.data.values[texname])) {
        tex = Texture::loadFile(renderer_, themePath + theme_.data.values[texname]);
    } else {
        tex = Texture::loadFile(renderer_, defaultPath + theme_.defaults.values[texname]);
    }
    return tex;
}

//*******************************
// ThemeAssets::load
//*******************************
void ThemeAssets::load() {
    theme_.load();     // (re)reads theme.ini, falling back to themes/default
    const string themePath = theme_.loadedPath();
    const string defaultPath = theme_.defaultsPath();

    backgroundImg = Texture();  // release the previous theme's textures before loading the new ones

    logoRect.x = atoi(theme_.data.values["lpositionx"].c_str());
    logoRect.y = atoi(theme_.data.values["lpositiony"].c_str());
    logoRect.w = atoi(theme_.data.values["lw"].c_str());
    logoRect.h = atoi(theme_.data.values["lh"].c_str());

    backgroundImg = loadThemeTexture(themePath, defaultPath, "background");
    logo = loadThemeTexture(themePath, defaultPath, "logo");
    if (config_.inifile.values["jewel"] != "none") {
        if (config_.inifile.values["jewel"] == "default") {
            cdJewel = Texture::loadFile(renderer_, Env::getWorkingPath() + sep + "evoimg/nofilter.png");
        } else {
            cdJewel = Texture::loadFile(renderer_,
                                        Env::getWorkingPath() + sep + "evoimg/frames/" +
                                        config_.inifile.values["jewel"]);
        }
    } else {
        cdJewel = Texture();
    }

    buttonTextureMap["O"] = loadThemeTexture(themePath, defaultPath, "circle");
    buttonTextureMap["X"] = loadThemeTexture(themePath, defaultPath, "cross");
    buttonTextureMap["T"] = loadThemeTexture(themePath, defaultPath, "triangle");
    buttonTextureMap["S"] = loadThemeTexture(themePath, defaultPath, "square");
    buttonTextureMap["Select"] = loadThemeTexture(themePath, defaultPath, "select");
    buttonTextureMap["Start"] = loadThemeTexture(themePath, defaultPath, "start");
    buttonTextureMap["L1"] = loadThemeTexture(themePath, defaultPath, "l1");
    buttonTextureMap["R1"] = loadThemeTexture(themePath, defaultPath, "r1");
    buttonTextureMap["L2"] = loadThemeTexture(themePath, defaultPath, "l2");
    buttonTextureMap["R2"] = loadThemeTexture(themePath, defaultPath, "r2");
    buttonTextureMap["Check"] = loadThemeTexture(themePath, defaultPath, "check");
    buttonTextureMap["Uncheck"] = loadThemeTexture(themePath, defaultPath, "uncheck");
    buttonTextureMap["Esc"] = loadThemeTexture(themePath, defaultPath, "esc");
    buttonTextureMap["Enter"] = loadThemeTexture(themePath, defaultPath, "enter");
    buttonTextureMap["Tab"] = loadThemeTexture(themePath, defaultPath, "tab");

    string fontPath = (themePath + theme_.data.values["font"]);
    int fontSize = 0;
    string fontSizeString = theme_.data.values["fsize"];
    if (fontSizeString != "")
        fontSize = atoi(fontSizeString.c_str());
    themeFont = Fonts::openNewSharedCachedFont(fontPath, fontSize, renderer_);
}
