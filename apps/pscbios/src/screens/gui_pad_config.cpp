//
// GuiPadConfig: the pad test and mapping wizard.
//
#include "gui_pad_config.h"
#include "core/game_controller_db.h"
#include "gui/gui.h"
#include "gui/screens/gui_keyboard.h"

#include <ableem/engine/log.h>

#include <algorithm>
#include <cstdlib>

using namespace std;
using ableem::Color;
using ableem::ControllerState;
using ableem::Joystick;
using ableem::Rect;

//*******************************
// GuiPadConfig::init
//*******************************
void GuiPadConfig::init() {
    padImage = ableem::Texture::loadFile(renderer, Env::getAppDir() + sep + "DS3.png");
    elements = PadMapping::standardElements();
    chooseFont();
    if (Joystick::count() == 0) {
        gui->drawText(_("NO GAMEPADS CONNECTED"));
        gui->platform().delay(1000);
    } else {
        openJoystick(0);
    }
}

//*******************************
// GuiPadConfig::chooseFont
//*******************************
// The left column is the facts (4 rows), the message (3 rows wrapped) and, while mapping, the 26 entries in
// two columns of 13 - 20 rows. The classic font at the theme's size (18 at most - a smaller one reads
// better next to the numbers) unless that runs past the footer, then the largest size down to 12 at
// which every row fits the panel's content.
void GuiPadConfig::chooseFont() {
    const ableem::Rect content = gui->classicContent();
    const int themeSize = min(18, static_cast<int>(app.theme().classic().font.size.value));
    pageFont = gui->assets().classicFontAtSize(themeSize);
    for (int size = themeSize; size >= 12; size--) {
        pageFont = gui->assets().classicFontAtSize(size);
        if (PageRows * pageFont.lineHeight() <= content.h - 8)
            break;
    }
}

//*******************************
// GuiPadConfig::openJoystick / nextJoystick / joystickTitle
//*******************************
void GuiPadConfig::openJoystick(int index) {
    joystick.close();
    if (index >= 0 && index < Joystick::count())
        joystick.open(index);
}

void GuiPadConfig::nextJoystick() {
    int count = Joystick::count();
    if (count == 0) {
        joystick.close();
        return;
    }
    openJoystick((joystick.index() + 1) % count);
}

string GuiPadConfig::joystickTitle() const {
    if (!joystick.isOpen())
        return _("NO GAME CONTROLLERS OPENED");
    string name = joystick.name();
    if (joystick.isGameController())
        name += " - SDL2:" + Joystick::controllerNameForIndex(joystick.index());
    return "#" + to_string(joystick.index() + 1) + "/" + to_string(Joystick::count()) + "/ " + name;
}

//*******************************
// GuiPadConfig::startMapping / cancelMapping
//*******************************
void GuiPadConfig::startMapping() {
    joysticksAtStart = Joystick::count();
    originalMapping = joystick.isGameController() ? gui->input().mappingForDeviceIndex(joystick.index()) : "";
    for (PadMapping::Element &element : elements)
        element.value.clear();
    // reopened so that the raw view starts from a clean state, then a snapshot of the pad at rest
    openJoystick(joystick.index());
    joystick.update();
    initialState = joystick.state();
    gui->input().flushEvents();
    stage = Stage::Mapping;
    current = 0;
}

void GuiPadConfig::cancelMapping() {
    for (PadMapping::Element &element : elements)
        element.value.clear();
    if (!originalMapping.empty())
        gui->input().addMapping(originalMapping);
    gui->input().flushEvents();
    stage = Stage::Test;
}

//*******************************
// GuiPadConfig::waitUntilReleased / takeInput
//*******************************
// the input just taken is still held; the next element must not see it
void GuiPadConfig::waitUntilReleased() {
    do {
        joystick.update();
    } while (PadMapping::anythingHeld(initialState, joystick.state()));
}

void GuiPadConfig::takeInput(const string &value) {
    app.audio().cursor.play();
    elements[current].value = value;
    waitUntilReleased();
    advance();
}

void GuiPadConfig::advance() {
    if (current + 1 < elements.size()) {
        current++;
    } else {
        finishMapping();
    }
}

