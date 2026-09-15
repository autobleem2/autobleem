//
// Created by screemer on 2018-12-19.
//

#include "gui.h"
#include "gui_about.h"
#include "gui_splash.h"
#include "menus/gui_optionsMenu.h"
#include "menus/gui_memCardsMenu.h"
#include "menus/gui_gameManagerMenu.h"
#include "gui_confirm.h"
#include "../ver_migration.h"
#include "../launcher/gui_launcher.h"
#include "gui_padTest.h"
#include "../lang.h"
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <json.h>
#include "../nlohmann/fifo_map.h"

using namespace std;
using namespace nlohmann;
using ableem::Rect;
using ableem::Size;
using ableem::Color;
using ableem::Texture;
using ableem::Event;
using ableem::Button;

// A workaround to give to use fifo_map as map, we are just ignoring the 'less' compare
template<class K, class V, class dummy_compare, class A>
using my_workaround_fifo_map = fifo_map<K, V, fifo_map_compare<K>, A>;
using ordered_json = basic_json<my_workaround_fifo_map>;


#define RA_PLAYLIST "AutoBleem.lpl"


//********************
// Gui::Gui
//********************
Gui::Gui() {
    sonyFonts.openAllFonts(Env::getSonyFontPath(), renderer());
    themeFonts.openAllFonts(getCurrentThemeFontPath(), renderer());
    input().probePads();
}

//*******************************
// Gui::getCurrentThemePath
//*******************************
string Gui::getCurrentThemePath() {
#ifdef AB_DEBUG_HOST
    string path = Env::getPathToThemesDir() + sep + cfg.inifile.values["theme"];
    if (!DirEntry::exists(path)) {
        path = Env::getSonyPath();
    }
    return path;
#else
    string path =  "/media/themes/" + cfg.inifile.values["theme"] + "";
    if (!DirEntry::exists(path))
    {
        path = "/usr/sony/share/data";
    }
    return path;
#endif
}

//*******************************
// Gui::getCurrentThemeImagePath
//*******************************
string Gui::getCurrentThemeImagePath() {
#ifdef AB_DEBUG_HOST
    string path = getCurrentThemePath() + sep + "images";
    if (!DirEntry::exists(path)) {
        path = Env::getSonyPath() + sep + "images";
    }
    return path;
#else
    string path =  "/media/themes/" + cfg.inifile.values["theme"] + "/images";
    if (!DirEntry::exists(path))
    {
        path = "/usr/sony/share/data/images";
    }
    return path;
#endif
}

//*******************************
// Gui::getCurrentThemeSoundPath
//*******************************
string Gui::getCurrentThemeSoundPath() {
#ifdef AB_DEBUG_HOST
    string path = getCurrentThemePath() + sep + "sounds";
    if (!DirEntry::exists(path)) {
        path = Env::getSonyPath() + sep + "sounds";
    }
    cout << path << endl;
    return path;
#else
    string path =  "/media/themes/" + cfg.inifile.values["theme"] + "/sounds";
    if (!DirEntry::exists(path))
    {
        path = "/usr/sony/share/data/sounds";
    }
    return path;
#endif
}

//*******************************
// Gui::getCurrentThemeFontPath
//*******************************
string Gui::getCurrentThemeFontPath() {
#ifdef AB_DEBUG_HOST
    string path = getCurrentThemePath() + sep + "font";
    if (!DirEntry::exists(path)) {
        path = Env::getSonyPath() + sep + "font";
    }
    return path;
#else
    string path =  "/media/themes/" + cfg.inifile.values["theme"] + "/font";
    if (!DirEntry::exists(path))
    {
        path = "/usr/sony/share/data/font";
    }
    return path;
#endif
}


//*******************************
// Gui::splash
//*******************************
void Gui::splash(const string &message) {
    shared_ptr<Gui> gui(Gui::getInstance());
    gui->drawText(message);
}

extern "C"
{
//*******************************
// Gui::splash
//*******************************
void splash(char *message) {
    shared_ptr<Gui> gui(Gui::getInstance());
    gui->drawText(message);
}
}

//*******************************
// Gui::getR
//*******************************
unsigned char Gui::getR(const string &val) {
    return atoi(Util::commaSep(val, 0).c_str());
}

//*******************************
// Gui::getG
//*******************************
unsigned char Gui::getG(const string &val) {
    return atoi(Util::commaSep(val, 1).c_str());
}

