#include "gui_options_menu.h"
#include "../../core/services/system.h"
#include "../../core/services/environment.h"
#include "../../core/services/theme_converter.h"
#include "../../core/services/theme_installer.h"

using namespace std;

string GuiOptions::getStatusLine() {
    auto id = lines[selected].id;
    if (id == CFG_THEME || id == CFG_MUSIC)
        return "|@X| " + _("OK") + "     " + "|@O| " + _("Cancel") + "  " + "|@Start|   " + _("Random") + "|";
    else
        return "|@X| " + _("OK") + "     " + "|@O| " + _("Cancel") + "|";
}

//*******************************
// GuiOptions::getThemes
//*******************************
vector<string> GuiOptions::getThemes() {
    vector<string> list;
    string uiThemePath = Env::getPathToThemesDir();
    ThemeInstaller::installZips(uiThemePath);   // a dropped <name>.zip is listed as <name>
    DirEntries uiThemeFolders = DirEntry::diru_DirsOnly(uiThemePath);
    for (const DirEntry &entry : uiThemeFolders) {
        // a theme.json, or an old-layout folder that Theme::load() will convert when it is picked
        if (ThemeConverter::isThemeFolder(uiThemePath + sep + entry.name)) {
            list.push_back(entry.name); // add the theme dir name
        }
    }

    return list;
}

//*******************************
// GuiOptions::getJewels
//*******************************
vector<string> GuiOptions::getJewels() {
    vector<string> list;
    DirEntries folders = DirEntry::diru_FilesOnly(Env::getWorkingPath() + sep + "evoimg/frames");
    for (const DirEntry & entry : folders) {
        if (DirEntry::getFileExtension(entry.name) == "png") {
            list.push_back(entry.name);
        }
    }

    return list;
}

//*******************************
// GuiOptions::getMusic
//*******************************
vector<string> GuiOptions::getMusic() {
    vector<string> list;
    list.push_back("--");
    DirEntries folders = DirEntry::diru_FilesOnly(Env::getWorkingPath() + sep + "music");
    for (const DirEntry & entry:folders) {
        if (DirEntry::getFileExtension(entry.name) == "ogg") {
            list.push_back(entry.name);
        }
    }

    return list;
}

//*******************************
// GuiOptions::getTimeoutValues
//*******************************
vector<string> GuiOptions::getTimeoutValues() {
    vector<string> list;
    for (int i=0; i <= 20; ++i) {
        list.push_back(to_string(i));
    }

    return list;
}

//*******************************
// GuiOptions::fill
//*******************************
void GuiOptions::fill() {
    // this is filled once and not on every render.
    // save the current lang and switch to English.  we need the "Prefix:" to be scanned in English for English.txt
    // getLineText() will do the translation
    string saveCurrentLang = app.lang().currentLanguage();
    app.lang().load(Env::getPathToLangDir(), "English");

    lines.emplace_back(CFG_THEME, _("AutoBleem Theme:"), "theme", false, getThemes());
#ifndef AB_PLATFORM_RPI
    // a Pi has no built-in games to show (GameQueryService::showInternalGames is hard false there)
    lines.emplace_back(CFG_SHOW_ORIGAMES, _("Show Internal Games:"), "origames", true, vector<string> ({ "false", "true" }) );
#endif
    lines.emplace_back(CFG_JEWEL, _("Cover Style:"), "jewel", false, getJewels());
    lines.emplace_back(CFG_MUSIC, _("Music:"), "music", false, getMusic());
    lines.emplace_back(CFG_ENABLE_BACKGROUND_MUSIC, _("Background Music:"), "nomusic", true, vector<string> ({ "true", "false" }) );
    lines.emplace_back(CFG_WIDESCREEN, _("Widescreen:"), "aspect", true, vector<string> ({ "false", "true" }) );
    lines.emplace_back(CFG_GFX_FILTER, _("GFX Filter:"), "mip", true, vector<string> ({ "true", "false" }) );
    lines.emplace_back(CFG_RACONFIG, _("Update RA Config:"), "raconfig", true, vector<string> ({ "false", "true" }) );
    lines.emplace_back(CFG_PLAY_ALL_PSX_WITH_RA, _("Play all PSX games with RA:"), "play_all_psx_with_ra", true, vector<string> ({ "false", "true" }) );
    lines.emplace_back(CFG_SHOWINGTIMEOUT, _("Showing Timeout (0 for no timeout):"), "showingtimeout", false, getTimeoutValues());
    lines.emplace_back(CFG_LANG, _("Language:"), "language", false, Lang::listLanguages(Env::getPathToLangDir()));

    app.lang().load(Env::getPathToLangDir(), saveCurrentLang);
}

