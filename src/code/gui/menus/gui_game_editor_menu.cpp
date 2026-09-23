//
// Created by screemer on 2019-01-25.
//

#include "gui_game_editor_menu.h"
#include "gui/gui.h"
#include "gui/screens/gui_keyboard.h"
#include "../screens/gui_select_memcard.h"
#include "core/main.h"
#include "core/services/environment.h"

using namespace std;

#define OPT_FIRST 0
#define OPT_FAVORITE 0
#define OPT_LIGHTGUN 1
#define OPT_PLAY_USING_RA 2
#define OPT_LOCK 3
#define OPT_HIGHRES 4
#define OPT_SPEEDHACK 5
#define OPT_SCANLINES 6
#define OPT_SCANLINELV 7
#define OPT_CLOCK_PSX 8
#define OPT_FRAMESKIP 9
#define OPT_PLUGIN 10
#define OPT_INTERPOLATION 11
#define OPT_BOOTLOGO 12
#define OPT_SMOOTHING 13 // pcsx-abnxt only
#define OPT_SONYHACKS 14 // pcsx-abnxt only

//*******************************
// GuiEditor::nxtEmulator
//*******************************
bool GuiEditor::nxtEmulator() const {
    auto &values = app.config().inifile.values;
    auto it = values.find("emulator");
    return it != values.end() && it->second == "pcsx-abnxt";
}

//*******************************
// GuiEditor::buildRows / selectedRow / moveSelection
//*******************************
void GuiEditor::buildRows() {
    const bool internal = settings.internal;
    IniFile &gameIni = settings.ini;
    const PcsxSettings &pcsx = settings.pcsx;
    rows.clear();
    auto heading = [&](const string &label) { rows.push_back({Row::Kind::Heading, label, "", false, -1}); };
    auto boolRow = [&](const string &label, bool on, int opt) {
        rows.push_back({Row::Kind::Bool, label, "", on, opt});
    };
    auto valueRow = [&](const string &label, const string &value, int opt) {
        rows.push_back({Row::Kind::Value, label, value, false, opt});
    };

    heading(_("Game"));
    boolRow(_("Favorite:"), gameData->internal ? gameData->favorite : gameIni.values["favorite"] == "1", OPT_FAVORITE);
    boolRow(_("Lightgun Game:"), gameData->lightgun, OPT_LIGHTGUN);
    boolRow(_("Play using RA:"),
            (gameData->internal || gameData->lightgun) ? gameData->play_using_ra
                                                       : gameIni.values["play_using_ra"] == "true",
            OPT_PLAY_USING_RA);
    boolRow(_("Lock data:"), gameIni.values["automation"] == "0", OPT_LOCK);

    heading(_("Video"));
    boolRow(_("High res:"), pcsx.highres == 1, OPT_HIGHRES);
    boolRow(_("Scanlines:"), pcsx.scanlines == 1, OPT_SCANLINES);
    valueRow(_("Scanline Level:"), to_string(pcsx.scanlineLevel), OPT_SCANLINELV);
    valueRow(_("Frameskip:"), to_string(pcsx.frameskip), OPT_FRAMESKIP);
    if (!internal)
        valueRow(_("Plugin:"), pcsx.gpu, OPT_PLUGIN);
    if (nxtEmulator()) // pcsx-abnxt's software scaler (its menu's "Smoothing"); the classic pcsx-ab ignores the key
        valueRow(_("Smoothing:"), GameSettingsService::SmoothingNames[pcsx.smoothing], OPT_SMOOTHING);

    heading(_("Emulator"));
    boolRow(_("SpeedHack:"), pcsx.speedhack == 1, OPT_SPEEDHACK);
    valueRow(_("Clock:"), to_string(pcsx.clock), OPT_CLOCK_PSX);
    valueRow(_("Spu Interpolation:"), to_string(pcsx.interpolation), OPT_INTERPOLATION);
    boolRow(_("Boot logo:"), pcsx.bootLogo != 0, OPT_BOOTLOGO);
    if (nxtEmulator()) // Sony's per-title overrides (the console's emulator had them); off unless a game asks
        boolRow(_("Sony hacks:"), pcsx.sonyHacks, OPT_SONYHACKS);
}

