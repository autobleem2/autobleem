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
#include "../launcher/gui_launcher.h"
#include "gui_padTest.h"
#include "../core/lang.h"
#include "../app.h"
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <cassert>

using namespace std;
using ableem::Rect;
using ableem::Size;
using ableem::Color;
using ableem::Texture;
using ableem::Event;
using ableem::Button;
//********************
// Gui::Gui
//********************
Gui::Gui() : text_(renderer(), App::get().theme(), themeFont, buttonTextureMap) {
    sonyFonts.openAllFonts(Env::getSonyFontPath(), renderer());
    themeFonts.openAllFonts(App::get().theme().fontPath(), renderer());
    input().probePads();
}

//*******************************
// Gui::splash
//*******************************
void Gui::splash(const string &message) {
    shared_ptr<Gui> gui(Gui::getInstance());
    gui->drawText(message);
}


//*******************************
// Gui::loadThemeTexture
//*******************************
Texture
Gui::loadThemeTexture(const string& themePath, const string& defaultPath, const string& texname) {
    Texture tex;
    if (DirEntry::exists(themePath + App::get().theme().data.values[texname])) {
        tex = Texture::loadFile(renderer(), themePath + App::get().theme().data.values[texname]);
    } else {
        tex = Texture::loadFile(renderer(), defaultPath + App::get().theme().defaults.values[texname]);
    }
    return tex;
}