//*******************************
// Gui::getB
//*******************************
unsigned char Gui::getB(const string &val) {
    return atoi(Util::commaSep(val, 2).c_str());
}

void Gui::stopAudio() {
    audio().close();
}

void Gui::restartAudio(int freq) {
    audio().open(freq, 2, 1024);
}

void Gui::playMusic(bool customMusic, string musicPath) {
    if (cfg.inifile.values["nomusic"] != "true")
        if (themeData.values["loop"] != "-1") {
            if (!customMusic) {
                music = ableem::Music::load(themePath + themeData.values["music"]);
                music.play(themeData.values["loop"] == "1" ? -1 : 0);
            } else {
                music = ableem::Music::load(Env::getWorkingPath() + sep + "music/" + musicPath);
                music.play(-1);
            }
        }
}

void Gui::freeMusic() {
    music = ableem::Music();
}
//*******************************
// Gui::loadThemeTexture
//*******************************
Texture
Gui::loadThemeTexture(const string& themePath, const string& defaultPath, const string& texname) {
    Texture tex;
    if (DirEntry::exists(themePath + themeData.values[texname])) {
        tex = Texture::loadFile(renderer(), themePath + themeData.values[texname]);
    } else {
        tex = Texture::loadFile(renderer(), defaultPath + defaultData.values[texname]);
    }
    return tex;
}