int GuiEditor::selectedRow() const {
    for (size_t i = 0; i < rows.size(); i++)
        if (rows[i].opt == selOption)
            return static_cast<int>(i);
    return -1;
}

void GuiEditor::moveSelection(int step) {
    int i = selectedRow();
    if (i < 0)
        i = step > 0 ? -1 : static_cast<int>(rows.size());
    for (int j = i + step; j >= 0 && j < static_cast<int>(rows.size()); j += step) {
        if (rows[j].opt >= 0) {
            selOption = rows[j].opt;
            return;
        }
    }
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

    gui->renderBackground();
    gui->renderTextBar();
    int yoffset = gui->renderHeader(gui->text().elide(gui->assets().themeFonts[FONT_28_BOLD], gameIni.values["title"],
                                                      gui->classicPanel().w - 2 * PanelStyle::RowInset));

    // the pane on the right: the cover and the game's facts
    pane.cover = cover;
    pane.facts.clear();
    pane.facts.emplace_back(_("Published by:"), gameIni.values["publisher"]);
    pane.facts.emplace_back(_("Year:"), gameIni.values["year"]);
    pane.facts.emplace_back(_("Players"), gameIni.values["players"]);
    pane.facts.emplace_back(_("Folder:"), internal ? gameData->folder : gameIni.entry);
    pane.facts.emplace_back(_("Memory Card:"), gameIni.values["memcard"] == "SONY"
                                                   ? string(_("Internal"))
                                                   : gameIni.values["memcard"] + " (" + _("Custom") + ")");
    pane.render(*gui);

    // the option rows on the left, their switch or value at the pane's edge; as many as fit, the rest
    // scroll with the cursor
    buildRows();
    const int right = GameDetailPane::rowsRight(*gui);
    const ableem::Font &font = gui->assets().themeFont;
    const int fit = gui->classicRowsThatFit(font);
    const int total = static_cast<int>(rows.size());
    const int sel = selectedRow();
    if (sel >= 0) {
        if (sel < firstVisible)
            firstVisible = sel;
        if (sel >= firstVisible + fit)
            firstVisible = sel - fit + 1;
        // a heading right above the selected row comes along, so a group is never headless at the top
        if (firstVisible > 0 && firstVisible == sel && rows[sel - 1].opt < 0 && sel < firstVisible + fit)
            firstVisible--;
    }
    firstVisible = max(0, min(firstVisible, max(0, total - fit)));
    for (int i = firstVisible, line = 0; i < total && line < fit; i++, line++) {
        const Row &row = rows[i];
        if (row.kind == Row::Kind::Heading) {
            gui->text().renderLabelBox(line, yoffset, right);
            gui->text().renderTextLine(row.label, line, yoffset, XALIGN_LEFT);
            continue;
        }
        if (row.opt == selOption)
            gui->text().renderSelectionBox(line, yoffset, 0, ableem::Font(), right);
        if (row.kind == Row::Kind::Bool) {
            gui->text().renderTextLineOptions(row.label + (row.on ? string("|@Check|") : string("|@Uncheck|")), line,
                                              yoffset, XALIGN_LEFT, 0, right);
        } else {
            gui->text().renderTextLine(row.label, line, yoffset, XALIGN_LEFT);
            gui->text().renderRowValue(row.value, line, yoffset, right);
        }
    }
    gui->renderScrollMarkers(firstVisible > 0, firstVisible + fit < total);

    string guiMenu = "|@T| " + _("Rename");
    if (!internal) {
        guiMenu += "  |@S| " + _("Change memory card") + " ";
        if (gameIni.values["memcard"] == "SONY") {
            guiMenu += "|@Start| " + _("Share memory card") + "  ";
        }
    }
    guiMenu += " |@O| " + _("Back") + "|";
    gui->renderStatus(guiMenu);

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
                        moveSelection(1);
                        render();
                    } while (fastForwardUntilAnotherEvent(120));
                }
                if (gui->input().dpadUp()) {
                    do {
                        app.audio().cursor.play();
                        moveSelection(-1);
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
