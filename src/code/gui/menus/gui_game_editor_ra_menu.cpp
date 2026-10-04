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
#define OPT_ASPECT 3
#define OPT_INTEGER 4
#define OPT_SMOOTHING 5
#define OPT_SCANLINES 6
#define OPT_FPS 7
#define OPT_ANALOG 8
#define OPT_RESUME 9
#define OPT_LAST OPT_RESUME

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
    options_ = app.raOptions().get(*gameData);
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
// GuiEditorRA::lastOption
//*******************************
// the last row shown: the Resume row only for a core that can save states (one that cannot has no slots to resume)
int GuiEditorRA::lastOption() const {
    return app.resumePoints().raSupportsStates(*gameData) ? OPT_RESUME : OPT_RESUME - 1;
}

//*******************************
// GuiEditorRA::optionLabel / optionValue / stepOption
//*******************************
string GuiEditorRA::optionLabel(int option) const {
    switch (option) {
    case OPT_ASPECT:
        return _("Aspect ratio:");
    case OPT_INTEGER:
        return _("Integer scaling:");
    case OPT_SMOOTHING:
        return _("Smoothing:");
    case OPT_SCANLINES:
        return _("Scanlines:");
    case OPT_FPS:
        return _("Show FPS:");
    case OPT_ANALOG:
        return _("Analog stick as D-pad:");
    default:
        return _("Resume:");
    }
}

string GuiEditorRA::optionValue(int option) const {
    auto tri = [](int v) {
        return v == RaGameOptions::TriDefault ? _("Default") : v == RaGameOptions::TriOn ? _("ON") : _("OFF");
    };
    switch (option) {
    case OPT_ASPECT:
        switch (options_.aspect) {
        case RaGameOptions::AspectCore:
            return _("From core");
        case RaGameOptions::Aspect43:
            return _("4:3");
        case RaGameOptions::AspectFull:
            return _("Full screen");
        case RaGameOptions::AspectPixel:
            return _("Pixel 1:1");
        default:
            return _("Default");
        }
    case OPT_INTEGER:
        return tri(options_.integerScaling);
    case OPT_SMOOTHING:
        return tri(options_.smoothing);
    case OPT_SCANLINES:
        switch (options_.scanlines) {
        case RaGameOptions::ScanOff:
            return _("Off");
        case RaGameOptions::ScanLight:
            return _("Light");
        case RaGameOptions::ScanStrong:
            return _("Strong");
        default:
            return _("Default");
        }
    case OPT_FPS:
        return tri(options_.showFps);
    case OPT_ANALOG:
        return tri(options_.analogAsDpad);
    default:
        switch (options_.resume) {
        case RaGameOptions::ResumeLast:
            return _("Last slot");
        case RaGameOptions::ResumeNever:
            return _("Never");
        default:
            return _("Ask");
        }
    }
}

void GuiEditorRA::stepOption(int option, int step) {
    auto next = [step](int value, int count) { return (value + step + count) % count; };
    switch (option) {
    case OPT_ASPECT:
        options_.aspect = next(options_.aspect, RaGameOptions::AspectCount);
        break;
    case OPT_INTEGER:
        options_.integerScaling = next(options_.integerScaling, RaGameOptions::TriCount);
        break;
    case OPT_SMOOTHING:
        options_.smoothing = next(options_.smoothing, RaGameOptions::TriCount);
        break;
    case OPT_SCANLINES:
        options_.scanlines = next(options_.scanlines, RaGameOptions::ScanCount);
        break;
    case OPT_FPS:
        options_.showFps = next(options_.showFps, RaGameOptions::TriCount);
        break;
    case OPT_ANALOG:
        options_.analogAsDpad = next(options_.analogAsDpad, RaGameOptions::TriCount);
        break;
    default:
        options_.resume = next(options_.resume, RaGameOptions::ResumeCount);
        break;
    }
    app.raOptions().set(*gameData, options_);
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
    {
        std::vector<std::string> rows = {"#" + _("Game"), _("Lightgun game:"), _("Core:")};
        for (int option = OPT_ASPECT; option <= lastOption(); option++)
            rows.push_back(optionLabel(option));
        if (menuVisible) // the DebugDriver's rows: the heading band, then the options, the cursor's index among them
            ableem::DebugDriver::publish(typeid(*this).name(), rows, selOption);
    }

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

    for (int option = OPT_ASPECT; option <= lastOption(); option++) {
        TextRenderer::RowRoleScope role(gui->text(), selOption == option ? TextRenderer::RowRole::Selected
                                                                         : TextRenderer::RowRole::Row);
        const ableem::Font &font = gui->assets().themeFont;
        const string label = optionLabel(option);
        const int room =
            right - gui->classicContent().x - gui->text().textWidth(font, label) - 3 * PanelStyle::RowInset;
        gui->text().renderTextLine(label, option, yoffset, XALIGN_LEFT);
        gui->text().renderRowValue(gui->text().elide(font, optionValue(option), max(room, 0)), option, yoffset, right);
    }

    gui->renderStatus("|@Left+Right| " + _("Choose") + "   |@O| " + _("Back") + "|");
}

//*******************************
// GuiEditorRA::loop
//*******************************
void GuiEditorRA::loop() {
    shared_ptr<Gui> gui(Gui::getInstance());
    menuVisible = true;
    // one step of the cursor: at the press, and again for every repeat of a held Up/Down
    const auto moveCursor = [&](int dir) {
        const int to = std::min(lastOption(), std::max(OPT_LIGHTGUN, selOption + dir));
        if (to != selOption) {
            app.audio().cursor.play();
            selOption = to;
            render();
        }
    };
    // nothing animates here: a frame after a press and four times a second meanwhile (the performance overlay, the
    // DebugDriver's shots), every pass while Up/Down is held (DpadHold)
    gui->input().setFrameNeed(ableem::Input::FrameNeed::Idle);
    while (menuVisible) {
        if (gui->input().frameDue())
            render();
        hold_.tick(gui->input(), gui->platform().ticks(), moveCursor);
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit) {
                menuVisible = false;
            }
            switch (e.type) {
            case Event::Type::DpadDown:
            case Event::Type::DpadUp:
                hold_.track(gui->input(), gui->platform().ticks());
                if (gui->input().dpadDown() || gui->input().dpadUp()) {
                    moveCursor(gui->input().dpadDown() ? 1 : -1);
                } else if (gui->input().dpadRight() || gui->input().dpadLeft()) {
                    app.audio().cursor.play();
                    const bool right = gui->input().dpadRight();
                    if (selOption == OPT_LIGHTGUN) {
                        if (right != app.lightguns().isLightgun(*gameData)) {
                            app.lightguns().setRetroArchLightgun(*gameData, right);
                            changed = true;
                        }
                    } else if (selOption == OPT_CORE) {
                        cycleCore(right ? 1 : -1);
                    } else {
                        stepOption(selOption, right ? 1 : -1);
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
