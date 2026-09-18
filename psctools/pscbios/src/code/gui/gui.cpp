//
// Created by screemer on 2018-12-19.
//

#include "gui.h"
#include "gui_about.h"

#include "gui_confirm.h"
#include <SDL2/SDL_image.h>

#include "../lang.h"
#include <fstream>
#include <unistd.h>
#include "../util.h"
#include <iostream>
#include <iomanip>
#include "../environment.h"
#include "menus/gui_networkMenu.h"
#include "../bluetooth/bluetool.h"
#include "menus/gui_btMenu.h"
#include "abl.h"
#include "gamepad/gui_padconfig.h"
#include "gamepad/gui_gamepadMenu.h"
#include <ctime>

using namespace std;


#define RA_PLAYLIST "AutoBleem.lpl"

//*******************************
// GuiBase
//*******************************

//********************
// GuiBase::GuiBase
//********************
GuiBase::GuiBase() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    SDL_InitSubSystem(SDL_INIT_AUDIO);


    window = SDL_CreateWindow("PSC Bios", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

#if defined(__x86_64__) || defined(_M_X64)

#else
    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetWindowGrab(window, SDL_TRUE);
    SDL_SetRelativeMouseMode(SDL_TRUE);
#endif


#if 0
#if defined(__x86_64__) || defined(_M_X64)

#else
    const Uint8 *state = SDL_GetKeyboardState(NULL);
    if (!state[SDL_SCANCODE_AUDIOPLAY]) {
        exit(0);
    }
#endif
#endif
    TTF_Init();
    sonyFonts.openAllFonts(Env::getSonyFontPath());
    themeFonts.openAllFonts(getCurrentThemeFontPath());
}

//********************
// GuiBase::~GuiBase
//********************
GuiBase::~GuiBase() {
    SDL_Quit();
}

//*******************************
// GuiBase::getCurrentThemePath
//*******************************
string GuiBase::getCurrentThemePath() {
    string path = Env::getWorkingPath() + sep + "theme";
    return path;

}