//*******************************
// GuiOptions::init
//*******************************
void GuiOptions::init() {
    GuiOptionsMenuBase::init(); // call the base class init()
    lines.clear();
    fill();
}

//*******************************
// void GuiOptions::getLineText
//*******************************
std::string GuiOptions::getLineText(const OptionsInfo& info) {
    std::string temp = app.lang().translate(info.descriptionToTranslate) + " ";
    auto value = app.config().inifile.values[info.iniKey];
    if (info.keyIsBoolean) {
        temp += getBooleanSymbolText(info, value);
    } else {
        temp += value;  // append the current text value in the options list
    }

    return temp;
}

//*******************************
// GuiOptions::doPrevNextOption
//*******************************
string GuiOptions::doPrevNextOption(OptionsInfo& info, bool next) {
    int id = info.id;

    // do the default action
    string nextValue = GuiOptionsMenuBase::doPrevNextOption(info, next);

    // after doing the default these need special action afterwards
    if (id == CFG_THEME) {
        gui->loadAssets();
        font = gui->assets().themeFont;  // get the new font for the menu
    } else if (id == CFG_LANG) {
        app.lang().load(Env::getPathToLangDir(), nextValue);
        gui->loadAssets(false);            // the fonts may change with the language (Chinese)
        font = gui->assets().themeFont;
    } else if (id == CFG_MUSIC || id == CFG_ENABLE_BACKGROUND_MUSIC) {
        gui->loadAssets();
    }

    return nextValue;
}

//*******************************
// string GuiOptions::doRandomOption()
// only a few lines will use this.  most will just return.
//*******************************
string GuiOptions::doRandomOption() {
    int id = lines[selected].id;
    if (id == CFG_THEME || id == CFG_MUSIC) {
        auto & choices = lines[selected].choices;
        unsigned int size = choices.size();
        if (size > 1)
            return doOptionIndex(System::getRandomIndex(size));
    }
    return "";
}

//*******************************
// string GuiOptions::doOptionIndex()
//*******************************
string GuiOptions::doOptionIndex(unsigned int index) {
    if (validSelectedIndex()) {
        int id = lines[selected].id;
        // do the default action
        string nextValue = GuiOptionsMenuBase::doOptionIndex(index);

        // after doing the default these need special action afterwards
        if (id == CFG_THEME) {
            gui->loadAssets();
            font = gui->assets().themeFont;  // get the new font for the menu
        } else if (id == CFG_LANG) {
            app.lang().load(Env::getPathToLangDir(), nextValue);
            gui->loadAssets(false);
            font = gui->assets().themeFont;
        } else if (id == CFG_MUSIC || id == CFG_ENABLE_BACKGROUND_MUSIC) {
            gui->loadAssets();
        }

        return nextValue;
    } else
        return "";
}

//*******************************
// GuiOptions::doCircle_Pressed
//*******************************
void GuiOptions::doCircle_Pressed() {
    app.audio().cancel.play();
    string cfg_path = Env::getWorkingPath() + sep + "config.ini";
    app.config().inifile.load(cfg_path);    // restore the original config.ini settings
    app.lang().load(Env::getPathToLangDir(), app.config().inifile.values["language"]);    // restore the original lang
    gui->loadAssets();                                  // restore original themes
    menuVisible = false;
    exitCode = -1;
}

//*******************************
// GuiOptions::doCross_Pressed
//*******************************
void GuiOptions::doCross_Pressed() {
    app.audio().cancel.play();
    app.config().save();
    menuVisible = false;
    exitCode = 0;
}

//*******************************
// GuiOptions::doJoyRight
//*******************************
void GuiOptions::doJoyRight() {
    do {
        doKeyRight();
        render();
    } while (fastForwardUntilAnotherEvent());
}

//*******************************
// GuiOptions::doJoyLeft
//*******************************
void GuiOptions::doJoyLeft() {
    do {
        doKeyLeft();
        render();
    } while (fastForwardUntilAnotherEvent());
}

//*******************************
// GuiOptions::doKeyRight
//*******************************
void GuiOptions::doKeyRight() {
    app.audio().cursor.play();
    doPrevNextOption(true);
}

//*******************************
// GuiOptions::doKeyLeft
//*******************************
void GuiOptions::doKeyLeft() {
    app.audio().cursor.play();
    doPrevNextOption(false);
}
