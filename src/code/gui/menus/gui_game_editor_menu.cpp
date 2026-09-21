//
// Created by screemer on 2019-01-25.
//

#include "gui_game_editor_menu.h"
#include "../gui.h"
#include "../screens/gui_keyboard.h"
#include "../screens/gui_select_memcard.h"
#include "../../core/main.h"
#include "../../core/services/environment.h"

using namespace std;

#define OPT_FIRST 5
#define OPT_FAVORITE 5
#define OPT_LIGHTGUN 6
#define OPT_PLAY_USING_RA 7
#define OPT_LOCK 8
#define OPT_HIGHRES 9
#define OPT_SPEEDHACK 10
#define OPT_SCANLINES 11
#define OPT_SCANLINELV 12
#define OPT_CLOCK_PSX 13
#define OPT_FRAMESKIP 14
#define OPT_PLUGIN 15
#define OPT_INTERPOLATION 16
#define OPT_BOOTLOGO 17
#define OPT_SMOOTHING 18 // pcsx-abnxt only
#define OPT_SONYHACKS 19 // pcsx-abnxt only
#define OPT_LAST 17
#define OPT_LAST_NXT 19

//*******************************
// GuiEditor::nxtEmulator / lastOption
//*******************************
bool GuiEditor::nxtEmulator() const {
    auto &values = app.config().inifile.values;
    auto it = values.find("emulator");
    return it != values.end() && it->second == "pcsx-abnxt";
}

int GuiEditor::lastOption() const {
    return nxtEmulator() ? OPT_LAST_NXT : OPT_LAST;
}

//*******************************
// GuiEditor::processOptionChange
//*******************************
// right (direction true) is "on" / "more", left is "off" / "less"
void GuiEditor::processOptionChange(bool direction) {
    GameSettingsService &svc = app.gameSettings();
    const PcsxSettings &pcsx = settings.pcsx;
    int step = direction ? 1 : -1;

    switch (selOption) {
    case OPT_FAVORITE:
        svc.setFavorite(settings, direction);
        break;

    case OPT_LIGHTGUN:
        svc.setLightgun(settings, direction); // on also switches Play using RA on
        break;

    case OPT_PLAY_USING_RA:
        if (gameData->lightgun)
            break; // a light-gun game plays in RetroArch, full stop
        svc.setPlayUsingRa(settings, direction);
        break;

    case OPT_LOCK:
        svc.setLocked(settings, direction);
        break;

    case OPT_HIGHRES:
        svc.setHighres(settings, direction);
        break;

    case OPT_SPEEDHACK:
        svc.setSpeedhack(settings, direction);
        break;

    case OPT_SCANLINES:
        svc.setScanlines(settings, direction);
        break;

    case OPT_SCANLINELV:
        svc.setScanlineLevel(settings, pcsx.scanlineLevel + step);
        break;

    case OPT_CLOCK_PSX:
        svc.setClock(settings, pcsx.clock + step);
        break;

    case OPT_FRAMESKIP:
        svc.setFrameskip(settings, pcsx.frameskip + step);
        break;

    case OPT_INTERPOLATION:
        svc.setInterpolation(settings, pcsx.interpolation + step);
        break;

    case OPT_PLUGIN:
        svc.setGpuPlugin(settings, direction ? GameSettingsService::PeopsGpu : GameSettingsService::BuiltinGpu);
        break;

    case OPT_BOOTLOGO:
        svc.setBootLogo(settings, direction);
        break;

    case OPT_SMOOTHING:
        svc.setSmoothing(settings, pcsx.smoothing + step);
        break;

    case OPT_SONYHACKS:
        svc.setSonyHacks(settings, direction);
        break;
    }
}

//*******************************
// GuiEditor::init
//*******************************
void GuiEditor::init() {
    settings = app.gameSettings().open(gameData);

    bool pngLoaded = false;
    for (const DirEntry &entry : DirEntry::diru(gameData->folder)) {
        if (DirEntry::matchExtension(entry.name, EXT_PNG)) {
            cover = ableem::Texture::loadFile(renderer, gameData->folder + sep + entry.name);
            pngLoaded = true;
        }
    }
    if (!pngLoaded) {
        cover = ableem::Texture::loadFile(renderer, Env::getWorkingPath() + sep + "default.png");
    }
}