//*******************************
// Gui::loadAssets
//*******************************
void Gui::loadAssets(bool reloadMusic) {
    // check theme exists - otherwise back to aergb

    string defaultPath = Env::getPathToThemesDir() + sep + "default" + sep;
    themePath = getCurrentThemePath() + sep;

    cout << "Loading UI theme:" << themePath << endl;
    if (!DirEntry::exists(themePath + "theme.ini"))
    {
        themePath=defaultPath;
        cfg.inifile.values["theme"] = "default";
        cfg.save();
    }

    defaultData.load(defaultPath + "theme.ini");
    themeData.load(defaultPath + "theme.ini");
    themeData.OverwriteAndAppend(themePath + "theme.ini");    // adds to default/theme.ini values

    backgroundImg = Texture();  // release the previous theme's textures/sounds before loading the new ones
    cursor = ableem::Sound();
    cancel = ableem::Sound();
    home_down = ableem::Sound();
    home_up = ableem::Sound();

    logoRect.x = atoi(themeData.values["lpositionx"].c_str());
    logoRect.y = atoi(themeData.values["lpositiony"].c_str());
    logoRect.w = atoi(themeData.values["lw"].c_str());
    logoRect.h = atoi(themeData.values["lh"].c_str());

    backgroundImg = loadThemeTexture(themePath, defaultPath, "background");
    logo = loadThemeTexture(themePath, defaultPath, "logo");
    if (cfg.inifile.values["jewel"] != "none") {
        if (cfg.inifile.values["jewel"] == "default") {
            cdJewel = Texture::loadFile(renderer(), Env::getWorkingPath() + sep + "evoimg/nofilter.png");
        } else {
            cdJewel = Texture::loadFile(renderer(),
                                        Env::getWorkingPath() + sep + "evoimg/frames/" +
                                        cfg.inifile.values["jewel"]);
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

    string fontPath = (themePath + themeData.values["font"]);
    int fontSize = 0;
    string fontSizeString = themeData.values["fsize"];
    if (fontSizeString != "")
        fontSize = atoi(fontSizeString.c_str());
    themeFont = Fonts::openNewSharedCachedFont(fontPath, fontSize, renderer());

    if (reloadMusic) {
        freeMusic();
    }
    customMusic = false;
    freq = 32000;
    musicPath = themeData.values["music"];
    if (cfg.inifile.values["music"] != "--") {
        customMusic = true;
        musicPath = cfg.inifile.values["music"];
    }

    if (DirEntry::getFileExtension(musicPath) == "ogg") {
        freq = 44100;
    }

    if (reloadMusic) {
        this->restartAudio(freq);
        this->playMusic(customMusic, musicPath);
    }
    cursor = ableem::Sound::load(this->getCurrentThemeSoundPath() + sep + "cursor.wav");
    cancel = ableem::Sound::load(this->getCurrentThemeSoundPath() + sep + "cancel.wav");
    home_up = ableem::Sound::load(this->getCurrentThemeSoundPath() + sep + "home_up.wav");
    home_down = ableem::Sound::load(this->getCurrentThemeSoundPath() + sep + "home_down.wav");
    resume = ableem::Sound::load(this->getCurrentThemeSoundPath() + sep + "resume_new.wav");
}

//*******************************
// Gui::hideMouseCursor
//*******************************
void Gui::hideMouseCursor() {
    if (!platform().isDevHost()) {
        platform().hideAndGrabCursor();
    }
}

//*******************************
// Gui::criticalException
//*******************************
void Gui::criticalException(const string &text) {
    drawText(text);
    while (true) {
        Event e;
        while (input().poll(e)) {
            if (e.type == Event::Type::Quit)
                return;
            else if (e.type == Event::Type::KeyUp && e.key == ableem::Key::Escape)
                return;

            if (e.type == Event::Type::ButtonDown) {
                return;
            }
        }
    }
}

//*******************************
// Gui::display
//*******************************
void Gui::display(bool forceScan, const string &_pathToGamesDir, Database *db, bool resume) {
    this->db = db;
    this->pathToGamesDir = _pathToGamesDir;
    this->forceScan = forceScan;

    cout << platform().versionString() << endl;

    platform().setScaleQuality(2);

    loadAssets();

    if (!resume) {
        GuiSplash splashScreen(*this);
        splashScreen.show();
        hideMouseCursor();
    } else {
        resumingGui = true;
    }
}

//*******************************
// Gui::saveSelection
//*******************************
void Gui::saveSelection() {
    ofstream os;
    string path = cfg.inifile.values["cfg"];
    os.open(path);
    if (!DirEntry::checkWritable(os, path)) return;   // the rc scripts then keep the previous selection
    os << "#!/bin/sh" << endl << endl;
    os << "AB_SELECTION=" << menuOption << endl;
    os << "AB_THEME=" << cfg.inifile.values["theme"] << endl;
    os << "AB_PCSX=" << cfg.inifile.values["pcsx"] << endl;
    os << "AB_MIP=" << cfg.inifile.values["mip"] << endl;

    os.flush();
    os.close();
}

bool otherMenuShift = false;
bool powerOffShift = false;


//*******************************
// Gui::menuSelection
//*******************************
void Gui::menuSelection() {
    shared_ptr<Scanner> scanner(Scanner::getInstance());


    if (!coverdb->isValid()) {
        criticalException(_("WARNING: NO COVER DB FOUND. PRESS ANY BUTTON."));
    }
    otherMenuShift = false;
    powerOffShift = false;
    string mainMenu = "|@Start| " + _("AutoBleem") + "    |@X|  " + _("Re/Scan") + " ";
    if (cfg.inifile.values["ui"] == "classic") {
        mainMenu += "  |@O|  " + _("Original") + "  ";
    }
    string RA_or_EA = _("RetroArch");
    string cfgPath = Env::getPathToRetroarchDir() + sep + "retroboot/retroboot.cfg";
    if (DirEntry::exists(cfgPath)) {
        Inifile RBcfg;
        RBcfg.load(cfgPath);
        if (RBcfg.values["use_emulationstation"] == "1")
            RA_or_EA = _("EmulationStation");
    }
    mainMenu += "|@S|  " + RA_or_EA + "   ";
    mainMenu += "|@T|  " + _("About") + "  |@Select|  " + _("Options") + " ";
    mainMenu += "|@L1| " + _("Advanced");
    mainMenu += " |@L2|+|@R2|" + _("Power Off");

    string forceScanMenu = _("Games changed. Press") + "  |@X|  " + _("to scan") + "|";
    string otherMenu;

    otherMenu += "|@S|  " + _("Hardware Information") + "  ";
    otherMenu += "|@X|  " + _("Memory Cards") + "   |@O|  " + _("Game Manager");


    string gamepadNotice = "";
    if (input().joystickCount() > input().activePadCount()) {
        gamepadNotice = _(
                "NOTICE: At least one connected gamepad is not recognized. Use Hardware Information page to setup.");
    }

    if (!forceScan) {
        drawText(mainMenu, gamepadNotice);

    } else {
        drawText(forceScanMenu, gamepadNotice);
    }


    bool menuVisible = true;
    while (menuVisible) {
        if (startingGame) {
            drawText(runningGame->title);
            this->menuOption = MENU_OPTION_START;
            menuVisible = false;
            startingGame = false;
            return;
        }

        if (resumingGui) {
            {   // scoped: the screen must be gone before menuSelection() recurses
                GuiLauncher launcherScreen(*this);
                launcherScreen.show();
            }
            drawText("");
            resumingGui = false;
            menuSelection();
            menuVisible = false;
        }
        Event e;
        while (input().poll(e)) {
            // this is for pc Only
            if (e.type == Event::Type::Quit) {
                menuVisible = false;
            }
            switch (e.type) {

                case Event::Type::ButtonUp:
                    if (!forceScan) {
                        if (e.button == Button::L1) {
                            cursor.play();
                            drawText(mainMenu);
                            otherMenuShift = false;
                        }
                        if (e.button == Button::L2) {
                            cursor.play();
                            powerOffShift = false;
                        }
                    }
                    break;
                case Event::Type::ButtonDown:
                    if (!forceScan) {
                        if (e.button == Button::L1) {
                            cursor.play();
                            drawText(otherMenu);
                            otherMenuShift = true;
                        }
                        if (e.button == Button::L2) {
                            cursor.play();
                            powerOffShift = true;
                        }
                    }

                    if (powerOffShift) {
                        if (e.button == Button::R2) {
                            cursor.play();
                            drawText(_("POWERING OFF... PLEASE WAIT"));
#ifdef AB_DEBUG_HOST
                            exit(0);
#else
                            Util::execUnixCommand("shutdown -h now");
                                    sync();
                                    exit(1);
#endif
                        };
                    }

                    if (!otherMenuShift) {
                        if (!forceScan)
                            if (e.button == Button::Start) {
                                if (cfg.inifile.values["ui"] == "classic") {
                                    cursor.play();
                                    this->menuOption = MENU_OPTION_RUN;
                                    menuVisible = false;
                                } else {
                                    if (lastSet < 0) {
                                        lastSet = SET_PS1;
                                        lastSelIndex = 0;
                                        resumingGui = false;
                                    }
                                    cursor.play();
                                    drawText(_("Starting EvolutionUI"));
                                    loadAssets(false);
                                    {   // scoped: the screen must be gone before menuSelection() recurses
                                        GuiLauncher launcherScreen(*this);
                                        launcherScreen.show();
                                    }

                                    menuSelection();
                                    menuVisible = false;
                                }
                            };

                        if (!forceScan)
                            if (e.button == Button::Square) {
                                cursor.play();
                                if (!DirEntry::exists(Env::getPathToRetroarchDir() + sep + "retroarch")) {

                                    bool result;
                                    {   // scoped: the screen must be gone before menuSelection() recurses
                                        GuiConfirm confirm(*this);
                                        confirm.label = _("RetroArch is not installed");
                                        confirm.show();
                                        result = confirm.result;
                                    }
                                    if (result) {
                                        this->menuOption = MENU_OPTION_RETRO;
                                        menuVisible = false;
                                    } else {
                                        menuSelection();
                                        menuVisible = false;
                                    }
                                } else {
                                    exportDBToRetroarch();
                                    this->menuOption = MENU_OPTION_RETRO;
                                    menuVisible = false;
                                }
                            };

                        if (e.button == Button::Cross) {
                            cursor.play();
                            this->menuOption = MENU_OPTION_SCAN;

                            menuVisible = false;
                        };
                        if (e.button == Button::Triangle) {
                            cursor.play();
                            {   // scoped: the screen must be gone before menuSelection() recurses
                                GuiAbout aboutScreen(*this);
                                aboutScreen.show();
                            }

                            menuSelection();
                            menuVisible = false;
                        };
                        if (e.button == Button::Select) {
                            cursor.play();
                            {   // scoped: the screen must be gone before menuSelection() recurses
                                GuiOptions options(*this);
                                options.show();
                            }
                            menuSelection();
                            menuVisible = false;
                        };
                        if (!forceScan)
                            if (cfg.inifile.values["ui"] == "classic")
                                if (e.button == Button::Circle) {
                                    cancel.play();
                                    this->menuOption = MENU_OPTION_SONY;
                                    menuVisible = false;
                                };
                        break;
                    } else {
                        if (e.button == Button::Square) {
                            cursor.play();
                            stopAudio();
                            input().flushPads();
#ifdef AB_DEBUG_HOST
                            drawText("Small delay to test");
                            platform().delay(2000);
#endif
                            string cmd = Env::getPathToAppsDir() + sep + "pscbios/run.sh";
                            Util::runAndWait(cmd, {});
                            input().flushEvents();
                            input().probePads();
                            restartAudio(freq);
                            playMusic(customMusic,musicPath);
                            menuSelection();
                            menuVisible = false;
                        };

                        if (e.button == Button::Cross) {
                            cursor.play();
                            {   // scoped: the screen must be gone before menuSelection() recurses
                                GuiMemcards memcardsScreen(*this);
                                memcardsScreen.show();
                            }

                            menuSelection();
                            menuVisible = false;
                        };

                        if (e.button == Button::Circle) {
                            cursor.play();
                            {   // scoped: the screen must be gone before menuSelection() recurses
                                GuiManager managerScreen(*this);
                                managerScreen.show();
                            }

                            menuSelection();
                            menuVisible = false;
                        };
                    }
                    break;
                default:
                    break;
            }
        }
    }
}

//*******************************
// Gui::finish
//*******************************
void Gui::finish() {

    if (music.isPlaying()) {
        music.fadeOut(300);
        while (music.isPlaying()) {
        }
    } else {
        usleep(300 * TicksPerSecond);
    }

    music.halt();
    music = ableem::Music();
    cursor = ableem::Sound();
    cancel = ableem::Sound();
    home_down = ableem::Sound();
    home_up = ableem::Sound();
    audio().close();
    backgroundImg = Texture();
}

//*******************************
// Gui::exportDBToRetroarch
//*******************************
void Gui::exportDBToRetroarch() {
    ordered_json j;
    j["version"]="1.0";

    PsGames gamesList;
    db->getGames(&gamesList);
    sort(gamesList.begin(), gamesList.end(), sortByTitle);

    ordered_json items = ordered_json::array();
    // copy the gamesList into json object
    for_each(begin(gamesList), end(gamesList), [&](PsGamePtr &game)
    {
        ordered_json item = ordered_json::object();

        string gameFile = (game->folder + sep + game->base);
        if (!DirEntry::matchExtension(game->base, ".pbp")) {
            gameFile += ".cue";
        }
        gameFile += "";

        string base;
        if (DirEntry::isPBPFile(game->base)) {
            base = game->base.substr(0, game->base.length() - 4);
        } else {
            base = game->base;
        }
        if (DirEntry::exists(game->folder + sep + base + ".m3u")) {
            gameFile = game->folder + sep + base + ".m3u";
        }


        item["path"]=gameFile;
        item["label"]=game->title;
        item["core_path"]=Env::getPathToRetroarchCoreFile();
        item["core_name"]="DETECT";
        item["crc32"]="00000000|crc";
        item["db_name"]=RA_PLAYLIST;

        items.push_back(item);
    });

    j["items"] = items;

    cout << j.dump() << endl;
    string playlistPath = Env::getPathToRetroarchDir() + sep + "playlists/" + RA_PLAYLIST;
    std::ofstream o(playlistPath);
    if (!DirEntry::checkWritable(o, playlistPath)) return;
    o << std::setw(2) << j << std::endl;
    o.flush();
    o.close();
}

//*******************************
// Rect and Size routines
//*******************************

//*******************************
// Gui::getFontTextSize
// return the w of the rendered text and h of the font
//*******************************
Size Gui::getFontTextSize(const ableem::Font &font, const char *text) {
    Size size;
    if (!font.valid()) {
        size.w = 0;
        size.h = 0;
        return size;
    }
    if (text == nullptr || strlen(text) == 0)
        size.w = 0;
    else
        size.w = font.width(text);
    size.h = font.lineHeight();

    return size;
}

//*******************************
// Gui::getFontTextRect
// return a rect at (x,y) sized to the rendered text
//*******************************
Rect Gui::getFontTextRect(const ableem::Font &font, const char *text, int x, int y) {
    Size size = getFontTextSize(font, text);
    Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = size.w;
    rect.h = size.h;

    return rect;

}

//*******************************
// Gui::getOpscreenRectOfTheme
//*******************************
Rect Gui::getOpscreenRectOfTheme() {
    Rect rect;
    rect.x = atoi(themeData.values["opscreenx"].c_str());
    rect.y = atoi(themeData.values["opscreeny"].c_str());
    rect.w = atoi(themeData.values["opscreenw"].c_str());
    rect.h = atoi(themeData.values["opscreenh"].c_str());

    return rect;
}

//*******************************
// Gui::getTextRectOfTheme
//*******************************
Rect Gui::getTextRectOfTheme() {
    Rect rect;
    rect.x = atoi(themeData.values["textx"].c_str());
    rect.y = atoi(themeData.values["texty"].c_str());
    rect.w = atoi(themeData.values["textw"].c_str());
    rect.h = atoi(themeData.values["texth"].c_str());

    return rect;
}

//*******************************
// Gui::getCheckIconWidth
// returns the width of the check/uncheck icon textures
//*******************************
int Gui::getCheckIconWidth() {
    auto it = buttonTextureMap.find("Check");
    if (it != buttonTextureMap.end()) {
        return it->second.size().w;
    } else {
        cout << "missing check icon" << endl;
        assert(false);
    }

    return 0;
}

//*******************************
// Gui::align_xPosition
//*******************************
int Gui::align_xPosition(XAlignment xAlign, int x, int width) {
    if (xAlign == XALIGN_CENTER) {
        x = (SCREEN_WIDTH / 2) - width / 2;
    } else if (xAlign == XALIGN_RIGHT) {
        x = SCREEN_WIDTH - x - width;
    }

    return x;
}

//*******************************
// Gui::AllTextOrEmojiTokenInfo::compute_xy_relativeOffsets
// compute x offset, center the y offset of each token to the total height
//*******************************
void Gui::AllTextOrEmojiTokenInfo::compute_xy_relativeOffsets() {
    int xOffset = 0;
    for (auto& info : tokenInfos) {
        x = xOffset;
        xOffset += info.rect.w;
        info.rect.y = (totalSize.h - info.rect.h) / 2;
    }
}

//*******************************
// Gui::AllTextOrEmojiTokenInfo::getTokenInfo
// break up the text into tokens of pure text or an emoji icon marker
// return a vector of the text, emoji texture pointers, width and height of each token and the total width and height.
//*******************************
void Gui::AllTextOrEmojiTokenInfo::getTokenInfo(ableem::Font _font, const string & _text) {
    auto gui = Gui::getInstance();
    font = _font;
    if (!font.valid())
        font = gui->themeFont;   // if font is invalid, default to themeFont

    //
    // break up the text into tokens of text and emoji markers
    //
    string text = _text;
    if (text.empty()) text = " ";
    if (text.back() != '|') {
        text = text + "|";  // in case a terminating | is needed
    }
    auto tokenStrings =  Util::getTokens(text, '|');

    //
    // fill the info structures
    //

    for (const auto& tokenString : tokenStrings) {      // for each token string
        if (tokenString == "") continue;
        TextOrEmojiTokenInfo tokenInfo;
        tokenInfo.tokenString = tokenString;
        if (tokenString[0] == '@') {    // if emoji marker
            auto it = gui->buttonTextureMap.find(tokenString.c_str()+1);
            if (it != gui->buttonTextureMap.end()) {
                tokenInfo.emoji = it->second;   // save the texture
                Size s = it->second.size();
                tokenInfo.rect.x = 0;
                tokenInfo.rect.y = 0;
                tokenInfo.rect.w = s.w;
                tokenInfo.rect.h = s.h;
                // update overall size
                totalSize.w += s.w;
                if (s.h > totalSize.h)
                    totalSize.h = s.h;
                // add the token info
                tokenInfos.emplace_back(tokenInfo);
            } else {
                cout << "emoji not found for " << tokenString << endl;
            }
        } else {
            tokenInfo.rect = gui->getFontTextRect(font, tokenString);
            // update overall size
            totalSize.w += tokenInfo.rect.w;
            if (tokenInfo.rect.h > totalSize.h)
                totalSize.h = tokenInfo.rect.h;
            // add the token info
            tokenInfos.emplace_back(tokenInfo);
        }
    }

    int xOffset = 0;
    for (auto& tokenInfo : tokenInfos) {
        // set the x posit within the string
        tokenInfo.rect.x = xOffset;
        xOffset += tokenInfo.rect.w;
        // adjust the text y and emoji y so they are centered in the total height
        tokenInfo.rect.y = (totalSize.h - tokenInfo.rect.h) / 2;
    }
}

//*******************************
// Gui::AllTextOrEmojiTokenInfo::render
// renders/draws the text and emoji icons at the chosen position on the screen
//*******************************
void Gui::AllTextOrEmojiTokenInfo::render(int x, int y, XAlignment xAlign) {
    auto gui = Gui::getInstance();
    ableem::Renderer &renderer = gui->renderer();

    // compute x offset, center the y offset of each token to the total height
    compute_xy_relativeOffsets();

    // adjust the upper left corner postion if needed
    if (xAlign != XALIGN_LEFT)
        x = align_xPosition(xAlign, x, totalSize.w);

    if (drawBackgroundRect) {
        // render a grey box behind the text
        renderer.setDrawColor(Color(0, 0, 0, 70));
        Rect backRect;
        backRect.x = x - 10;
        backRect.y = y - 2;
        backRect.w = totalSize.w + 20;
        backRect.h = totalSize.h + 4;

        renderer.fillRect(backRect);
    }

    for (auto& tokenInfo : tokenInfos) {
        if (tokenInfo.emoji.valid()) {
            // the token is an emoji texture
            Rect tempRect = tokenInfo.rect;
            tempRect.x += x;
            tempRect.y += y;
            renderer.copy(tokenInfo.emoji, nullptr, &tempRect);
        } else {
            // the token is text
            if (useTextColor) {
                font.drawColor(renderer, x + tokenInfo.rect.x, y + tokenInfo.rect.y,
                               textColor, tokenInfo.tokenString);
            } else {
                font.drawAlign(renderer, x + tokenInfo.rect.x, y + tokenInfo.rect.y,
                               ableem::Align::Left, tokenInfo.tokenString);
            }
        }
    }
}

//*******************************
// Text Rendering routines
//*******************************

//*******************************
// Gui::renderText
// renders/draws the line of text and emoji icons at the chosen position on the screen.  returns the height.
//*******************************
int Gui::renderText(const ableem::Font &font, const string & text, int x, int y, XAlignment xAlign) {
    AllTextOrEmojiTokenInfo allTokenInfo(font, text);
    allTokenInfo.render(x, y, xAlign);

    return allTokenInfo.totalSize.h;    // return the height
}

//*******************************
// Gui::renderText_WithColor
// if background == true it draws a solid grey box around/behind the text
// this routine does not support emoji icons.  text only.
//*******************************
int Gui::renderText_WithColor(const ableem::Font &font, const std::string &text, int x, int y, Color textColor,
                              XAlignment xAlign, bool background) {
    AllTextOrEmojiTokenInfo allTokenInfo(font, text);
    allTokenInfo.setTextColor(textColor);
    allTokenInfo.drawBackgroundRect = background;

    allTokenInfo.render(x, y, xAlign);

    return allTokenInfo.totalSize.h;    // return the height
};

//*******************************
// Gui::renderTextLine
//*******************************
int Gui::renderTextLine(const string &text, int line, int yoffset, XAlignment xAlign, int xoffset, ableem::Font font) {
    if (!font.valid())
        font = themeFont;   // default to themeFont

    Rect opscreen = getOpscreenRectOfTheme();
    int fontHeight = font.lineHeight();
    int x = opscreen.x + 10 + xoffset;
    int y = (fontHeight * line) + yoffset;

    if (line<0)
    {
        line=-line;
        y=line;
    }

    return renderText(font, text, x, y, xAlign);
}

//*******************************
// Gui::renderTextLineToColumns
//*******************************
int Gui::renderTextLineToColumns(const string &textLeft, const string &textRight,
                                 int xLeft, int xRight,
                                 int line, int yoffset, ableem::Font font) {

    renderTextLine(textLeft,  line, yoffset, XALIGN_LEFT, xLeft, font);
    int h = renderTextLine(textRight, line, yoffset, XALIGN_LEFT, xRight, font);

    return h;   // rectangle height
}

//*******************************
// Gui::renderTextLineOptions
//*******************************
int Gui::renderTextLineOptions(const string &_text, int line, int yoffset, XAlignment xAlign, int xoffset) {
    string text = _text;

    // if there is a check or uncheck icon, flag which one and remove the emoji toekn from the string
    int button = -1;
    if (text.find("|@Check|") != std::string::npos) {
        button = 1;
    }
    if (text.find("|@Uncheck|") != std::string::npos) {
        button = 0;
    }
    if (button != -1) {
        text = text.substr(0, text.find("|"));
    }

    // render the text string without the check/uncheck icon
    int h = renderTextLine(text, line, yoffset, xAlign, xoffset);

    if (button == -1) {
        return h;   // there is no check/uncheck emoji on this line
    }

    // render the check/uncheck icon on the right side of opscreen
    Rect opscreen = getOpscreenRectOfTheme();
    int fontHeight = themeFont.lineHeight();

    int x = opscreen.x + opscreen.w - 10 - getCheckIconWidth();
    int y = (fontHeight * line) + yoffset;
    if (button == 1) {
        renderText(themeFont, "|@Check|", x, y);
    } else if (button == 0) {
        renderText(themeFont, "|@Uncheck|", x, y);
    }

    return h;
}

//*******************************
// Gui::renderSelectionBox
//*******************************
void Gui::renderSelectionBox(int line, int yoffset, int xoffset, ableem::Font font) {
    if (!font.valid())
        font = themeFont;

    string fg = themeData.values["text_fg"];
    int fontHeight = font.lineHeight();
    Rect opscreen = getOpscreenRectOfTheme();
    Rect rectSelection;
    rectSelection.x = opscreen.x + 5 + xoffset;
    rectSelection.y = yoffset + fontHeight * (line);
    rectSelection.w = opscreen.w - 10 - xoffset;
    rectSelection.h = fontHeight;

    renderer().setDrawColor(Color(getR(fg), getG(fg), getB(fg), 255));
    renderer().setBlendMode(ableem::BlendMode::Blend);
    renderer().drawRect(rectSelection);
}

//*******************************
// Gui::renderLabelBox
//*******************************
void Gui::renderLabelBox(int line, int yoffset) {
    string bg = themeData.values["label_bg"];
    int fontHeight = themeFont.lineHeight();
    Rect opscreen = getOpscreenRectOfTheme();
    Rect rectSelection;
    rectSelection.x = opscreen.x + 5;
    rectSelection.y = yoffset + fontHeight * (line);
    rectSelection.w = opscreen.w - 10;
    rectSelection.h = fontHeight;

    renderer().setDrawColor(Color(getR(bg), getG(bg), getB(bg), atoi(themeData.values["keyalpha"].c_str())));
    renderer().setBlendMode(ableem::BlendMode::Blend);
    renderer().fillRect(rectSelection);
}

//*******************************
// Gui::renderTextChar
//*******************************
void Gui::renderTextChar(const string &text, int line, int yoffset, int x) {
    int fontHeight = themeFont.lineHeight();
    int y = (fontHeight * line) + yoffset;
    themeFont.drawAlign(renderer(), x, y, ableem::Align::Left, text);
}

//*******************************
// Gui::renderFreeSpace
//*******************************
void Gui::renderFreeSpace() {
    int x = atoi(themeData.values["fsposx"].c_str());
    int y = atoi(themeData.values["fsposy"].c_str());
    renderText(themeFont, _("Free space") + " : " + Util::getAvailableSpace(), x, y);
}

//*******************************
// Gui::renderBackground
//*******************************
void Gui::renderBackground() {
    renderer().setDrawColor(Color(0x00, 0x00, 0x00, 0x00));
    renderer().clear();
    renderer().copy(backgroundImg, nullptr, &backgroundRect);
}

//*******************************
// Gui::renderLogo
//*******************************
int Gui::renderLogo(bool small) {
    if (!small) {
        renderer().copy(logo, nullptr, &logoRect);
        return 0;
    } else {
        Rect rect;
        rect.x = atoi(themeData.values["opscreenx"].c_str());
        rect.y = atoi(themeData.values["opscreeny"].c_str());
        rect.w = logoRect.w / 3;
        rect.h = logoRect.h / 3;
        renderer().copy(logo, nullptr, &rect);
        return rect.y + rect.h;
    }
}

//*******************************
// Gui::renderStatus
//*******************************
void Gui::renderStatus(const string &text, int posy) {
    string bg = themeData.values["text_bg"];

    renderer().setDrawColor(Color(getR(bg), getG(bg), getB(bg), atoi(themeData.values["textalpha"].c_str())));
    renderer().setBlendMode(ableem::BlendMode::Blend);
    Rect rect = getTextRectOfTheme();
    renderer().fillRect(rect);

    int y = atoi(themeData.values["ttop"].c_str());
    if (posy!=-1)
        y=posy; // override the bottom status y position.  so far this has never been used.

    renderText(themeFont, text, 0, y, XALIGN_CENTER);
}

//*******************************
// Gui::renderTextBar
//*******************************
void Gui::renderTextBar() {
    string bg = themeData.values["main_bg"];
    renderer().setDrawColor(Color(getR(bg), getG(bg), getB(bg), atoi(themeData.values["mainalpha"].c_str())));
    renderer().setBlendMode(ableem::BlendMode::Blend);

    Rect rect2 = getOpscreenRectOfTheme();

    renderer().fillRect(rect2);
}

//*******************************
// Gui::drawText
//*******************************
void Gui::drawText(const string &text, const string &topLine) {
    renderBackground();
    renderLogo(false);
    renderStatus(text);
    renderStatus(topLine, 5);
    renderer().present();
}