string GuiBase::getAutobleemThemePath() {
#if defined(__x86_64__) || defined(_M_X64)
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
// GuiBase::getCurrentThemeImagePath
//*******************************
string GuiBase::getCurrentThemeImagePath() {
#if defined(__x86_64__) || defined(_M_X64)
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
// GuiBase::getCurrentThemeSoundPath
//*******************************
string GuiBase::getCurrentThemeSoundPath() {
#if defined(__x86_64__) || defined(_M_X64)
    string path = getCurrentThemePath() + sep + "sounds";
    if (!DirEntry::exists(path)) {
        path = Env::getSonyPath() + sep + "sounds";
    }
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
// GuiBase::getCurrentThemeFontPath
//*******************************
string GuiBase::getCurrentThemeFontPath() {
#if defined(__x86_64__) || defined(_M_X64)
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
// Gui
//*******************************

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
Uint8 Gui::getR(const string &val) {
    return atoi(Util::commaSep(val, 0).c_str());
}

//*******************************
// Gui::getG
//*******************************
Uint8 Gui::getG(const string &val) {
    return atoi(Util::commaSep(val, 1).c_str());
}

//*******************************
// Gui::getB
//*******************************
Uint8 Gui::getB(const string &val) {
    return atoi(Util::commaSep(val, 2).c_str());
}

//*******************************
// Gui::getTextAndRect
//*******************************
void Gui::getTextAndRect(SDL_Shared<SDL_Renderer> renderer, int x, int y, const char *text, TTF_Font_Shared font,
                         SDL_Shared<SDL_Texture> *texture, SDL_Rect *rect) {
    int text_width;
    int text_height;
    SDL_Shared<SDL_Surface> surface;
    string fg = themeData.values["text_fg"];
    SDL_Color textColor = {getR(fg), getG(fg), getB(fg), 0};

    Uint32 pixelFormat = SDL_GetWindowPixelFormat(window);
    if (strlen(text) == 0) {
        *texture = SDL_CreateTexture(renderer, pixelFormat, SDL_TEXTUREACCESS_STATIC, 0, 0);
        rect->x = 0;
        rect->y = 0;
        rect->h = 0;
        rect->w = 0;
        return;
    }

    surface = TTF_RenderUTF8_Solid(font, text, textColor);
    *texture = SDL_CreateTextureFromSurface(renderer, surface);
    text_width = surface->w;
    text_height = surface->h;
    rect->x = x;
    rect->y = y;
    rect->w = text_width;
    rect->h = text_height;
}

//*******************************
// Gui::renderBackground
//*******************************
void Gui::renderBackground() {
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, backgroundImg, nullptr, &backgroundRect);
}

//*******************************
// Gui::renderLogo
//*******************************
int Gui::renderLogo(bool small) {
    if (!small) {
        SDL_RenderCopy(renderer, logo, nullptr, &logoRect);
        return 0;
    } else {
        SDL_Rect rect;
        rect.x = atoi(themeData.values["opscreenx"].c_str());
        rect.y = atoi(themeData.values["opscreeny"].c_str());
        rect.w = logoRect.w / 3;
        rect.h = logoRect.h / 3;
        SDL_RenderCopy(renderer, logo, nullptr, &rect);
        return rect.y + rect.h;
    }
}

//*******************************
// Gui::loadThemeTexture
//*******************************
SDL_Shared<SDL_Texture>
Gui::loadThemeTexture(SDL_Shared<SDL_Renderer> renderer, string themePath, string defaultPath, string texname) {
    SDL_Shared<SDL_Texture> tex = nullptr;
    if (DirEntry::exists(themePath + themeData.values[texname])) {
        tex = IMG_LoadTexture(renderer, (themePath + themeData.values[texname]).c_str());
    } else {
        tex = IMG_LoadTexture(renderer, (defaultPath + defaultData.values[texname]).c_str());
    }
    return tex;
}

//*******************************
// Gui::loadAssets
//*******************************
void Gui::loadAssets(bool reloadMusic) {
    // check theme exists - otherwise back to aergb

    string defaultPath = getCurrentThemePath() + sep;
    themePath = getCurrentThemePath() + sep;

    cout << "Loading UI theme:" << themePath << endl;
    if (!DirEntry::exists(themePath + "theme.ini")) {
        themePath = defaultPath;
        cfg.inifile.values["theme"] = "default";
        cfg.save();
    }

    themeData.load(defaultPath + "theme.ini");
    defaultData.load(defaultPath + "theme.ini");
    themeData.load(themePath + "theme.ini");


    if (backgroundImg != nullptr) {
        Mix_FreeChunk(cursor);
        Mix_FreeChunk(cancel);
        Mix_FreeChunk(home_down);
        Mix_FreeChunk(home_up);
        backgroundImg = nullptr;
    }

    logoRect.x = atoi(themeData.values["lpositionx"].c_str());
    logoRect.y = atoi(themeData.values["lpositiony"].c_str());
    logoRect.w = atoi(themeData.values["lw"].c_str());
    logoRect.h = atoi(themeData.values["lh"].c_str());

    backgroundImg = loadThemeTexture(renderer, themePath, defaultPath, "background");
    cout << backgroundImg << "img" << endl;
    logo = loadThemeTexture(renderer, themePath, defaultPath, "logo");

    gamepadImg = IMG_LoadTexture(renderer, "DS3.png");
    buttonO = loadThemeTexture(renderer, themePath, defaultPath, "circle");
    buttonX = loadThemeTexture(renderer, themePath, defaultPath, "cross");
    buttonT = loadThemeTexture(renderer, themePath, defaultPath, "triangle");
    buttonS = loadThemeTexture(renderer, themePath, defaultPath, "square");
    buttonSelect = loadThemeTexture(renderer, themePath, defaultPath, "select");
    buttonStart = loadThemeTexture(renderer, themePath, defaultPath, "start");
    buttonL1 = loadThemeTexture(renderer, themePath, defaultPath, "l1");
    buttonR1 = loadThemeTexture(renderer, themePath, defaultPath, "r1");
    buttonL2 = loadThemeTexture(renderer, themePath, defaultPath, "l2");
    buttonR2 = loadThemeTexture(renderer, themePath, defaultPath, "r2");
    buttonCheck = loadThemeTexture(renderer, themePath, defaultPath, "check");
    buttonUncheck = loadThemeTexture(renderer, themePath, defaultPath, "uncheck");
    buttonEsc = loadThemeTexture(renderer, themePath, defaultPath, "esc");
    buttonEnter = loadThemeTexture(renderer, themePath, defaultPath, "enter");
    buttonTab = loadThemeTexture(renderer, themePath, defaultPath, "tab");

    string fontPath = (themePath + themeData.values["font"]);
    int fontSize = 0;
    string fontSizeString = themeData.values["fsize"];
    if (fontSizeString != "")
        fontSize = atoi(fontSizeString.c_str());
    themeFont = Fonts::openNewSharedFont(fontPath, fontSize);
    fastFont = FC_CreateFont();
    FC_LoadFont(fastFont, renderer, fontPath.c_str(), fontSize, FC_MakeColor(255, 255, 255, 255), TTF_STYLE_NORMAL);

    int w, h; // texture width & height
    SDL_SetTextureBlendMode(backgroundImg, SDL_BLENDMODE_BLEND);
    SDL_QueryTexture(backgroundImg, NULL, NULL, &w, &h);
    backgroundRect.x = 0;
    backgroundRect.y = 0;
    backgroundRect.w = w;
    backgroundRect.h = h;


    if (reloadMusic) {
        if (music != nullptr) {

            Mix_FreeMusic(music);
            music = nullptr;
        }
    }
    bool customMusic = false;
    int freq = 32000;
    string musicPath = themeData.values["music"];


    if (DirEntry::getFileExtension(musicPath) == "ogg") {
        freq = 44100;
    }

    if (reloadMusic) {
        int numtimesopened, frequency, channels;
        Uint16 format;
        numtimesopened = Mix_QuerySpec(&frequency, &format, &channels);
        for (int i = 0; i < numtimesopened; i++) {
            Mix_CloseAudio();
        }
        numtimesopened = Mix_QuerySpec(&frequency, &format, &channels);

        if (Mix_OpenAudio(freq, MIX_DEFAULT_FORMAT, 2, 1024) == -1) {
            printf("Unable to open audio: %s\n", Mix_GetError());
        }
    }
    cursor = Mix_LoadWAV((this->getCurrentThemeSoundPath() + sep + "cursor.wav").c_str());
    cancel = Mix_LoadWAV((this->getCurrentThemeSoundPath() + sep + "cancel.wav").c_str());
    home_up = Mix_LoadWAV((this->getCurrentThemeSoundPath() + sep + "home_up.wav").c_str());
    home_down = Mix_LoadWAV((this->getCurrentThemeSoundPath() + sep + "home_down.wav").c_str());
    resume = Mix_LoadWAV((this->getCurrentThemeSoundPath() + sep + "resume_new.wav").c_str());

    if (reloadMusic)
        if (cfg.inifile.values["nomusic"] != "true")
            if (themeData.values["loop"] != "-1") {


                if (!customMusic) {
                    music = Mix_LoadMUS((themePath + themeData.values["music"]).c_str());
                    if (music == nullptr) { printf("Unable to load Music file: %s\n", Mix_GetError()); }
                    if (Mix_PlayMusic(music, themeData.values["loop"] == "1" ? -1 : 0) == -1) {
                        printf("Unable to play music file: %s\n", Mix_GetError());
                    }
                }

            }
}

//*******************************
// Gui::hideMouseCursor
//*******************************
void Gui::hideMouseCursor() {

#if defined(__x86_64__) || defined(_M_X64)

#else
    SDL_ShowCursor(SDL_DISABLE);
SDL_SetWindowGrab(window, SDL_TRUE);
SDL_SetRelativeMouseMode(SDL_TRUE);
#endif

}

//*******************************
// Gui::criticalException
//*******************************
void Gui::criticalException(const string &text) {
    drawText(text);
    while (true) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            mapper.handleHotPlug(&e);
            mapper.handlePowerBtn(&e);

            if (e.type == SDL_QUIT)
                return;
            else if (e.type == SDL_KEYUP && e.key.keysym.sym == SDLK_ESCAPE)
                return;

            if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                return;
            }
        }
    }
}