//*******************************
// GuiEditor::render
//*******************************
void GuiEditor::render() {
    shared_ptr<Gui> gui(Gui::getInstance());
    const bool internal = settings.internal;
    IniFile &gameIni = settings.ini;
    const PcsxSettings &pcsx = settings.pcsx;

    int line = 0;
    gui->renderBackground();
    gui->renderTextBar();
    int yoffset = gui->renderHeader(gameIni.values["title"]);

    // Game.ini

    if (!internal) {
        gui->text().renderTextLine(_("Folder:") + " " + gameIni.entry, line++, yoffset, XALIGN_CENTER);
    } else {
        gui->text().renderTextLine(_("Folder:") + " " + gameData->folder, line++, yoffset, XALIGN_CENTER);
    }

    gui->text().renderTextLine(_("Published by:") + " " + gameIni.values["publisher"], line++, yoffset, XALIGN_CENTER);

    gui->text().renderTextLine(_("Year:") + " " + gameIni.values["year"] + "   " + _("Players") + ":" + " " +
                                   gameIni.values["players"],
                               line++, yoffset, XALIGN_CENTER);

    gui->text().renderTextLine(_("Memory Card:") + " " +
                                   (gameIni.values["memcard"] == "SONY"
                                        ? string(_("Internal"))
                                        : gameIni.values["memcard"] + " " + "(" + _("Custom") + ")"),
                               line++, yoffset, XALIGN_CENTER);

    if (gameData->internal) {
        gui->text().renderTextLineOptions(_("Favorite:") +
                                              (gameData->favorite ? string("|@Check|") : string("|@Uncheck|")),
                                          OPT_FAVORITE, yoffset, XALIGN_LEFT, 300);
    } else {
        gui->text().renderTextLineOptions(
            _("Favorite:") + (gameIni.values["favorite"] == "1" ? string("|@Check|") : string("|@Uncheck|")),
            OPT_FAVORITE, yoffset, XALIGN_LEFT, 300);
    }

    gui->text().renderTextLineOptions(_("Lightgun Game:") +
                                          (gameData->lightgun ? string("|@Check|") : string("|@Uncheck|")),
                                      OPT_LIGHTGUN, yoffset, XALIGN_LEFT, 300);

    if (gameData->internal || gameData->lightgun) {
        gui->text().renderTextLineOptions(_("Play using RA:") +
                                              (gameData->play_using_ra ? string("|@Check|") : string("|@Uncheck|")),
                                          OPT_PLAY_USING_RA, yoffset, XALIGN_LEFT, 300);
    } else {
        gui->text().renderTextLineOptions(_("Play using RA:") + (gameIni.values["play_using_ra"] == "true"
                                                                     ? string("|@Check|")
                                                                     : string("|@Uncheck|")),
                                          OPT_PLAY_USING_RA, yoffset, XALIGN_LEFT, 300);
    }

    // pcsx.cfg

    gui->text().renderTextLineOptions(
        _("Lock data:") + (gameIni.values["automation"] == "0" ? string("|@Check|") : string("|@Uncheck|")), OPT_LOCK,
        yoffset, XALIGN_LEFT, 300);

    gui->text().renderTextLineOptions(_("High res:") + (pcsx.highres == 1 ? string("|@Check|") : string("|@Uncheck|")),
                                      OPT_HIGHRES, yoffset, XALIGN_LEFT, 300);

    gui->text().renderTextLineOptions(_("SpeedHack:") +
                                          (pcsx.speedhack == 1 ? string("|@Check|") : string("|@Uncheck|")),
                                      OPT_SPEEDHACK, yoffset, XALIGN_LEFT, 300);

    gui->text().renderTextLineOptions(_("Scanlines:") +
                                          (pcsx.scanlines == 1 ? string("|@Check|") : string("|@Uncheck|")),
                                      OPT_SCANLINES, yoffset, XALIGN_LEFT, 300);

    gui->text().renderTextLineOptions(_("Scanline Level:") + " " + to_string(pcsx.scanlineLevel), OPT_SCANLINELV,
                                      yoffset, XALIGN_LEFT, 300);

    gui->text().renderTextLineOptions(_("Clock:") + " " + to_string(pcsx.clock), OPT_CLOCK_PSX, yoffset, XALIGN_LEFT,
                                      300);

    gui->text().renderTextLineOptions(_("Frameskip:") + " " + to_string(pcsx.frameskip), OPT_FRAMESKIP, yoffset,
                                      XALIGN_LEFT, 300);

    if (!internal) {
        gui->text().renderTextLineOptions(_("Plugin:") + " " + pcsx.gpu, OPT_PLUGIN, yoffset, XALIGN_LEFT, 300);
    }

    gui->text().renderTextLineOptions(_("Spu Interpolation:") + " " + to_string(pcsx.interpolation), OPT_INTERPOLATION,
                                      yoffset, XALIGN_LEFT, 300);

    gui->text().renderTextLineOptions(_("Boot logo:") +
                                          (pcsx.bootLogo != 0 ? string("|@Check|") : string("|@Uncheck|")),
                                      OPT_BOOTLOGO, yoffset, XALIGN_LEFT, 300);

    if (nxtEmulator()) {
        // pcsx-abnxt's software scaler (its menu's "Smoothing"); the classic pcsx-ab ignores the key
        gui->text().renderTextLineOptions(_("Smoothing:") + " " + GameSettingsService::SmoothingNames[pcsx.smoothing],
                                          OPT_SMOOTHING, yoffset, XALIGN_LEFT, 300);
        // Sony's per-title overrides (the console's emulator had them) for the disc's serial; off unless a
        // game asks for them
        gui->text().renderTextLineOptions(_("Sony hacks:") +
                                              (pcsx.sonyHacks ? string("|@Check|") : string("|@Uncheck|")),
                                          OPT_SONYHACKS, yoffset, XALIGN_LEFT, 300);
    }

    gui->text().renderSelectionBox(selOption, yoffset, 300);

    string guiMenu = "|@T| " + _("Rename");

    if (!internal) {
        guiMenu += "  |@S| " + _("Change MC") + " ";

        if (gameIni.values["memcard"] == "SONY") {
            guiMenu += "|@Start| " + _("Share MC") + "  ";
        }
    }

    guiMenu += " |@O| " + _("Go back") + "|";

    gui->renderStatus(guiMenu);

    ableem::Rect rect;
    rect.x = app.theme().classic().editorCover.x;
    rect.y = app.theme().classic().editorCover.y;
    rect.w = 226;
    rect.h = 226;

    renderer.copy(cover, nullptr, &rect);

    renderer.present();
}

