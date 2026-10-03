//
// GuiEditorRA: the game editor for a RetroArch game - the light-gun flag and the core that plays it.
//
#include "gui/menus/gui_game_editor_ra_menu.h"
#include "gui/gui.h"
#include "../../app.h"
#include "core/services/environment.h"

#include <ableem/ui/debug_driver.h>

#include <algorithm>
#include <typeinfo>

using namespace std;

#define OPT_LIGHTGUN 1
#define OPT_CORE 2

//*******************************
// GuiEditorRA::init
//*******************************
void GuiEditorRA::init() {
    string path = app.thumbnails().findBoxArt(gameData->db_name, gameData->title);
    if (path.empty())
        path = Env::getWorkingPath() + sep + "evoimg/ra-cover.png";
    cover = ableem::Texture::loadFile(renderer, path);

    // the game's own core is one of the list (the default is the first); an entry whose core is not among
    // them - a core the cfg names that no .info lists for the system - shows the first
    cores_ = app.retroArch().coresForGame(*gameData);
    coreIndex = 0;
    for (size_t i = 0; i < cores_.size(); i++) {
        if (cores_[i]->core_path == gameData->core_path) {
            coreIndex = static_cast<int>(i);
            break;
        }
    }
    selOption = OPT_LIGHTGUN;
}

//*******************************
// GuiEditorRA::coreValue
//*******************************
string GuiEditorRA::coreValue() const {
    if (cores_.empty())
        return gameData->core_name;
    string value = cores_[coreIndex]->shortName();
    if (coreIndex == 0)
        value += " " + _("(default)");
    return value;
}

//*******************************
// GuiEditorRA::cycleCore
//*******************************
void GuiEditorRA::cycleCore(int step) {
    const int count = static_cast<int>(cores_.size());
    if (count < 2)
        return;
    const int next = (coreIndex + step + count) % count;
    if (!app.retroArch().setGameCore(*gameData, cores_[next]))
        return; // the entry could not be written: the row keeps showing what the game uses
    coreIndex = next;
}

//*******************************
// GuiEditorRA::draw
//*******************************
void GuiEditorRA::draw() {
    shared_ptr<Gui> gui(Gui::getInstance());
    gui->renderBackground();
    gui->renderTextBar();
    int yoffset = gui->renderHeader(gui->text().elide(gui->assets().themeFonts[FONT_28_BOLD], gameData->title,
                                                      gui->classicPanel().w - 2 * PanelStyle::RowInset));

    pane.cover = cover;
    pane.facts.clear();
    pane.facts.emplace_back(_("File:"), DirEntry::getFileNameFromPath(gameData->image_path));
    if (!gameData->publisher.empty())
        pane.facts.emplace_back(_("Published by:"), gameData->publisher);
    if (gameData->year > 0)
        pane.facts.emplace_back(_("Year:"), to_string(gameData->year));
    pane.render(*gui);
    if (menuVisible) // the DebugDriver's rows: the heading band, then the two options, the cursor's index among them
        ableem::DebugDriver::publish(typeid(*this).name(), {"#" + _("Game"), _("Lightgun game:"), _("Core:")},
                                     selOption);

    const int right = GameDetailPane::rowsRight(*gui);
    // a theme's selection frame goes under the heading and the rows' text, so it comes first (G4d)
    const bool framed = gui->text().selectionFramed(gui->uiContext());
    if (framed)
        gui->text().renderSelectionBox(gui->uiContext(), selOption, yoffset, 0, ableem::Font(), right);
    gui->text().renderLabelBox(gui->uiContext(), 0, yoffset, right);
    {
        TextRenderer::RowRoleScope role(gui->text(), TextRenderer::RowRole::Heading);
        gui->text().renderTextLine(_("Game"), 0, yoffset, XALIGN_LEFT);
    }
    if (!framed)
        gui->text().renderSelectionBox(gui->uiContext(), selOption, yoffset, 0, ableem::Font(), right);

    {
        TextRenderer::RowRoleScope role(gui->text(), selOption == OPT_LIGHTGUN ? TextRenderer::RowRole::Selected
                                                                               : TextRenderer::RowRole::Row);
        gui->text().renderTextLineOptions(
            _("Lightgun game:") + (app.lightguns().isLightgun(*gameData) ? string("|@Check|") : string("|@Uncheck|")),
            OPT_LIGHTGUN, yoffset, XALIGN_LEFT, 0, right);
    }
    {
        TextRenderer::RowRoleScope role(gui->text(), selOption == OPT_CORE ? TextRenderer::RowRole::Selected
                                                                           : TextRenderer::RowRole::Row);
        const ableem::Font &font = gui->assets().themeFont;
        const string label = _("Core:");
        // the value sits at the right edge: what is left of the row after the label and a gap is its room
        const int room =
            right - gui->classicContent().x - gui->text().textWidth(font, label) - 3 * PanelStyle::RowInset;
        gui->text().renderTextLine(label, OPT_CORE, yoffset, XALIGN_LEFT);
        gui->text().renderRowValue(gui->text().elide(font, coreValue(), max(room, 0)), OPT_CORE, yoffset, right);
    }

    gui->renderStatus("|@Left+Right| " + _("Choose") + "   |@O| " + _("Back") + "|");
}

//*******************************
// GuiEditorRA::loop
//*******************************
void GuiEditorRA::loop() {
    shared_ptr<Gui> gui(Gui::getInstance());
    menuVisible = true;
    while (menuVisible) {
        // nothing animates here: sleep until a press, and redraw 4 times a second meanwhile (the performance
        // overlay, the DebugDriver's shots)
        if (!gui->input().waitForEvent(250))
            render();
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit) {
                menuVisible = false;
            }
            switch (e.type) {
            case Event::Type::DpadDown:
            case Event::Type::DpadUp:
                if (gui->input().dpadDown() || gui->input().dpadUp()) {
                    const int to = gui->input().dpadDown() ? OPT_CORE : OPT_LIGHTGUN;
                    if (to != selOption) {
                        app.audio().cursor.play();
                        selOption = to;
                        render();
                    }
                } else if (gui->input().dpadRight() || gui->input().dpadLeft()) {
                    app.audio().cursor.play();
                    const bool right = gui->input().dpadRight();
                    if (selOption == OPT_LIGHTGUN) {
                        if (right != app.lightguns().isLightgun(*gameData)) {
                            app.lightguns().setRetroArchLightgun(*gameData, right);
                            changed = true;
                        }
                    } else {
                        cycleCore(right ? 1 : -1);
                    }
                    render();
                }
                break;
            case Event::Type::ButtonDown:
                if (e.button == Button::Circle) {
                    app.audio().cancel.play();
                    menuVisible = false;
                }
                break;
            default:
                break;
            }
        }
    }
}
