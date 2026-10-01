//
// Created by screemer on 2019-01-25.
//

#include "gui_game_editor_menu.h"
#include "gui/gui.h"
#include "gui/screens/gui_confirm.h"
#include "gui/screens/gui_keyboard.h"
#include "../screens/gui_select_memcard.h"
#include "core/main.h"
#include "core/services/environment.h"

#include <ableem/ui/debug_driver.h>

#include <typeinfo>

using namespace std;

#define OPT_FIRST 0
#define OPT_FAVORITE 0
#define OPT_LIGHTGUN 1
#define OPT_PLAY_USING_RA 2
#define OPT_LOCK 3
#define OPT_HIGHRES 4 // Resolution: the built-in NEON GPU only
#define OPT_SPEEDHACK 5
#define OPT_SCANLINES 6
#define OPT_SCANLINELV 7
#define OPT_CLOCK_PSX 8
#define OPT_FRAMESKIP 9
#define OPT_PLUGIN 10
#define OPT_INTERPOLATION 11
#define OPT_BOOTLOGO 12
#define OPT_SMOOTHING 13
#define OPT_SONYHACKS 14 // pcsx-abnxt only
#define OPT_FILTER 15
#define OPT_UNLOCK 16  // only while the game has its own config
#define OPT_NOSEAMS 17 // Remove seams: with Resolution
#define OPT_DITHERING 18

namespace {

// pcsx-abnxt's names for its values (ab_menu.c, ab_shaders.c), through the language files; a literal each,
// so tools/lang_tools.py finds them
string filterName(int filter) {
    switch (filter) {
    case 0:
        return _("Nearest");
    case 1:
        return _("Linear");
    case 2:
        return _("Sharp");
    case 3:
        return _("Sharp (simple)");
    case 4:
        return _("Quilez");
    case 5:
        return _("CRT (fast)");
    case 6:
        return _("CRT-Pi");
    default:
        return to_string(filter);
    }
}

string ditheringName(int mode) {
    switch (mode) {
    case 0:
        return _("Off");
    case 2:
        return _("Always");
    default:
        return _("On");
    }
}

string smoothingName(int mode) {
    if (mode <= 0 || mode >= GameSettingsService::SmoothingCount)
        return _("None");
    return GameSettingsService::SmoothingNames[mode]; // the scalers' own names
}

// pcsx-abnxt's CRT filters draw their own scanlines (ab_filter_is_crt)
bool crtFilter(int filter) {
    return filter == 5 || filter == 6;
}

} // namespace

//*******************************
// GuiEditor::nxtEmulator
//*******************************
bool GuiEditor::nxtEmulator() const {
    auto &values = app.config().inifile.values;
    auto it = values.find("emulator");
    return it != values.end() && it->second == "pcsx-abnxt";
}

//*******************************
// GuiEditor::publishToDriver
//*******************************
// the DebugDriver's `items` and `selected`: the rows as drawn, a heading band with a leading '#' (translated
// labels), the cursor's row index among them
void GuiEditor::publishToDriver(int selectedIndex) const {
    if (!menuVisible || !ableem::DebugDriver::active())
        return;
    vector<string> names;
    for (const Row &row : rows)
        names.push_back((row.kind == Row::Kind::Heading ? "#" : "") + row.label);
    ableem::DebugDriver::publish(typeid(*this).name(), names, selectedIndex);
}