//*******************************
// GuiEditor::loop
//*******************************
void GuiEditor::loop() {
    shared_ptr<Gui> gui(Gui::getInstance());
    const bool internal = settings.internal;
    IniFile &gameIni = settings.ini;

    menuVisible = true;
    while (menuVisible) {
        Event e;
        while (gui->input().poll(e)) {
            // this is for pc Only
            if (e.type == Event::Type::Quit) {
                menuVisible = false;
            }
            switch (e.type) {
            case Event::Type::DpadDown: /* Handle Joystick Motion */
            case Event::Type::DpadUp:

                if (gui->input().dpadDown()) {
                    do {
                        app.audio().cursor.play();
                        selOption++;
                        if (selOption > lastOption()) {
                            selOption = lastOption();
                        }
                        render();
                    } while (fastForwardUntilAnotherEvent(120));
                }
                if (gui->input().dpadUp()) {
                    do {
                        app.audio().cursor.play();
                        selOption--;
                        if (selOption < OPT_FIRST) {
                            selOption = OPT_FIRST;
                        }
                        render();
                    } while (fastForwardUntilAnotherEvent(120));
                }

                if (gui->input().dpadRight()) {
                    do {
                        app.audio().cursor.play();
                        processOptionChange(true);
                        render();
                    } while (fastForwardUntilAnotherEvent(80));
                }
                if (gui->input().dpadLeft()) {
                    do {
                        app.audio().cursor.play();
                        processOptionChange(false);
                        render();
                    } while (fastForwardUntilAnotherEvent(80));
                }
                break;

            case Event::Type::ButtonDown:
                if (!internal) {
                    if (gameIni.values["memcard"] == "SONY") {
                        if (e.button == Button::Start) {
                            app.audio().cursor.play();
                            GuiKeyboard keyboard(*gui);
                            keyboard.label = _("Enter new name for memory card");
                            keyboard.result = gameIni.values["title"];
                            keyboard.show();
                            string result = keyboard.result;
                            bool cancelled = keyboard.cancelled;

                            if (result.empty()) {
                                cancelled = true;
                            }

                            if (!cancelled) {
                                string savePath =
                                    Env::getPathToSaveStatesDir() + sep + gameIni.entry + sep + "memcards";
                                app.memcards().storeGameCardsAsSet(savePath, result);
                                app.gameSettings().setMemcard(settings, result);
                            }
                        };
                    }
                } else {
                    app.audio().cancel.play();
                }

                if (e.button == Button::Square) {
                    if (!internal) {
                        app.audio().cursor.play();
                        GuiSelectMemcard selector(*gui);
                        selector.cardSelected = gameIni.values["memcard"];
                        selector.show();

                        if (selector.selected != -1) {
                            if (selector.selected == 0) {
                                app.gameSettings().setMemcard(settings, "SONY");
                            } else {
                                app.gameSettings().setMemcard(settings, selector.cards[selector.selected]);
                            }
                        }
                    } else {
                        app.audio().cancel.play();
                    }
                };

                if (e.button == Button::Circle) {
                    app.audio().cancel.play();
                    cover = ableem::Texture();
                    menuVisible = false;
                };

                if (e.button == Button::Triangle) {
                    app.audio().cursor.play();
                    GuiKeyboard keyboard(*gui);
                    keyboard.label = _("Enter new game name");
                    keyboard.result = gameIni.values["title"];
                    keyboard.show();
                    string result = keyboard.result;
                    bool cancelled = keyboard.cancelled;

                    if (result.empty()) {
                        cancelled = true;
                    }

                    if (!cancelled) {
                        if (!internal) {
                            app.gameSettings().rename(settings, result);
                        } else {
                            lastName = result;
                        }
                        changes = true;
                    }
                };
                break;
            default:
                break;
            }
        }
        render();
    }
}