//*******************************
// GuiPadConfig::finishMapping
//*******************************
// every element asked for: the list is merged, given to SDL for the test stage, and the pad reopened so
// that its controller view uses it
void GuiPadConfig::finishMapping() {
    finals = PadMapping::finalElements(elements, ableem::Platform::osName());
    string line = PadMapping::mappingLine(joystick.guid(), PadMapping::cleanName(joystick.name()), finals);
    PLOG_INFO << "New mapping: " << line;
    if (gui->input().addMapping(line))
        openJoystick(joystick.index());
    gui->input().flushEvents();
    stage = Stage::Save;
}

//*******************************
// GuiPadConfig::saveMapping
//*******************************
void GuiPadConfig::saveMapping() {
    GuiKeyboard keyboard(*gui);
    keyboard.result = PadMapping::cleanName(joystick.name());
    keyboard.label = _("Enter name for new gamepad");
    keyboard.show();
    string name = PadMapping::cleanName(keyboard.result);
    if (keyboard.cancelled || name.empty()) {
        cancelMapping();
        return;
    }
    string line = PadMapping::mappingLine(joystick.guid(), name, finals);
    if (gui->input().addMapping(line)) {
        // the file the launcher loads from: the one probePads() found, else the shipped one
        string path = gui->input().currentMappingPath();
        if (path.empty())
            path = Env::getPathToGameControllerDb();
        GameControllerDb db;
        db.load(path);
        db.replaceMapping(joystick.guid(), ableem::Platform::osName(), line);
        if (db.save(path)) {
            PLOG_INFO << "Mapping stored in " << path;
            gui->drawText(_("Mapping stored to database"));
        } else {
            gui->drawText(_("Error Storing mapping to database"));
        }
    } else {
        gui->drawText(_("Error Storing mapping to database"));
    }
    gui->platform().delay(2000);
    stage = Stage::Test;
}

//*******************************
// GuiPadConfig::pictureRect
//*******************************
// the DualShock picture (drawn 420x425 in the 2020 tool) at the right of the panel's content, scaled down
// when the content is not that tall; the overlays below are in the picture's own 420x425 coordinates
ableem::Rect GuiPadConfig::pictureRect() const {
    const ableem::Rect content = gui->classicContent();
    int h = min(425, content.h - 8);
    int w = 420 * h / 425;
    return Rect(content.x + content.w - PanelStyle::RowInset - 8 - w, content.y + (content.h - h) / 2, w, h);
}

//*******************************
// GuiPadConfig::renderPadPicture
//*******************************
// the DualShock picture with the pressed buttons and the sticks' positions drawn over it; while mapping,
// the element being asked for is what lights up instead
void GuiPadConfig::renderPadPicture(const ControllerState &liveState) {
    const Rect picture = pictureRect();
    if (padImage.valid())
        renderer.copy(padImage, nullptr, &picture);
    // the overlays were placed on the picture drawn at (430, 200) 420x425
    const float k = static_cast<float>(picture.h) / 425.0f;
    auto at = [&](int x, int y, int w, int h) {
        return Rect(picture.x + static_cast<int>((x - 430) * k), picture.y + static_cast<int>((y - 200) * k),
                    max(1, static_cast<int>(w * k)), max(1, static_cast<int>(h * k)));
    };

    ControllerState state = liveState;
    bool showSticks = true;
    if (stage == Stage::Mapping) {
        renderer.setDrawColor(Color(255, 0, 0, 100));
        state = ControllerState();
        const PadMapping::Element &element = elements[current];
        if (element.button >= 0)
            state.buttons[element.button] = true;
        if (element.axis >= 0)
            state.axes[element.axis] = element.negative ? -32767 : 32767;
        showSticks = element.scan == PadMapping::Scan::Analog;
    } else {
        renderer.setDrawColor(Color(0, 255, 0, 150));
    }
    renderer.setBlendMode(ableem::BlendMode::Blend);

    // where each of SDL's 15 standard buttons is on the picture
    static const int positions[15][4] = {{745, 481, 30, 30}, {777, 449, 30, 30}, {713, 449, 30, 30}, {745, 416, 30, 30},
                                         {586, 454, 32, 21}, {624, 472, 32, 32}, {660, 454, 32, 21}, {548, 492, 61, 61},
                                         {670, 492, 61, 61}, {498, 298, 51, 27}, {730, 298, 51, 27}, {508, 432, 22, 26},
                                         {508, 471, 22, 26}, {486, 452, 26, 23}, {526, 452, 26, 23}};
    for (int i = 0; i < 15; i++)
        if (state.buttons[i])
            renderer.fillRect(at(positions[i][0], positions[i][1], positions[i][2], positions[i][3]));

    if (showSticks) {
        renderer.fillRect(at(571 + state.axes[0] * 20 / 32768, 515 + state.axes[1] * 20 / 32768, 16, 16));
        renderer.fillRect(at(693 + state.axes[2] * 20 / 32768, 515 + state.axes[3] * 20 / 32768, 16, 16));
    }
    // the triggers fill up from the bottom
    int left = state.axes[4] * 47 / 32768;
    renderer.fillRect(at(498, 237 + 46 - left, 51, 1 + left));
    int right = state.axes[5] * 47 / 32768;
    renderer.fillRect(at(730, 237 + 46 - right, 51, 1 + right));
}