//*******************************
// GuiEditor::buildRows / selectedRow / moveSelection
//*******************************
void GuiEditor::buildRows() {
    const bool internal = settings.internal;
    IniFile &gameIni = settings.ini;
    const PcsxSettings &pcsx = settings.pcsx;
    const string platform = Env::platformName();
    const bool nxt = nxtEmulator();
    rows.clear();
    auto heading = [&](const string &label) { rows.push_back({Row::Kind::Heading, label, "", false, -1}); };
    // `greyed`: shown, not changeable (the emulator does not read it, or another setting rules it out)
    auto boolRow = [&](const string &label, bool on, int opt, bool greyed = false) {
        rows.push_back({Row::Kind::Bool, label, "", on, opt, greyed});
    };
    auto valueRow = [&](const string &label, const string &value, int opt, bool greyed = false) {
        rows.push_back({Row::Kind::Value, label, value, false, opt, greyed});
    };

    heading(_("Game"));
    boolRow(_("Favorite:"), gameData->internal ? gameData->favorite : gameIni.values["favorite"] == "1", OPT_FAVORITE);
    boolRow(_("Lightgun game:"), gameData->lightgun, OPT_LIGHTGUN);
    boolRow(_("Play using RA:"),
            (gameData->internal || gameData->lightgun) ? gameData->play_using_ra
                                                       : gameIni.values["play_using_ra"] == "true",
            OPT_PLAY_USING_RA);
    boolRow(_("Lock data:"), gameIni.values["automation"] == "0", OPT_LOCK);

    // the game has its own config, saved in an emulator's menu: the rows below show its values, greyed,
    // until the settings are unlocked (PcsxConfig)
    const size_t firstPcsxRow = rows.size() + (settings.custom ? 2 : 0);
    if (settings.custom) {
        heading(_("Saved in the emulator"));
        valueRow(_("Unlock the settings"), "", OPT_UNLOCK);
    }

    // pcsx-abnxt's in-game menu's Picture rows, in its order, with its values for this platform; its Scaling
    // and Display (the output mode) are global Options. The classic pcsx-ab reads no dithering2 and no
    // enhancement_no_seams, and its soft_filter is upstream's of 2017 (no HQ2x/HQ3x, NEON builds only):
    // those rows are greyed with it selected.
    heading(_("Display"));
    // the built-in NEON GPU's 2x (no line in Gpu3 = the built-in one)
    const bool neon = GameSettingsService::neonGpuFor(platform, nxt) &&
                      (pcsx.gpu.empty() || pcsx.gpu == GameSettingsService::BuiltinGpu);
    if (neon) {
        valueRow(_("Resolution:"), pcsx.highres != 0 ? "2x" : "1x", OPT_HIGHRES);
        // only with 2x, as the emulator's menu has it
        boolRow(_("Remove seams:"), pcsx.noSeams != 0, OPT_NOSEAMS, !nxt || pcsx.highres == 0);
    }
    valueRow(_("Dithering:"), ditheringName(pcsx.dither), OPT_DITHERING, !nxt);
    // a CRT filter draws its own scanlines, and on the console it rules out the smoothing too
    const bool crt = nxt && crtFilter(pcsx.filter);
    valueRow(_("Smoothing:"), smoothingName(pcsx.smoothing), OPT_SMOOTHING, !nxt || (crt && platform == "psc"));
    // the classic pcsx-ab and RetroArch have nearest and bilinear only: every filter but Linear is nearest
    // there (LaunchService)
    valueRow(_("Filter:"), filterName(pcsx.filter), OPT_FILTER);
    valueRow(_("Scanlines:"), pcsx.scanlines == 0 ? _("Off") : to_string(pcsx.scanlines), OPT_SCANLINES, crt);
    valueRow(_("Scanline brightness:"), to_string(pcsx.scanlineLevel), OPT_SCANLINELV, crt);

    // what does not fit the emulator's Picture section
    heading(_("Rendering"));
    if (!internal)
        valueRow(_("Plugin:"), pcsx.gpu, OPT_PLUGIN);
    // the emulators' own setting and names (men_frameskip): Auto, Off, then how many frames are skipped
    const string frameskipNames[GameSettingsService::FrameskipCount] = {_("Auto"), _("Off"), "1", "2", "3"};
    valueRow(_("Frameskip:"), frameskipNames[pcsx.frameskip], OPT_FRAMESKIP);

    heading(_("Emulator"));
    boolRow(_("Speedhack:"), pcsx.speedhack == 1, OPT_SPEEDHACK);
    valueRow(_("Clock:"), to_string(pcsx.clock), OPT_CLOCK_PSX);
    valueRow(_("Spu interpolation:"), to_string(pcsx.interpolation), OPT_INTERPOLATION);
    boolRow(_("Boot logo:"), pcsx.bootLogo != 0, OPT_BOOTLOGO);
    if (nxt) // Sony's per-title overrides (the console's emulator had them); off unless a game asks
        boolRow(_("Sony hacks:"), pcsx.sonyHacks, OPT_SONYHACKS);

    if (settings.custom) {
        for (size_t i = firstPcsxRow; i < rows.size(); i++)
            rows[i].locked = rows[i].locked || rows[i].opt >= 0;
    }
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
    // a locked row can be landed on - the list scrolls with the cursor, and its values are worth reading -
    // but not changed (processOptionChange)
    for (int j = i + step; j >= 0 && j < static_cast<int>(rows.size()); j += step) {
        if (rows[j].opt >= 0) {
            selOption = rows[j].opt;
            return;
        }
    }
}