//*******************************
// Gui::display
//*******************************
void Gui::display() {
    joysticks.clear();
    joynames.clear();


    SDL_version compiled;
    SDL_version linked;

    SDL_VERSION(&compiled);
    SDL_GetVersion(&linked);
    printf("We compiled against SDL version %d.%d.%d ...\n",
           compiled.major, compiled.minor, compiled.patch);
    printf("But we are linking against SDL version %d.%d.%d.\n",
           linked.major, linked.minor, linked.patch);

    Mix_Init(0);
    TTF_Init();
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");

    loadAssets();


    hideMouseCursor();

}

//*******************************
// Gui::saveSelection
//*******************************


bool otherMenuShift = false;
bool powerOffShift = false;


void Gui::refreshInfo() {
    wirelessFound = networkController.isWlanOn();
    ethFound = networkController.interfaceFound("eth0");
    wirelessActive = networkController.isUp("wlan0");
    ethActive = networkController.isUp("eth0");
    wirelessAddr = networkController.getIPInfo("wlan0");
    ethAddr = networkController.getIPInfo("eth0");
    btActive = networkController.isBluetoothOn();
    btName = networkController.getBluetoothInfo();
}

//*******************************
// Gui::menuSelection
//*******************************
void Gui::menuSelection() {

#if defined(__x86_64__) || defined(_M_X64)
    // nope
#else
    if (!Env::autobleemKernel)
    {
        Gui::splash(_("Custom Firmware Kernel Not Found !"));
        usleep(5 *1000000);
        exit(0);
        return;
    }
#endif

    hideMouseCursor();
    otherMenuShift = false;
    powerOffShift = false;

    string mainMenu = "|@Select| " + _("WiFi Settings") + "   ";
    // mainMenu += "|@Start| " + _("BT Manager") + "   " ;
    mainMenu += "|@T| " + _("About") + "   ";
    mainMenu += "|@O| " + _("Cancel") + "   ";


    int frame = 240;
    bool menuVisible = true;
    refreshInfo();

    while (menuVisible) {


        if (frame >= 75) {
            refreshInfo();
            frame = 0;
        }


        renderBackground();
        renderLogo(false);

        renderTextBar();
        int offset = renderLogo(true);


        string mainMenu;
        if (wirelessFound && Env::autobleemKernel)
            mainMenu += "|@Select| " + _("WiFi Settings") + "   ";
        mainMenu += "|@S| " + _("Setup Gamepads") + "   ";
        mainMenu += "|@T| " + _("About") + "   ";
        mainMenu += "|@O| " + _("Cancel") + "   ";
        renderStatus(mainMenu);

        renderTextLine("-=" + _("Playstation Classic Hardware Information") + "=-", 0, offset, true);

        int ciLine=1;
        if (Env::autobleemKernel) {
            ciLine=15;
            string clock = getClock();
            clock = clock.substr(0, clock.length() - 1);
            string wifiStatus = "" + (wirelessFound ? _("Found") : _("Not found")) + "/" +
                                (wirelessActive ? _("Active") : _("Not active"));
            string ethStatus =
                    "" + (ethFound ? _("Found") : _("Not found")) + "/" + (ethActive ? _("Active") : _("Not active"));
            string btStatus =
                    "" + (btActive ? _("Found") : _("Not found")) + "/" + (btActive ? _("Active") : _("Not active"));




            renderTextLine(_("Current Time:") + "  " + clock + "    (" + guiGetTZ() + ")", 1, offset, false);

            renderTextLine(_("WiFi information:"), 3, offset, false);
            renderTextLine("   " + _("Dongle status:") + "  " + wifiStatus, 4, offset, false);
            renderTextLine("   " + _("IP configuration:") + "  " + wirelessAddr, 5, offset, false);
            renderTextLine(_("Ethernet information:"), 7, offset, false);
            renderTextLine("   " + _("Dongle status:") + "  " + ethStatus, 8, offset, false);
            renderTextLine("   " + _("IP configuration:") + "  " + ethAddr, 9, offset, false);
            renderTextLine(_("Bluetooth information:"), 11, offset, false);
            renderTextLine("   " + _("Dongle status:") + "  " + btStatus, 12, offset, false);
            renderTextLine("   " + _("Interface details:") + "  " + btName, 13, offset, false);
        }
        string controllerNumInfo =
                "   " + _("Game Controllers number: ") + to_string(mapper.getActivePadNum()) + "/" +
                to_string(SDL_NumJoysticks());

        renderTextLine(_("Game Controller information:"), ciLine+0, offset, false);
        renderTextLine(controllerNumInfo, ciLine+1, offset, false);
        int ctrlnum = 0;
        for (int i = 0; i < SDL_NumJoysticks(); i++) {
            char guid_str[100];
            SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(i);
            SDL_JoystickGetGUIDString(guid, guid_str, 100);
            string name = SDL_JoystickNameForIndex(i);
            if (SDL_IsGameController(i)) {
                name += string(_(" - Mapping (Available):")) + SDL_GameControllerNameForIndex(i);
            } else {
                name += string(_(" - Mapping (Not found)"));
            }
            renderTextLine("     #" + to_string(i) + /* "    (" + guid_str + ")   " + */ +" " + name, ciLine+2+ i, offset,
                           false);
            ctrlnum++;
            if (ctrlnum >= 4) {
                renderTextLine("..", 18 + i, offset, false);
                break;
            }
        }
        SDL_RenderPresent(renderer);

        frame++;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            mapper.handleHotPlug(&e);
            mapper.handlePowerBtn(&e);
            // this is for pc Only
            if (e.type == SDL_QUIT) {
                menuVisible = false;
            }
            switch (e.type) {
                case SDL_CONTROLLERBUTTONUP:
                    if (e.cbutton.button == SDL_BTN_L2) {
                        powerOffShift = false;
                    }
                    break;

                case SDL_CONTROLLERBUTTONDOWN:
                    if (e.cbutton.button == SDL_BTN_L2) {
                        powerOffShift = true;

                    }

                    if (!otherMenuShift) {
                        if (e.cbutton.button == SDL_BTN_CIRCLE) {
                            Mix_PlayChannel(-1, cursor, 0);
                            mapper.flushPads();
                            SDL_Quit();
                            exit(0);
                        };


                        if (e.cbutton.button == SDL_BTN_START) {

                        }
                        if (e.cbutton.button == SDL_BTN_SQUARE) {

                            Mix_PlayChannel(-1, cursor, 0);
                            auto *aboutScreen = new GuiGamepadMenu(renderer);
                            aboutScreen->show();
                            delete aboutScreen;
                            menuSelection();
                            menuVisible = false;

                        };

                        if (e.cbutton.button == SDL_BTN_CROSS) {
                        };

                        if (e.cbutton.button == SDL_BTN_TRIANGLE) {
                            Mix_PlayChannel(-1, cursor, 0);
                            auto *aboutScreen = new GuiAbout(renderer);
                            aboutScreen->show();
                            delete aboutScreen;
                            menuSelection();
                            menuVisible = false;
                        };
                        if (e.cbutton.button == SDL_BTN_SELECT) {
                            if (Env::autobleemKernel) {
                                Mix_PlayChannel(-1, cursor, 0);
                                auto networkMenu = new GuiNetworkMenu(renderer);
                                networkMenu->show();
                                delete networkMenu;
                            }
                        };

                    } else {

                    }
            }
        }

    }

}

