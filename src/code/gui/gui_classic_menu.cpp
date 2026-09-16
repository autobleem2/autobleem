//
// ClassicMenuScreen: what Gui::menuSelection() used to be.
//
#include "gui_classic_menu.h"
#include "gui_about.h"
#include "gui_confirm.h"
#include "menus/gui_optionsMenu.h"
#include "menus/gui_memCardsMenu.h"
#include "menus/gui_gameManagerMenu.h"
#include "../launcher/gui_launcher.h"
#include "../core/lang.h"
#include "../core/environment.h"
#include "../core/util.h"

#include <unistd.h>

using namespace std;

//*******************************
// ClassicMenuScreen::init
//*******************************
void ClassicMenuScreen::init() {
    if (!app.library().covers().hasAnyRegion()) {
        gui->criticalException(_("WARNING: NO COVER DB FOUND. PRESS ANY BUTTON."));
    }
    otherMenuShift = false;
    powerOffShift = false;
    forceScan = app.session().forceScan;

    mainMenu = "|@Start| " + _("AutoBleem") + "    |@X|  " + _("Re/Scan") + " ";
    if (app.config().inifile.values["ui"] == "classic") {
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

    forceScanMenu = _("Games changed. Press") + "  |@X|  " + _("to scan") + "|";

    otherMenu = "|@S|  " + _("Hardware Information") + "  ";
    otherMenu += "|@X|  " + _("Memory Cards") + "   |@O|  " + _("Game Manager");

    gamepadNotice = "";
    if (gui->input().joystickCount() > gui->input().activePadCount()) {
        gamepadNotice = _(
                "NOTICE: At least one connected gamepad is not recognized. Use Hardware Information page to setup.");
    }
}

//*******************************
// ClassicMenuScreen::render
//*******************************
void ClassicMenuScreen::render() {
    drawMenu();
}

//*******************************
// ClassicMenuScreen::drawMenu
//*******************************
void ClassicMenuScreen::drawMenu() {
    if (!forceScan) {
        gui->drawText(mainMenu, gamepadNotice);
    } else {
        gui->drawText(forceScanMenu, gamepadNotice);
    }
}

//*******************************
// ClassicMenuScreen::restart
//*******************************
void ClassicMenuScreen::restart() {
    init();
    render();
}

//*******************************
// ClassicMenuScreen::showLauncher
//*******************************
void ClassicMenuScreen::showLauncher() {
    GuiLauncher launcherScreen(*gui);
    launcherScreen.show();
}

//*******************************
// ClassicMenuScreen::powerOff
//*******************************
void ClassicMenuScreen::powerOff() {
    gui->drawText(_("POWERING OFF... PLEASE WAIT"));
#ifdef AB_DEBUG_HOST
    exit(0);
#else
    Util::execUnixCommand("shutdown -h now");
    sync();
    exit(1);
#endif
}

//*******************************
// ClassicMenuScreen::loop
//*******************************
void ClassicMenuScreen::loop() {
    menuVisible = true;
    while (menuVisible) {
        if (app.session().startingGame) {
            gui->drawText(app.session().runningGame->title);
            app.session().menuOption = MENU_OPTION_START;
            menuVisible = false;
            app.session().startingGame = false;
            return;
        }

        if (app.session().resumingGui) {
            showLauncher();
            gui->drawText("");
            app.session().resumingGui = false;
            restart();
            continue;
        }

        // a sub-screen was shown: the menu is drawn afresh and the loop starts over, which is also where
        // a game the launcher asked to start is picked up
        bool restarted = false;
        Event e;
        while (!restarted && gui->input().poll(e)) {
            // this is for pc Only
            if (e.type == Event::Type::Quit) {
                menuVisible = false;
            }
            switch (e.type) {

                case Event::Type::ButtonUp:
                    if (!forceScan) {
                        if (e.button == Button::L1) {
                            app.audio().cursor.play();
                            gui->drawText(mainMenu);
                            otherMenuShift = false;
                        }
                        if (e.button == Button::L2) {
                            app.audio().cursor.play();
                            powerOffShift = false;
                        }
                    }
                    break;
                case Event::Type::ButtonDown:
                    if (!forceScan) {
                        if (e.button == Button::L1) {
                            app.audio().cursor.play();
                            gui->drawText(otherMenu);
                            otherMenuShift = true;
                        }
                        if (e.button == Button::L2) {
                            app.audio().cursor.play();
                            powerOffShift = true;
                        }
                    }

                    if (powerOffShift) {
                        if (e.button == Button::R2) {
                            app.audio().cursor.play();
                            powerOff();
                        };
                    }

                    if (!otherMenuShift) {
                        if (!forceScan)
                            if (e.button == Button::Start) {
                                if (app.config().inifile.values["ui"] == "classic") {
                                    app.audio().cursor.play();
                                    app.session().menuOption = MENU_OPTION_RUN;
                                    menuVisible = false;
                                } else {
                                    app.audio().cursor.play();
                                    gui->drawText(_("Starting EvolutionUI"));
                                    gui->loadAssets(false);
                                    showLauncher();
                                    restart();
                                    restarted = true;
                                }
                            };

                        if (!forceScan)
                            if (e.button == Button::Square) {
                                app.audio().cursor.play();
                                if (!DirEntry::exists(Env::getPathToRetroarchDir() + sep + "retroarch")) {
                                    bool result;
                                    {
                                        GuiConfirm confirm(*gui);
                                        confirm.label = _("RetroArch is not installed");
                                        confirm.show();
                                        result = confirm.result;
                                    }
                                    if (result) {
                                        app.session().menuOption = MENU_OPTION_RETRO;
                                        menuVisible = false;
                                    } else {
                                        restart();
                                        restarted = true;
                                    }
                                } else {
                                    app.library().exportToRetroArchPlaylist();
                                    app.session().menuOption = MENU_OPTION_RETRO;
                                    menuVisible = false;
                                }
                            };

                        if (e.button == Button::Cross) {
                            app.audio().cursor.play();
                            app.session().menuOption = MENU_OPTION_SCAN;

                            menuVisible = false;
                        };
                        if (e.button == Button::Triangle) {
                            app.audio().cursor.play();
                            {
                                GuiAbout aboutScreen(*gui);
                                aboutScreen.show();
                            }
                            restart();
                            restarted = true;
                        };
                        if (e.button == Button::Select) {
                            app.audio().cursor.play();
                            {
                                GuiOptions options(*gui);
                                options.show();
                            }
                            restart();
                            restarted = true;
                        };
                        if (!forceScan)
                            if (app.config().inifile.values["ui"] == "classic")
                                if (e.button == Button::Circle) {
                                    app.audio().cancel.play();
                                    app.session().menuOption = MENU_OPTION_SONY;
                                    menuVisible = false;
                                };
                        break;
                    } else {
                        if (e.button == Button::Square) {
                            app.audio().cursor.play();
                            app.audio().close();
                            gui->input().flushPads();
#ifdef AB_DEBUG_HOST
                            gui->drawText("Small delay to test");
                            gui->platform().delay(2000);
#endif
                            string cmd = Env::getPathToAppsDir() + sep + "pscbios/run.sh";
                            Util::runAndWait(cmd, {});
                            gui->input().flushEvents();
                            gui->input().probePads();
                            app.audio().restart();
                            app.audio().playMusic();
                            restart();
                            restarted = true;
                        };

                        if (e.button == Button::Cross) {
                            app.audio().cursor.play();
                            {
                                GuiMemcards memcardsScreen(*gui);
                                memcardsScreen.show();
                            }
                            restart();
                            restarted = true;
                        };

                        if (e.button == Button::Circle) {
                            app.audio().cursor.play();
                            {
                                GuiManager managerScreen(*gui);
                                managerScreen.show();
                            }
                            restart();
                            restarted = true;
                        };
                    }
                    break;
                default:
                    break;
            }
        }
    }
}