// the option row at `index` (a heading is no place for the cursor): the next one in `dir`'s direction, else
// the nearest the other way
void GuiEditor::selectNear(int index, int dir) {
    const int size = static_cast<int>(rows.size());
    index = max(0, min(index, size - 1));
    for (int j = index; j >= 0 && j < size; j += dir) {
        if (rows[j].opt >= 0) {
            selOption = rows[j].opt;
            return;
        }
    }
    for (int j = index; j >= 0 && j < size; j -= dir) {
        if (rows[j].opt >= 0) {
            selOption = rows[j].opt;
            return;
        }
    }
}

void GuiEditor::pageSelection(int dir) {
    const int fit = gui->classicRowsThatFit(gui->assets().themeFont);
    selectNear(selectedRow() + dir * fit, dir);
}

//*******************************
// GuiEditor::unlockSettings
//*******************************
void GuiEditor::unlockSettings() {
    shared_ptr<Gui> gui(Gui::getInstance());
    GuiConfirm confirm(*gui);
    confirm.label = _("Delete the settings saved in the emulator and use AutoBleem's again?");
    confirm.confirmLabel = _("Delete");
    confirm.show();
    if (!confirm.result)
        return;
    app.gameSettings().unlock(settings);
    // the unlock row is gone; the cursor lands on the first of the rows it freed, the one after Lock data
    // (Resolution is not on every platform)
    buildRows();
    selOption = OPT_LOCK;
    moveSelection(1);
}