//*******************************
// GuiPadConfig::renderElements
//*******************************
// the mapping entries as "name  value" rows in two columns from y; returns the y below them
int GuiPadConfig::renderElements(int x, int y, int width) {
    const vector<PadMapping::Element> &list = stage == Stage::Mapping ? elements : finals;
    const ableem::Color secondary = gui->panelStyle().secondary;
    const ableem::Color text = gui->panelStyle().text;
    const int perColumn = (static_cast<int>(list.size()) + 1) / 2;
    const int columnWidth = (width - 16) / 2;
    const int top = y;
    for (size_t i = 0; i < list.size(); i++) {
        const PadMapping::Element &element = list[i];
        const int column = static_cast<int>(i) / perColumn;
        const int cx = x + column * (columnWidth + 16);
        const int cy = top + (static_cast<int>(i) % perColumn) * pageFont.lineHeight();
        const bool asking = stage == Stage::Mapping && i == current;
        if (asking)
            gui->panelStyle().selection(renderer, Rect(cx - 8, cy, columnWidth + 8, pageFont.lineHeight()));
        gui->text().renderText_WithColor(pageFont, element.apiName, cx, cy, asking ? text : secondary, XALIGN_LEFT);
        gui->text().renderText_WithColor(pageFont, element.value, cx + columnWidth * 6 / 10, cy, text, XALIGN_LEFT);
    }
    return top + perColumn * pageFont.lineHeight();
}

