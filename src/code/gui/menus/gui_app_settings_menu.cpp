//
// GuiAppSettings: the "Game settings" screen of an App - see the header.
//
#include "gui/menus/gui_app_settings_menu.h"
#include "gui/gui.h"
#include "../../app.h"
#include "core/services/app_manifest.h"
#include "core/services/app_settings.h"
#include "core/services/environment.h"

#include <ableem/ui/debug_driver.h>

#include <algorithm>
#include <typeinfo>

using namespace std;

namespace {
const int OptPadMode = 1; // the row the cursor is on (the heading is row 0)
}

//*******************************
// GuiAppSettings::init
//*******************************
void GuiAppSettings::init() {
    string path = gameData->image_path;
    if (path.empty() || !DirEntry::exists(path))
        path = Env::getWorkingPath() + sep + "evoimg/app-cover.png";
    cover = ableem::Texture::loadFile(renderer, path);

    const AppManifest manifest = AppManifest::load(gameData->base, "app.ini", Env::appPlatformKeys());
    appPadMode_ = AppSettings::normalizePadMode(manifest.value("padmode"));
    const string chosen = AppSettings::padModeOverride(gameData->base);
    const vector<string> &modes = AppSettings::padModes();
    const auto found = find(modes.begin(), modes.end(), chosen);
    padModeIndex_ = chosen.empty() || found == modes.end() ? 0 : static_cast<int>(found - modes.begin()) + 1;
}

//*******************************
// GuiAppSettings::padModeName / padModeValue
//*******************************
string GuiAppSettings::padModeName(int index) const {
    if (index == 0)
        return _("Automatic");
    const string &mode = AppSettings::padModes()[static_cast<size_t>(index) - 1];
    if (mode == "psc")
        return _("PSC pad");
    if (mode == "x360")
        return _("Xbox 360 pad");
    if (mode == "psc-kernel")
        return _("PSC pad (system)");
    return _("Xbox 360 pad (system)");
}

string GuiAppSettings::padModeValue() const {
    if (padModeIndex_ != 0 || appPadMode_.empty())
        return padModeName(padModeIndex_);
    // Automatic with a mode of the App's own: say which
    const vector<string> &modes = AppSettings::padModes();
    const int own = static_cast<int>(find(modes.begin(), modes.end(), appPadMode_) - modes.begin()) + 1;
    return padModeName(0) + " (" + padModeName(own) + ")";
}

//*******************************
// GuiAppSettings::stepPadMode
//*******************************
void GuiAppSettings::stepPadMode(int step) {
    const int count = static_cast<int>(AppSettings::padModes().size()) + 1;
    const int next = (padModeIndex_ + step + count) % count;
    const string mode = next == 0 ? "" : AppSettings::padModes()[static_cast<size_t>(next) - 1];
    if (!AppSettings::setPadModeOverride(gameData->base, mode))
        return; // could not be written: the row keeps showing what is saved
    padModeIndex_ = next;
}

//*******************************
// GuiAppSettings::draw
//*******************************
void GuiAppSettings::draw() {
    shared_ptr<Gui> gui(Gui::getInstance());
    gui->renderBackground();
    gui->renderTextBar();
    int yoffset = gui->renderHeader(gui->text().elide(gui->assets().themeFonts[FONT_28_BOLD], gameData->title,
                                                      gui->classicPanel().w - 2 * PanelStyle::RowInset));

    pane.cover = cover;
    pane.facts.clear();
    if (!gameData->publisher.empty())
        pane.facts.emplace_back(_("Published by:"), gameData->publisher);
    pane.render(*gui);
    if (menuVisible) // the DebugDriver's rows: the heading band, then the options, the cursor's index among them
        ableem::DebugDriver::publish(typeid(*this).name(), {"#" + _("Game settings"), _("Pad mode:")}, OptPadMode);

    const int right = GameDetailPane::rowsRight(*gui);
    // a theme's selection frame goes under the heading and the rows' text, so it comes first
    const bool framed = gui->text().selectionFramed(gui->uiContext());
    if (framed)
        gui->text().renderSelectionBox(gui->uiContext(), OptPadMode, yoffset, 0, ableem::Font(), right);
    gui->text().renderLabelBox(gui->uiContext(), 0, yoffset, right);
    {
        TextRenderer::RowRoleScope role(gui->text(), TextRenderer::RowRole::Heading);
        gui->text().renderTextLine(_("Game settings"), 0, yoffset, XALIGN_LEFT);
    }
    if (!framed)
        gui->text().renderSelectionBox(gui->uiContext(), OptPadMode, yoffset, 0, ableem::Font(), right);

    {
        TextRenderer::RowRoleScope role(gui->text(), TextRenderer::RowRole::Selected);
        const ableem::Font &font = gui->assets().themeFont;
        const string label = _("Pad mode:");
        const int room =
            right - gui->classicContent().x - gui->text().textWidth(font, label) - 3 * PanelStyle::RowInset;
        gui->text().renderTextLine(label, OptPadMode, yoffset, XALIGN_LEFT);
        gui->text().renderRowValue(gui->text().elide(font, padModeValue(), max(room, 0)), OptPadMode, yoffset, right);
    }

    gui->renderStatus("|@Left+Right| " + _("Choose") + "   |@O| " + _("Back") + "|");
}

//*******************************
// GuiAppSettings::loop
//*******************************
void GuiAppSettings::loop() {
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
                if (gui->input().dpadRight() || gui->input().dpadLeft()) {
                    app.audio().cursor.play();
                    stepPadMode(gui->input().dpadRight() ? 1 : -1);
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