//*******************************
// GuiEditor::processOptionChange
//*******************************
// right (direction true) is "on" / "more", left is "off" / "less"
void GuiEditor::processOptionChange(bool direction) {
    GameSettingsService &svc = app.gameSettings();
    const PcsxSettings &pcsx = settings.pcsx;
    const string platform = Env::platformName();
    int step = direction ? 1 : -1;
    int sel = selectedRow();
    if (sel >= 0 && rows[sel].locked)
        return; // the game's own config speaks for it, or the row is greyed (buildRows)

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

    case OPT_HIGHRES: // 1x / 2x
        svc.setHighres(settings, direction);
        break;

    case OPT_NOSEAMS:
        svc.setNoSeams(settings, direction);
        break;

    case OPT_DITHERING: // Off / On / Always
        svc.setDithering(settings, pcsx.dither + step);
        break;

    case OPT_SPEEDHACK:
        svc.setSpeedhack(settings, direction);
        break;

    case OPT_SCANLINES: // Off / 1 / 2 / 3
        svc.setScanlines(settings, pcsx.scanlines + step);
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

    case OPT_SMOOTHING: // this platform's scalers
        svc.setSmoothing(
            settings, GameSettingsService::stepIn(GameSettingsService::smoothingsFor(platform), pcsx.smoothing, step));
        break;

    case OPT_SONYHACKS:
        svc.setSonyHacks(settings, direction);
        break;

    case OPT_FILTER: // this platform's filters
        svc.setFilter(settings,
                      GameSettingsService::stepIn(GameSettingsService::filtersFor(platform), pcsx.filter, step));
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
// GuiEditor::draw
//*******************************
void GuiEditor::draw() {
    shared_ptr<Gui> gui(Gui::getInstance());
    const bool internal = settings.internal;
    IniFile &gameIni = settings.ini;
    const PcsxSettings &pcsx = settings.pcsx;

    gui->renderBackground();
    gui->renderTextBar();
    // a game with no title in its metadata (an empty Game.ini "title=", or - internal games - an unrecognised
    // one) falls back to the folder name rather than showing a blank header (UIREV-18)
    string title = gameIni.values["title"];
    if (title.empty())
        title = internal ? DirEntry::getFileNameFromPath(DirEntry::removeSeparatorFromEndOfPath(gameData->folder))
                         : gameIni.entry;
    int yoffset = gui->renderHeader(gui->text().elide(gui->assets().themeFonts[FONT_28_BOLD], title,
                                                      gui->classicPanel().w - 2 * PanelStyle::RowInset));

    // the pane on the right: the cover and the game's facts
    pane.cover = cover;
    pane.facts.clear();
    pane.facts.emplace_back(_("Published by:"), gameIni.values["publisher"]);
    // year 0 and a players count of 0 both mean "unknown" here, not a real value - skip the row rather
    // than show a fact that reads as broken; pane.render draws whatever facts are in the vector, so
    // leaving one out closes the gap by itself
    if (!gameIni.values["year"].empty() && gameIni.values["year"] != "0")
        pane.facts.emplace_back(_("Year:"), gameIni.values["year"]);
    if (!gameIni.values["players"].empty() && gameIni.values["players"] != "0")
        pane.facts.emplace_back(_("Players:"), gameIni.values["players"]);
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
    publishToDriver(sel);
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
    // a theme's selection frame goes under every row's text, so it is drawn before all of them - its bleed would
    // cover the row above otherwise (G4d); without a frame the band is drawn with its row, as before
    const bool framed = gui->text().selectionFramed(gui->uiContext());
    if (framed) {
        for (int i = firstVisible, line = 0; i < total && line < fit; i++, line++) {
            if (rows[i].kind != Row::Kind::Heading && rows[i].opt == selOption) {
                gui->text().renderSelectionBox(gui->uiContext(), line, yoffset, 0, ableem::Font(), right);
                break;
            }
        }
    }
    for (int i = firstVisible, line = 0; i < total && line < fit; i++, line++) {
        const Row &row = rows[i];
        if (row.kind == Row::Kind::Heading) {
            gui->text().renderLabelBox(gui->uiContext(), line, yoffset, right);
            TextRenderer::RowRoleScope role(gui->text(), TextRenderer::RowRole::Heading);
            gui->text().renderTextLine(row.label, line, yoffset, XALIGN_LEFT);
            continue;
        }
        // the theme's roles (UIREV-29): the selected row bright, the others dim
        // a locked row is under the theme's `disabled` role when it has one (G5t): its text in `description`
        const bool lockedRole = row.locked && gui->uiContext().disabledVeil().set;
        TextRenderer::RowRoleScope role(gui->text(), lockedRole             ? TextRenderer::RowRole::Disabled
                                                     : row.opt == selOption ? TextRenderer::RowRole::Selected
                                                                            : TextRenderer::RowRole::Row);
        if (row.opt == selOption && !framed)
            gui->text().renderSelectionBox(gui->uiContext(), line, yoffset, 0, ableem::Font(), right);
        if (row.kind == Row::Kind::Bool) {
            gui->text().renderTextLineOptions(row.label + (row.on ? string("|@Check|") : string("|@Uncheck|")), line,
                                              yoffset, XALIGN_LEFT, 0, right);
        } else {
            gui->text().renderTextLine(row.label, line, yoffset, XALIGN_LEFT);
            gui->text().renderRowValue(row.value, line, yoffset, right);
        }
        if (row.locked)
            gui->text().renderDisabledBox(gui->uiContext(), line, yoffset, right);
    }
    gui->renderScrollMarkers(firstVisible > 0, firstVisible + fit < total);

    string guiMenu = "|@L1/R1| " + _("First/last") + "   |@L2/R2| " + _("Page") + "   ";
    if (selOption != OPT_UNLOCK) // the unlock row has no value to choose
        guiMenu += "|@Left+Right| " + _("Choose") + "   ";
    guiMenu += selOption == OPT_UNLOCK ? "|@X| " + _("Unlock") + "  |@T| " + _("Rename") : "|@T| " + _("Rename");
    if (!internal) {
        guiMenu += "  |@S| " + _("Change memory card") + " ";
        if (gameIni.values["memcard"] == "SONY") {
            guiMenu += "|@Start| " + _("Share memory card") + "  ";
        }
    }
    guiMenu += " |@O| " + _("Back") + "|";
    gui->renderStatus(guiMenu);
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
                // one step at the press, repeats while held (holdTick, once a pass) - see startHold
                if (gui->input().dpadDown())
                    startHold(false, 1);
                else if (gui->input().dpadUp())
                    startHold(false, -1);
                else if (gui->input().dpadRight())
                    startHold(true, 1);
                else if (gui->input().dpadLeft())
                    startHold(true, -1);
                else
                    hold.release();
                break;

            case Event::Type::ButtonDown:
                // L1/R1 the first and last row, L2/R2 a page (the cursor only ever rests on an option row)
                if (e.button == Button::L1 || e.button == Button::R1 || e.button == Button::L2 ||
                    e.button == Button::R2) {
                    app.audio().cursor.play();
                    const bool down = e.button == Button::R1 || e.button == Button::R2;
                    if (e.button == Button::L1 || e.button == Button::R1)
                        selectNear(down ? static_cast<int>(rows.size()) - 1 : 0, down ? -1 : 1);
                    else
                        pageSelection(down ? 1 : -1);
                    break;
                }
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

                if (e.button == Button::Cross && selOption == OPT_UNLOCK) {
                    app.audio().cursor.play();
                    unlockSettings();
                }

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
        holdTick();
        render();
    }
}