//*******************************
// Gui::finish
//*******************************
void Gui::finish() {

    if (Mix_PlayingMusic()) {
        Mix_FadeOutMusic(300);
        while (Mix_PlayingMusic()) {
        }
    } else {
        usleep(300 * 1000);
    }

    Mix_HaltMusic();
    Mix_FreeMusic(music);
    Mix_FreeChunk(cursor);
    Mix_FreeChunk(cancel);
    Mix_FreeChunk(home_down);
    Mix_FreeChunk(home_up);
    Mix_CloseAudio();
    music = nullptr;
    backgroundImg = nullptr;
}

//*******************************
// Gui::getEmojiTextTexture
//*******************************
void Gui::getEmojiTextTexture(SDL_Shared<SDL_Renderer> renderer, string text, TTF_Font_Shared font,
                              SDL_Shared<SDL_Texture> *texture,
                              SDL_Rect *rect) {
    if (text.empty()) text = " ";
    if (text.back() != '|') {
        text = text + "|";
    }

    vector<SDL_Shared<SDL_Texture>> textTexures;
    vector<string> textParts;
    std::string delimiter = "|";

    size_t pos = 0;
    std::string token;
    while ((pos = text.find(delimiter)) != std::string::npos) {
        token = text.substr(0, pos);
        if (!token.empty())
            textParts.push_back(token);
        text.erase(0, pos + delimiter.length());
    }

    for (const string &str:textParts) {
        if (str.empty()) continue;
        if (str[0] == '@') {
            string icon = str.substr(1);
            if (icon == "Start") {
                textTexures.push_back(buttonStart);
            }
            if (icon == "S") {
                textTexures.push_back(buttonS);
            }
            if (icon == "O") {
                textTexures.push_back(buttonO);
            }
            if (icon == "Select") {
                textTexures.push_back(buttonSelect);
            }
            if (icon == "L1") {
                textTexures.push_back(buttonL1);
            }
            if (icon == "R1") {
                textTexures.push_back(buttonR1);
            }
            if (icon == "L2") {
                textTexures.push_back(buttonL2);
            }
            if (icon == "R2") {
                textTexures.push_back(buttonR2);
            }
            if (icon == "T") {
                textTexures.push_back(buttonT);
            }
            if (icon == "X") {
                textTexures.push_back(buttonX);
            }
            if (icon == "Check") {
                textTexures.push_back(buttonCheck);
            }
            if (icon == "Uncheck") {
                textTexures.push_back(buttonUncheck);
            }
            if (icon == "Esc") {
                textTexures.push_back(buttonEsc);
            }
            if (icon == "Enter") {
                textTexures.push_back(buttonEnter);
            }
            if (icon == "Tab") {
                textTexures.push_back(buttonTab);
            }
        } else {
            SDL_Shared<SDL_Texture> textTex = nullptr;
            SDL_Rect textRec;
            getTextAndRect(renderer, 0, atoi(themeData.values["ttop"].c_str()), str.c_str(), font, &textTex,
                           &textRec);
            textTexures.push_back(textTex);
        }
    }

    int w = 0;
    int h = 0;

    for (SDL_Shared<SDL_Texture> tex:textTexures) {
        Uint32 format;
        int access;
        int tw, th;
        SDL_QueryTexture(tex, &format, &access, &tw, &th);

        w += tw;
        if (th > h) h = th;
    }

    Uint32 pixelFormat = SDL_GetWindowPixelFormat(window);
    *texture = SDL_CreateTexture(renderer, pixelFormat, SDL_TEXTUREACCESS_TARGET, w, h);
    SDL_SetTextureBlendMode(*texture, SDL_BLENDMODE_NONE);
    SDL_SetRenderTarget(renderer, *texture);
    SDL_SetTextureBlendMode(*texture, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    int xpos = 0;
    for (SDL_Shared<SDL_Texture> tex:textTexures) {
        Uint32 format;
        int access;
        int tw, th;
        SDL_QueryTexture(tex, &format, &access, &tw, &th);

        SDL_Rect posRect;
        posRect.x = xpos;

        posRect.y = 0;

        if (th != h) {
            posRect.y = (h - th) / 2;
        }
        posRect.w = tw;
        posRect.h = th;
        xpos += tw;
        SDL_RenderCopy(renderer, tex, nullptr, &posRect);
    }
    rect->w = w;
    rect->h = h;
    rect->x = 0;
    rect->y = 0;
    SDL_SetRenderTarget(renderer, nullptr);

    textTexures.clear();
}

//*******************************
// Gui::renderStatus
//*******************************
void Gui::renderStatus(string text, int posy) {
    string bg = themeData.values["text_bg"];

    SDL_Shared<SDL_Texture> textTex;
    SDL_SetRenderDrawColor(renderer, getR(bg), getG(bg), getB(bg), atoi(themeData.values["textalpha"].c_str()));
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_Rect rect;
    rect.x = atoi(themeData.values["textx"].c_str());
    rect.y = atoi(themeData.values["texty"].c_str());
    rect.w = atoi(themeData.values["textw"].c_str());
    rect.h = atoi(themeData.values["texth"].c_str());
    SDL_RenderFillRect(renderer, &rect);


    if (text.empty()) text = " ";
    if (text.back() != '|') {
        text = text + "|";
    }

    vector<string> textParts;
    int maxHeight = 0;
    int totalWidth = 0;
    std::string delimiter = "|";

    size_t pos = 0;
    std::string token;
    while ((pos = text.find(delimiter)) != std::string::npos) {
        token = text.substr(0, pos);
        if (!token.empty())
            textParts.push_back(token);
        text.erase(0, pos + delimiter.length());
    }

    for (const string &str:textParts) {
        if (str.empty()) continue;
        if (str[0] == '@') {
            string icon = str.substr(1);
            SDL_Shared<SDL_Texture> tex = getTextureByName(icon);
            if (tex != nullptr) {
                int w, h;
                SDL_QueryTexture(tex, NULL, NULL, &w, &h);
                if (h > maxHeight) maxHeight = h;
                totalWidth += w;
            }
        } else {
            int w = FC_GetWidth(fastFont, str.c_str());
            int h = FC_GetLineHeight(fastFont);
            if (h > maxHeight) maxHeight = h;
            totalWidth += w;

        }
    }

    int x = 640 - totalWidth / 2;
    int y = atoi(themeData.values["ttop"].c_str());
    if (posy != -1) {
        y = posy;
    }
    int textHeight = FC_GetLineHeight(fastFont);
    int texty = y + (maxHeight-textHeight)/2;
    for (const string &str:textParts) {
        if (str.empty()) continue;
        if (str[0] == '@') {
            string icon = str.substr(1);
            SDL_Shared<SDL_Texture> tex = getTextureByName(icon);
            if (tex != nullptr) {
                int w, h;
                SDL_QueryTexture(tex, NULL, NULL, &w, &h);
                SDL_Rect texR;
                texR.x = x;
                texR.y = y;
                texR.w = w;
                texR.h = h;
                SDL_RenderCopy(renderer, tex, NULL, &texR);
                x += w;
            }
        } else {
            int width = FC_GetWidth(fastFont, str.c_str());
            FC_DrawAlign(fastFont, renderer, x, texty, FC_ALIGN_LEFT, str.c_str());
            x += width;
        }
    }

}

//*******************************
// Gui::drawText
//*******************************
void Gui::drawText(const string &text) {
    renderBackground();
    renderLogo(false);
    renderStatus(text);
    SDL_RenderPresent(renderer);
}

//*******************************
// Gui::renderLabelBox
//*******************************
void Gui::renderLabelBox(int line, int offset) {
    SDL_Shared<SDL_Texture> textTex;
    SDL_Rect textRec;

    string bg = themeData.values["label_bg"];

    getTextAndRect(renderer, 0, 0, "*", themeFont, &textTex, &textRec);

    SDL_Rect rect2;
    rect2.x = atoi(themeData.values["opscreenx"].c_str());
    rect2.y = atoi(themeData.values["opscreeny"].c_str());
    rect2.w = atoi(themeData.values["opscreenw"].c_str());
    rect2.h = atoi(themeData.values["opscreenh"].c_str());

    SDL_Rect rectSelection;
    rectSelection.x = rect2.x + 5;
    rectSelection.y = offset + textRec.h * (line);
    rectSelection.w = rect2.w - 10;
    rectSelection.h = textRec.h;


    SDL_SetRenderDrawColor(renderer, getR(bg), getG(bg), getB(bg), atoi(themeData.values["keyalpha"].c_str()));
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderFillRect(renderer, &rectSelection);
}

//*******************************
// Gui::renderSelectionBox
//*******************************
void Gui::renderSelectionBox(int line, int offset, int xoffset, TTF_Font_Shared font) {
    SDL_Shared<SDL_Texture> textTex;
    SDL_Rect textRec;

    if (!font)
        font = themeFont;

    string fg = themeData.values["text_fg"];

    getTextAndRect(renderer, 0, 0, "*", font, &textTex, &textRec);

    SDL_Rect rect2;
    rect2.x = atoi(themeData.values["opscreenx"].c_str());
    rect2.y = atoi(themeData.values["opscreeny"].c_str());
    rect2.w = atoi(themeData.values["opscreenw"].c_str());
    rect2.h = atoi(themeData.values["opscreenh"].c_str());

    SDL_Rect rectSelection;
    rectSelection.x = rect2.x + 5 + xoffset;
    rectSelection.y = offset + textRec.h * (line);
    rectSelection.w = rect2.w - 10 - xoffset;
    rectSelection.h = textRec.h;

    SDL_SetRenderDrawColor(renderer, getR(fg), getG(fg), getB(fg), 255);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderDrawRect(renderer, &rectSelection);
}

//*******************************
// Gui::renderTextLineOptions
//*******************************
int Gui::renderTextLineOptions(const string &_text, int line, int offset, int position, int xoffset) {
    string text = _text;
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

    int h = renderTextLine(text, line, offset, position, xoffset);

    SDL_Shared<SDL_Texture> buttonTex;
//    SDL_Rect rect;

    if (button == -1) {
        return h;
    }

    SDL_Rect textRec;
    SDL_Rect rect2;

    rect2.x = atoi(themeData.values["opscreenx"].c_str());
    rect2.y = atoi(themeData.values["opscreeny"].c_str());
    rect2.w = atoi(themeData.values["opscreenw"].c_str());
    rect2.h = atoi(themeData.values["opscreenh"].c_str());
    getTextAndRect(renderer, 0, 0, "*", themeFont, &buttonTex, &textRec);
    int lineh = textRec.h;
    if (button == 1) {
        getEmojiTextTexture(renderer, "|@Check|", themeFont, &buttonTex, &textRec);
    } else if (button == 0) {
        getEmojiTextTexture(renderer, "|@Uncheck|", themeFont, &buttonTex, &textRec);
    }

    textRec.x = rect2.x + rect2.w - 10 - textRec.w;
    textRec.y = (lineh * line) + offset;

    if (textRec.w >= (1280 - rect2.x * 4)) {
        textRec.w = (1280 - rect2.x * 4);
    }
    if (position == POS_CENTER) {
        textRec.x = (1280 / 2) - textRec.w / 2;
    }
    if (position == POS_RIGHT) {
        textRec.x = 1280 - textRec.x - textRec.w;
    }

    SDL_RenderCopy(renderer, buttonTex, nullptr, &textRec);
    return h;
}

SDL_Shared<SDL_Texture> Gui::getTextureByName(string name) {
    if (name == "Start") {
        return buttonStart;
    }
    if (name == "S") {
        return buttonS;
    }
    if (name == "O") {
        return buttonO;
    }
    if (name == "Select") {
        return buttonSelect;
    }
    if (name == "L1") {
        return buttonL1;
    }
    if (name == "R1") {
        return buttonR1;
    }
    if (name == "L2") {
        return buttonL2;
    }
    if (name == "R2") {
        return buttonR2;
    }
    if (name == "T") {
        return buttonT;
    }
    if (name == "X") {
        return buttonX;
    }
    if (name == "Check") {
        return buttonCheck;
    }
    if (name == "Uncheck") {
        return buttonUncheck;
    }
    if (name == "Esc") {
        return buttonEsc;
    }
    if (name == "Enter") {
        return buttonEnter;
    }
    if (name == "Tab") {
        return buttonTab;
    }
    return nullptr;
}

//*******************************
// Gui::renderTextLine
//*******************************
int Gui::renderTextLine(string text, int line, int offset, int position, int xoffset, TTF_Font_Shared font) {

    if (text.empty()) text = " ";
    if (text.back() != '|') {
        text = text + "|";
    }

    vector<string> textParts;
    int maxHeight = 0;
    int totalWidth = 0;
    std::string delimiter = "|";

    size_t pos = 0;
    std::string token;
    while ((pos = text.find(delimiter)) != std::string::npos) {
        token = text.substr(0, pos);
        if (!token.empty())
            textParts.push_back(token);
        text.erase(0, pos + delimiter.length());
    }

    for (const string &str:textParts) {
        if (str.empty()) continue;
        if (str[0] == '@') {
            string icon = str.substr(1);
            SDL_Shared<SDL_Texture> tex = getTextureByName(icon);
            if (tex != nullptr) {
                int w, h;
                SDL_QueryTexture(tex, NULL, NULL, &w, &h);
                if (h > maxHeight) maxHeight = h;
                totalWidth += w;
            }
        } else {
            int w = FC_GetWidth(fastFont, str.c_str());
            int h = FC_GetLineHeight(fastFont);
            if (h > maxHeight) maxHeight = h;
            totalWidth += w;

        }
    }


    SDL_Rect rect2;
    rect2.x = atoi(themeData.values["opscreenx"].c_str());
    rect2.y = atoi(themeData.values["opscreeny"].c_str());
    rect2.w = atoi(themeData.values["opscreenw"].c_str());
    rect2.h = atoi(themeData.values["opscreenh"].c_str());

    Uint16 lineHeight = maxHeight;
    int x = 0 + rect2.x + 10, y = offset + line * lineHeight;
    if (position == POS_LEFT) {
        x = 0 + rect2.x + 10, y = offset + line * lineHeight;
    }
    if (position == POS_CENTER) {
        x = 640 - totalWidth / 2;
    }
    if (position == POS_RIGHT) {
        x = 1280 - (totalWidth) - 10 - rect2.x;
    }

    for (const string &str:textParts) {
        if (str.empty()) continue;
        if (str[0] == '@') {
            string icon = str.substr(1);
            SDL_Shared<SDL_Texture> tex = getTextureByName(icon);
            if (tex != nullptr) {
                int w, h;
                SDL_QueryTexture(tex, NULL, NULL, &w, &h);
                SDL_Rect texR;
                texR.x = x;
                texR.y = y;
                texR.w = w;
                texR.h = h;
                SDL_RenderCopy(renderer, tex, NULL, &texR);
                x += w;
            }
        } else {
            int width = FC_GetWidth(fastFont, str.c_str());
            FC_DrawAlign(fastFont, renderer, x, y, FC_ALIGN_LEFT, str.c_str());
            x += width;
        }
    }
    return maxHeight;
}

//*******************************
// Gui::getTextRectangleOnScreen
//*******************************
// returns the SDL_Rect of the screen positions if your rendered this text with these args
SDL_Rect Gui::getTextRectangleOnScreen(string text, int line, int offset, int position, int xoffset,
                                       TTF_Font_Shared font) {
    if (text.empty()) text = " ";
    if (text.back() != '|') {
        text = text + "|";
    }

    vector<string> textParts;
    int maxHeight = 0;
    int totalWidth = 0;
    std::string delimiter = "|";

    size_t pos = 0;
    std::string token;
    while ((pos = text.find(delimiter)) != std::string::npos) {
        token = text.substr(0, pos);
        if (!token.empty())
            textParts.push_back(token);
        text.erase(0, pos + delimiter.length());
    }

    for (const string &str:textParts) {
        if (str.empty()) continue;
        if (str[0] == '@') {
            string icon = str.substr(1);
            SDL_Shared<SDL_Texture> tex = getTextureByName(icon);
            if (tex != nullptr) {
                int w, h;
                SDL_QueryTexture(tex, NULL, NULL, &w, &h);
                if (h > maxHeight) maxHeight = h;
                totalWidth += w;
            }
        } else {
            int w = FC_GetWidth(fastFont, str.c_str());
            int h = FC_GetLineHeight(fastFont);
            if (h > maxHeight) maxHeight = h;
            totalWidth += w;

        }
    }

    SDL_Rect rect2;
    rect2.x = atoi(themeData.values["opscreenx"].c_str());
    rect2.y = atoi(themeData.values["opscreeny"].c_str());
    rect2.w = atoi(themeData.values["opscreenw"].c_str());
    rect2.h = atoi(themeData.values["opscreenh"].c_str());

    SDL_Rect res;
    Uint16 lineHeight = maxHeight;
    int x = 0 + rect2.x + 10, y = offset + line * lineHeight;
    if (position == POS_LEFT) {
        x = 0 + rect2.x + 10, y = offset + line * lineHeight;
    }
    if (position == POS_CENTER) {
        x = 640 - totalWidth / 2;
    }
    if (position == POS_RIGHT) {
        x = 1280 - (totalWidth) - 10 - rect2.x;
    }
    res.x=x; res.y=y;res.w=totalWidth;res.h=maxHeight;
    return res;
}

//*******************************
// Gui::renderTextLineToColumns
//*******************************
int Gui::renderTextLineToColumns(const string &textLeft, const string &textRight,
                                 int xLeft, int xRight,
                                 int line, int offset, TTF_Font_Shared font) {

    renderTextLine(textLeft, line, offset, POS_LEFT, xLeft, font);
    int h = renderTextLine(textRight, line, offset, POS_LEFT, xRight, font);

    return h;   // rectangle height
}

//*******************************
// Gui::renderTextChar
//*******************************
void Gui::renderTextChar(const string &text, int line, int offset, int posx) {
#if 0
    SDL_Rect rect2;
    rect2.x = atoi(themeData.values["opscreenx"].c_str());
    rect2.y = atoi(themeData.values["opscreeny"].c_str());
    rect2.w = atoi(themeData.values["opscreenw"].c_str());
    rect2.h = atoi(themeData.values["opscreenh"].c_str());
#endif

    SDL_Shared<SDL_Texture> textTex;
    SDL_Rect textRec;

    getTextAndRect(renderer, 0, 0, "*", themeFont, &textTex, &textRec);
    getTextAndRect(renderer, posx, (textRec.h * line) + offset,
                   text.c_str(), themeFont, &textTex, &textRec);

    SDL_RenderCopy(renderer, textTex, nullptr, &textRec);
}

//*******************************
// Gui::renderTextBar
//*******************************
void Gui::renderTextBar(int height) {
    string bg = themeData.values["main_bg"];
    SDL_SetRenderDrawColor(renderer, getR(bg), getG(bg), getB(bg), atoi(themeData.values["mainalpha"].c_str()));
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_Rect rect2;
    rect2.x = atoi(themeData.values["opscreenx"].c_str());
    rect2.y = atoi(themeData.values["opscreeny"].c_str());
    rect2.w = atoi(themeData.values["opscreenw"].c_str());
    if (height == -1)
        rect2.h = atoi(themeData.values["opscreenh"].c_str());
    else
        rect2.h = height;
    SDL_RenderFillRect(renderer, &rect2);
}


string Gui::getClock() {
    std::time_t t = std::time(0);   // get time now
    std::tm *now = std::localtime(&t);
    return std::asctime(now);
}

string Gui::guiGetTZ() {
#if defined(__x86_64__) || defined(_M_X64)
    return "Unknown";

#else
    string timezone = Util::execUnixCommand("/bin/settime tz");
    return timezone;
#endif
}