//*******************************
// Gui::loadAssets
//*******************************
void Gui::loadAssets(bool reloadMusic) {
    App::get().theme().load();     // (re)reads theme.ini, falling back to themes/default
    const string themePath = App::get().theme().loadedPath();
    const string defaultPath = App::get().theme().defaultsPath();

    backgroundImg = Texture();  // release the previous theme's textures before loading the new ones

    logoRect.x = atoi(App::get().theme().data.values["lpositionx"].c_str());
    logoRect.y = atoi(App::get().theme().data.values["lpositiony"].c_str());
    logoRect.w = atoi(App::get().theme().data.values["lw"].c_str());
    logoRect.h = atoi(App::get().theme().data.values["lh"].c_str());

    backgroundImg = loadThemeTexture(themePath, defaultPath, "background");
    logo = loadThemeTexture(themePath, defaultPath, "logo");
    if (App::get().config().inifile.values["jewel"] != "none") {
        if (App::get().config().inifile.values["jewel"] == "default") {
            cdJewel = Texture::loadFile(renderer(), Env::getWorkingPath() + sep + "evoimg/nofilter.png");
        } else {
            cdJewel = Texture::loadFile(renderer(),
                                        Env::getWorkingPath() + sep + "evoimg/frames/" +
                                        App::get().config().inifile.values["jewel"]);
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

    string fontPath = (themePath + App::get().theme().data.values["font"]);
    int fontSize = 0;
    string fontSizeString = App::get().theme().data.values["fsize"];
    if (fontSizeString != "")
        fontSize = atoi(fontSizeString.c_str());
    themeFont = Fonts::openNewSharedCachedFont(fontPath, fontSize, renderer());

    App::get().audio().loadTheme(reloadMusic);
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
void Gui::display(bool resume) {
    cout << platform().versionString() << endl;

    platform().setScaleQuality(2);

    loadAssets();

    if (!resume) {
        GuiSplash splashScreen(*this);
        splashScreen.show();
        hideMouseCursor();
    } else {
        App::get().session().resumingGui = true;
    }
}

bool otherMenuShift = false;
bool powerOffShift = false;


//*******************************
// Gui::menuSelection
//*******************************
void Gui::menuSelection() {
    shared_ptr<Scanner> scanner(Scanner::getInstance());


    if (!App::get().library().covers().hasAnyRegion()) {
        criticalException(_("WARNING: NO COVER DB FOUND. PRESS ANY BUTTON."));
    }
    otherMenuShift = false;
    powerOffShift = false;
    bool forceScan = App::get().session().forceScan;
    string mainMenu = "|@Start| " + _("AutoBleem") + "    |@X|  " + _("Re/Scan") + " ";
    if (App::get().config().inifile.values["ui"] == "classic") {
        mainMenu += "  |@O|  " + _("Original") + "  ";
    }
    string RA_or_EA = _("RetroArch");
    string cfgPath = Env::getPathToRetroarchDir() + sep + "retroboot/retroboot.cfg";
    if (DirEntry::exists(cfgPath)) {
        IniFile RBcfg;
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
        if (App::get().session().startingGame) {
            drawText(App::get().session().runningGame->title);
            App::get().session().menuOption = MENU_OPTION_START;
            menuVisible = false;
            App::get().session().startingGame = false;
            return;
        }

        if (App::get().session().resumingGui) {
            {   // scoped: the screen must be gone before menuSelection() recurses
                GuiLauncher launcherScreen(*this);
                launcherScreen.show();
            }
            drawText("");
            App::get().session().resumingGui = false;
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
                            App::get().audio().cursor.play();
                            drawText(mainMenu);
                            otherMenuShift = false;
                        }
                        if (e.button == Button::L2) {
                            App::get().audio().cursor.play();
                            powerOffShift = false;
                        }
                    }
                    break;
                case Event::Type::ButtonDown:
                    if (!forceScan) {
                        if (e.button == Button::L1) {
                            App::get().audio().cursor.play();
                            drawText(otherMenu);
                            otherMenuShift = true;
                        }
                        if (e.button == Button::L2) {
                            App::get().audio().cursor.play();
                            powerOffShift = true;
                        }
                    }

                    if (powerOffShift) {
                        if (e.button == Button::R2) {
                            App::get().audio().cursor.play();
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
                                if (App::get().config().inifile.values["ui"] == "classic") {
                                    App::get().audio().cursor.play();
                                    App::get().session().menuOption = MENU_OPTION_RUN;
                                    menuVisible = false;
                                } else {
                                    App::get().audio().cursor.play();
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
                                App::get().audio().cursor.play();
                                if (!DirEntry::exists(Env::getPathToRetroarchDir() + sep + "retroarch")) {

                                    bool result;
                                    {   // scoped: the screen must be gone before menuSelection() recurses
                                        GuiConfirm confirm(*this);
                                        confirm.label = _("RetroArch is not installed");
                                        confirm.show();
                                        result = confirm.result;
                                    }
                                    if (result) {
                                        App::get().session().menuOption = MENU_OPTION_RETRO;
                                        menuVisible = false;
                                    } else {
                                        menuSelection();
                                        menuVisible = false;
                                    }
                                } else {
                                    App::get().library().exportToRetroArchPlaylist();
                                    App::get().session().menuOption = MENU_OPTION_RETRO;
                                    menuVisible = false;
                                }
                            };

                        if (e.button == Button::Cross) {
                            App::get().audio().cursor.play();
                            App::get().session().menuOption = MENU_OPTION_SCAN;

                            menuVisible = false;
                        };
                        if (e.button == Button::Triangle) {
                            App::get().audio().cursor.play();
                            {   // scoped: the screen must be gone before menuSelection() recurses
                                GuiAbout aboutScreen(*this);
                                aboutScreen.show();
                            }

                            menuSelection();
                            menuVisible = false;
                        };
                        if (e.button == Button::Select) {
                            App::get().audio().cursor.play();
                            {   // scoped: the screen must be gone before menuSelection() recurses
                                GuiOptions options(*this);
                                options.show();
                            }
                            menuSelection();
                            menuVisible = false;
                        };
                        if (!forceScan)
                            if (App::get().config().inifile.values["ui"] == "classic")
                                if (e.button == Button::Circle) {
                                    App::get().audio().cancel.play();
                                    App::get().session().menuOption = MENU_OPTION_SONY;
                                    menuVisible = false;
                                };
                        break;
                    } else {
                        if (e.button == Button::Square) {
                            App::get().audio().cursor.play();
                            App::get().audio().close();
                            input().flushPads();
#ifdef AB_DEBUG_HOST
                            drawText("Small delay to test");
                            platform().delay(2000);
#endif
                            string cmd = Env::getPathToAppsDir() + sep + "pscbios/run.sh";
                            Util::runAndWait(cmd, {});
                            input().flushEvents();
                            input().probePads();
                            App::get().audio().restart();
                            App::get().audio().playMusic();
                            menuSelection();
                            menuVisible = false;
                        };

                        if (e.button == Button::Cross) {
                            App::get().audio().cursor.play();
                            {   // scoped: the screen must be gone before menuSelection() recurses
                                GuiMemcards memcardsScreen(*this);
                                memcardsScreen.show();
                            }

                            menuSelection();
                            menuVisible = false;
                        };

                        if (e.button == Button::Circle) {
                            App::get().audio().cursor.play();
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
    App::get().audio().shutdown();
    backgroundImg = Texture();
}


//*******************************
// Gui::renderFreeSpace
//*******************************
void Gui::renderFreeSpace() {
    int x = atoi(App::get().theme().data.values["fsposx"].c_str());
    int y = atoi(App::get().theme().data.values["fsposy"].c_str());
    text_.renderText(themeFont, _("Free space") + " : " + Util::getAvailableSpace(), x, y);
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
        rect.x = atoi(App::get().theme().data.values["opscreenx"].c_str());
        rect.y = atoi(App::get().theme().data.values["opscreeny"].c_str());
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
    string bg = App::get().theme().data.values["text_bg"];

    renderer().setDrawColor(Color(TextRenderer::getR(bg), TextRenderer::getG(bg), TextRenderer::getB(bg),
                                  atoi(App::get().theme().data.values["textalpha"].c_str())));
    renderer().setBlendMode(ableem::BlendMode::Blend);
    Rect rect = text_.getTextRectOfTheme();
    renderer().fillRect(rect);

    int y = atoi(App::get().theme().data.values["ttop"].c_str());
    if (posy!=-1)
        y=posy; // override the bottom status y position.  so far this has never been used.

    text_.renderText(themeFont, text, 0, y, XALIGN_CENTER);
}

//*******************************
// Gui::renderTextBar
//*******************************
void Gui::renderTextBar() {
    string bg = App::get().theme().data.values["main_bg"];
    renderer().setDrawColor(Color(TextRenderer::getR(bg), TextRenderer::getG(bg), TextRenderer::getB(bg),
                                  atoi(App::get().theme().data.values["mainalpha"].c_str())));
    renderer().setBlendMode(ableem::BlendMode::Blend);

    Rect rect2 = text_.getOpscreenRectOfTheme();

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