//*******************************
// GuiEditor::startHold / holdTick / holdStep
//*******************************
// Up/Down move the cursor, Left/Right change the value: one step at the press, and held past the delay they
// go on, faster the longer they are held (HoldRepeat), a pass of the loop at a time. The repeat used to be a
// loop with no delay before the first repeat (80 ms on a value) - a tap only a little slow took two steps
void GuiEditor::startHold(bool value, int step) {
    if (hold.held() && holdOnValue == value && hold.step() == step)
        return; // the same direction still down
    holdOnValue = value;
    hold.press(step, gui->platform().ticks());
    holdStep(step);
}

void GuiEditor::holdTick() {
    if (!hold.held())
        return;
    ableem::Input &input = gui->input();
    const bool stillDown = holdOnValue ? (hold.step() > 0 ? input.dpadRight() : input.dpadLeft())
                                       : (hold.step() > 0 ? input.dpadDown() : input.dpadUp());
    if (!stillDown) {
        hold.release();
        return;
    }
    for (int steps = hold.due(gui->platform().ticks()); steps != 0; steps -= hold.step())
        holdStep(hold.step());
}

void GuiEditor::holdStep(int step) {
    app.audio().cursor.play();
    if (holdOnValue)
        processOptionChange(step > 0);
    else
        moveSelection(step);
}