//*******************************
// GuiPadConfig::render
//*******************************
void GuiPadConfig::render() {
    if (stage != Stage::Test && Joystick::count() != joysticksAtStart) {
        cancelMapping();
        gui->drawText(_("Gamepad configuration changed. Mapping interrupted."));
        gui->platform().delay(2000);
    }
    joystick.update();

    renderer.clear();
    gui->renderBackground();
    gui->renderTextBar();
    gui->renderHeader(_("Gamepad configuration details"));
    TextRenderer &text = gui->text();
    const PanelStyle style = gui->panelStyle();
    const ableem::Rect content = gui->classicContent();
    const Rect picture = pictureRect();
    const int x = content.x + PanelStyle::RowInset + 8;
    const int width = picture.x - 24 - x;
    const int lineHeight = pageFont.lineHeight();
    int y = content.y + 4;
    auto row = [&](const string &line, const ableem::Color &color) {
        text.renderText_WithColor(pageFont, text.elide(pageFont, line, width), x, y, color, XALIGN_LEFT);
        y += lineHeight;
    };

    if (stage == Stage::Mapping) {
        string moved = PadMapping::detectChange(initialState, joystick.state(), elements);
        if (!moved.empty())
            takeInput(moved);
    }

    // the facts: the pad, its inputs, the raw buttons and hats, the axes eight to a row
    const ableem::JoystickState &state = joystick.state();
    row(joystickTitle(), style.text);
    row(_("Gamepad input configuration:") + " A:" + to_string(state.axes.size()) +
            "  B:" + to_string(state.buttons.size()) + " D:" + to_string(state.hats.size()),
        style.secondary);
    string buttons = _("Buttons:") + " ";
    for (bool pressed : state.buttons)
        buttons += string(pressed ? "1" : "0") + " ";
    buttons += " " + _("Hats:") + " ";
    for (unsigned hat : state.hats)
        buttons += to_string(hat) + " ";
    row(buttons, style.secondary);
    string axes;
    for (size_t i = 0; i < state.axes.size(); i++) {
        int percent = state.axes[i] * 100 / 32767;
        string padded = to_string(abs(percent));
        padded.insert(0, 3 - min<size_t>(3, padded.size()), '0');
        axes += string(i < 9 ? " " : "") + "#" + to_string(i + 1) + ":" + (percent < 0 ? "-" : " ") + padded + " ";
        if ((i + 1) % 8 == 0 || i + 1 == state.axes.size()) {
            row(axes, style.secondary);
            axes.clear();
        }
    }

    // the message for the stage, wrapped to the column
    y += lineHeight / 2;
    string first, second;
    switch (stage) {
    case Stage::Test:
        first = _("NOTE: Make sure none of the buttons are pressed before mapping and all analog sticks are in default "
                  "position.");
        second = _("You can test your controller");
        break;
    case Stage::Mapping:
        switch (elements[current].scan) {
        case PadMapping::Scan::Digital:
            first = _("Press a button highlighted or (OPEN) if not avaliable.");
            break;
        case PadMapping::Scan::Trigger:
            first = _("Press a trigger highlighted fully or (OPEN) if not avaliable.");
            break;
        case PadMapping::Scan::Analog:
            first = _("Move your sticks to state shown or press (OPEN) if stick position not avaliable.");
            break;
        }
        second = _("Updating mapping");
        break;
    case Stage::Save:
        first = _("Mapping Complete - press (OPEN) to save. (POWER) to cancel.");
        second = _("Please test a new mapping");
        break;
    }
    y += max(lineHeight, text.renderWrappedText(pageFont, first, x, y, width, style.text));
    y += max(lineHeight, text.renderWrappedText(pageFont, second, x, y, width, style.secondary));
    y += lineHeight / 2;

    if (stage != Stage::Test)
        renderElements(x, y, width);

    renderPadPicture(joystick.controllerState());

    // the console's front buttons, as chips
    if (stage == Stage::Test)
        gui->renderStatus("|@Reset| " + _("Next pad") + "   |@Open| " + _("Update mapping") + "   |@Power| " +
                          _("Exit"));
    else if (stage == Stage::Mapping)
        gui->renderStatus("|@Open| " + _("No button on controller") + "   |@Power| " + _("Cancel mapping"));
    else
        gui->renderStatus("|@Open| " + _("Save") + "   |@Power| " + _("Cancel mapping"));
    renderer.present();
}

//*******************************
// GuiPadConfig::loop
//*******************************
// the console's front buttons drive this screen (the pad under test is not to be trusted): Power, Reset
// and Open; on a keyboard Escape, Start (Space) and Return do the same. Input hands the power button over
// as a key for the duration instead of powering off.
void GuiPadConfig::loop() {
    menuVisible = true;
    gui->input().setPowerKeyAsKey(true);
    while (menuVisible) {
        render();
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit)
                menuVisible = false;
            bool power = e.type == Event::Type::KeyDown && (e.key == Key::Sleep || e.key == Key::Escape);
            bool reset = (e.type == Event::Type::KeyDown && e.key == Key::Reset) ||
                         (e.type == Event::Type::ButtonDown && e.button == Button::Start);
            bool open = e.type == Event::Type::KeyDown && (e.key == Key::Open || e.key == Key::Return);
            if (power) {
                app.audio().cursor.play();
                if (stage == Stage::Test)
                    menuVisible = false;
                else
                    cancelMapping();
            } else if (reset) {
                if (stage == Stage::Test && Joystick::count() > 0) {
                    app.audio().cursor.play();
                    nextJoystick();
                }
            } else if (open) {
                switch (stage) {
                case Stage::Test:
                    if (Joystick::count() > 0) {
                        app.audio().cursor.play();
                        startMapping();
                    }
                    break;
                case Stage::Mapping:
                    app.audio().cursor.play();
                    elements[current].value.clear(); // no such input on this pad
                    advance();
                    break;
                case Stage::Save:
                    app.audio().cursor.play();
                    saveMapping();
                    break;
                }
            } else if (e.type == Event::Type::PadAdded || e.type == Event::Type::PadRemoved) {
                if (stage == Stage::Test) {
                    gui->drawText(_("Gamepad configuration changed."));
                    gui->platform().delay(2000);
                    if (!joystick.isOpen() || joystick.index() >= Joystick::count())
                        openJoystick(0);
                }
            }
        }
    }
    gui->input().setPowerKeyAsKey(false);
    joystick.close();
}
