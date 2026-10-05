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
// the rows (the heading is row 0)
const int OptPadMode = 1;
const int OptDpad2Analog = 2;
const int OptAnalog2Dpad = 3;
const int OptLast = OptAnalog2Dpad;

// a flag value ("1" / "0" / "") as the row's index: 0 Automatic, 1 On, 2 Off
int flagIndex(const string &value) {
    if (value == "1")
        return 1;
    return value == "0" ? 2 : 0;
}
} // namespace

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

    flags_[0].key = AppSettings::Dpad2AnalogKey;
    flags_[0].iniKey = "dpad2analog";
    flags_[1].key = AppSettings::Analog2DpadKey;
    flags_[1].iniKey = "analog2dpad";
    for (FlagRow &flag : flags_) {
        flag.appOwn = AppSettings::normalizeFlag(manifest.value(flag.iniKey));
        flag.index = flagIndex(AppSettings::flagOverride(gameData->base, flag.key));
    }
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
void GuiAppSettings::stepPadMode(int step, bool repeat) {
    const int count = static_cast<int>(AppSettings::padModes().size()) + 1;
    const int next = abgui::stepIndex(padModeIndex_, step, count, repeat); // a press wraps, a repeat stops
    if (next == padModeIndex_)
        return;
    const string mode = next == 0 ? "" : AppSettings::padModes()[static_cast<size_t>(next) - 1];
    if (!AppSettings::setPadModeOverride(gameData->base, mode))
        return; // could not be written: the row keeps showing what is saved
    padModeIndex_ = next;
}

//*******************************
// GuiAppSettings::flagLabel / flagValue / stepFlag
//*******************************
string GuiAppSettings::flagLabel(int row) const {
    return row == OptDpad2Analog ? _("D-pad as stick:") : _("Stick as d-pad:");
}

string GuiAppSettings::flagValue(const FlagRow &flag) const {
    if (flag.index == 1)
        return _("On");
    if (flag.index == 2)
        return _("Off");
    if (flag.appOwn.empty())
        return _("Automatic");
    // Automatic with a value of the App's own: say which
    return _("Automatic") + " (" + (flag.appOwn == "1" ? _("On") : _("Off")) + ")";
}

void GuiAppSettings::stepFlag(FlagRow &flag, int step, bool repeat) {
    const int next = abgui::stepIndex(flag.index, step, 3, repeat); // a press wraps, a repeat stops
    if (next == flag.index)
        return;
    const string value = next == 1 ? "1" : (next == 2 ? "0" : "");
    if (!AppSettings::setFlagOverride(gameData->base, flag.key, value))
        return; // could not be written: the row keeps showing what is saved
    flag.index = next;
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
        ableem::DebugDriver::publish(
            typeid(*this).name(),
            {"#" + _("Game settings"), _("Pad mode:"), flagLabel(OptDpad2Analog), flagLabel(OptAnalog2Dpad)},
            selected_);

    const int right = GameDetailPane::rowsRight(*gui);
    // a theme's selection frame goes under the heading and the rows' text, so it comes first
    const bool framed = gui->text().selectionFramed(gui->uiContext());
    if (framed)
        gui->text().renderSelectionBox(gui->uiContext(), selected_, yoffset, 0, ableem::Font(), right);
    gui->text().renderLabelBox(gui->uiContext(), 0, yoffset, right);
    {
        TextRenderer::RowRoleScope role(gui->text(), TextRenderer::RowRole::Heading);
        gui->text().renderTextLine(_("Game settings"), 0, yoffset, XALIGN_LEFT);
    }
    if (!framed)
        gui->text().renderSelectionBox(gui->uiContext(), selected_, yoffset, 0, ableem::Font(), right);

    for (int row = OptPadMode; row <= OptLast; ++row) {
        TextRenderer::RowRoleScope role(gui->text(), selected_ == row ? TextRenderer::RowRole::Selected
                                                                      : TextRenderer::RowRole::Row);
        const ableem::Font &font = gui->assets().themeFont;
        const string label = row == OptPadMode ? _("Pad mode:") : flagLabel(row);
        const string value = row == OptPadMode ? padModeValue() : flagValue(flags_[row - OptDpad2Analog]);
        // the value sits at the right edge: what is left of the row after the label and a gap is its room
        const int room =
            right - gui->classicContent().x - gui->text().textWidth(font, label) - 3 * PanelStyle::RowInset;
        gui->text().renderTextLine(label, row, yoffset, XALIGN_LEFT);
        gui->text().renderRowValue(gui->text().elide(font, value, max(room, 0)), row, yoffset, right);
    }

    gui->renderStatus("|@Left+Right| " + _("Choose") + "   |@O| " + _("Back") + "|");
}

//*******************************
// GuiAppSettings::loop
//*******************************
void GuiAppSettings::loop() {
    shared_ptr<Gui> gui(Gui::getInstance());
    menuVisible = true;
    // one step of the cursor: at the press (it wraps), and again for every repeat of a held Up/Down (it stops at the
    // end)
    const auto moveCursor = [&](int dir, bool repeat = false) {
        const int to = OptPadMode + abgui::stepIndex(selected_ - OptPadMode, dir, OptLast - OptPadMode + 1, repeat);
        if (to != selected_) {
            app.audio().cursor.play();
            selected_ = to;
            render();
        }
    };
    // one step of the value on the cursor's row: at the press, and again for every repeat of a held Left/Right
    const auto changeValue = [&](int step, bool repeat = false) {
        int &value = selected_ == OptPadMode ? padModeIndex_ : flags_[selected_ - OptDpad2Analog].index;
        const int before = value;
        if (selected_ == OptPadMode)
            stepPadMode(step, repeat);
        else
            stepFlag(flags_[selected_ - OptDpad2Analog], step, repeat);
        if (value != before) // a repeat at the last value stays put, silently
            app.audio().cursor.play();
        render();
    };
    // nothing animates here: a frame after a press and four times a second meanwhile (the performance overlay, the
    // DebugDriver's shots), every pass while Up/Down or Left/Right is held (DpadHold, ValueHold)
    gui->input().setFrameNeed(ableem::Input::FrameNeed::Idle);
    while (menuVisible) {
        if (gui->input().frameDue())
            render();
        hold_.tick(gui->input(), gui->platform().ticks(), moveCursor);
        valueHold_.tick(gui->input(), gui->platform().ticks(), changeValue);
        Event e;
        int valueDir = 0;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit) {
                menuVisible = false;
            }
            switch (e.type) {
            case Event::Type::DpadDown:
            case Event::Type::DpadUp:
                hold_.track(gui->input(), gui->platform().ticks());
                valueDir = valueHold_.press(gui->input(), gui->platform().ticks());
                if (gui->input().dpadDown() || gui->input().dpadUp()) {
                    moveCursor(gui->input().dpadDown() ? 1 : -1);
                } else if (valueDir != 0) {
                    changeValue(valueDir);
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
