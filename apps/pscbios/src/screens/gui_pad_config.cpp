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
    if (Joystick::count() == 0) {
        gui->drawText(_("NO GAMEPADS CONNECTED"));
        gui->platform().delay(1000);
    } else {
        openJoystick(0);
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
// GuiPadConfig::renderPadPicture
//*******************************
// the DualShock picture at (430, 200) 420x425, with the pressed buttons and the sticks' positions drawn
// over it; while mapping, the element being asked for is what lights up instead
void GuiPadConfig::renderPadPicture(const ControllerState &liveState) {
    Rect picture(430, 200, 420, 425);
    if (padImage.valid())
        renderer.copy(padImage, nullptr, &picture);

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
            renderer.fillRect(Rect(positions[i][0], positions[i][1], positions[i][2], positions[i][3]));

    if (showSticks) {
        renderer.fillRect(Rect(571 + state.axes[0] * 20 / 32768, 515 + state.axes[1] * 20 / 32768, 16, 16));
        renderer.fillRect(Rect(693 + state.axes[2] * 20 / 32768, 515 + state.axes[3] * 20 / 32768, 16, 16));
    }
    // the triggers fill up from the bottom
    int left = state.axes[4] * 47 / 32768;
    renderer.fillRect(Rect(498, 237 + 46 - left, 51, 1 + left));
    int right = state.axes[5] * 47 / 32768;
    renderer.fillRect(Rect(730, 237 + 46 - right, 51, 1 + right));
}

//*******************************
// GuiPadConfig::renderElements
//*******************************
void GuiPadConfig::renderElements() {
    const vector<PadMapping::Element> &list = stage == Stage::Mapping ? elements : finals;
    int row = 8;
    for (const PadMapping::Element &element : list)
        gui->text().renderTextLine(element.apiName + ":" + element.value, row++, 10);
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
    const int offset = 10;
    TextRenderer &text = gui->text();
    text.renderTextLine(_("Gamepad configuration details"), 0, offset, XALIGN_CENTER);

    if (stage == Stage::Mapping) {
        string moved = PadMapping::detectChange(initialState, joystick.state(), elements);
        if (!moved.empty())
            takeInput(moved);
    }

    const ableem::JoystickState &state = joystick.state();
    text.renderTextLine(joystickTitle(), 1, offset);
    text.renderTextLine(_("Gamepad input configuration: ") + "A:" + to_string(state.axes.size()) +
                            "  B:" + to_string(state.buttons.size()) + " D:" + to_string(state.hats.size()),
                        2, offset);
    string buttons = _("Buttons:") + " ";
    for (bool pressed : state.buttons)
        buttons += string(pressed ? "1" : "0") + " ";
    buttons += " " + _("Hats:") + " ";
    for (unsigned hat : state.hats)
        buttons += to_string(hat) + " ";
    text.renderTextLine(buttons, 3, offset);
    // the axes as a signed percentage each, fifteen to a row
    string axes;
    int row = 4;
    for (size_t i = 0; i < state.axes.size(); i++) {
        int percent = state.axes[i] * 100 / 32767;
        string padded = to_string(abs(percent));
        padded.insert(0, 3 - min<size_t>(3, padded.size()), '0');
        axes += string(i < 9 ? " " : "") + "#" + to_string(i + 1) + ":" + (percent < 0 ? "-" : " ") + padded + " ";
        if ((i + 1) % 15 == 0 || i + 1 == state.axes.size()) {
            text.renderTextLine(axes, row++, offset);
            axes.clear();
        }
    }

    switch (stage) {
    case Stage::Test:
        text.renderTextLine(
            _("NOTE: Make sure none of the buttons are pressed before mapping and all analog sticks are in default "
              "position."),
            7, offset, XALIGN_CENTER);
        text.renderTextLine(_("You can test your controller"), 8, offset, XALIGN_CENTER);
        break;
    case Stage::Mapping: {
        renderElements();
        string ask;
        switch (elements[current].scan) {
        case PadMapping::Scan::Digital:
            ask = _("Press a button highlighted or (OPEN) if not avaliable.");
            break;
        case PadMapping::Scan::Trigger:
            ask = _("Press a trigger highlighted fully or (OPEN) if not avaliable.");
            break;
        case PadMapping::Scan::Analog:
            ask = _("Move your sticks to state shown or press (OPEN) if stick position not avaliable.");
            break;
        }
        text.renderTextLine(ask, 7, offset, XALIGN_CENTER);
        text.renderTextLine(_("Updating mapping"), 8, offset, XALIGN_CENTER);
        break;
    }
    case Stage::Save:
        renderElements();
        text.renderTextLine(_("Mapping Complete - press (OPEN) to save. (POWER) to cancel."), 7, offset, XALIGN_CENTER);
        text.renderTextLine(_("Please test a new mapping"), 8, offset, XALIGN_CENTER);
        break;
    }

    renderPadPicture(joystick.controllerState());

    if (stage == Stage::Test)
        gui->renderStatus("(RESET) " + _("Next pad") + "   (OPEN) " + _("Update mapping") + "   (POWER) " + _("Exit"));
    else if (stage == Stage::Mapping)
        gui->renderStatus("(OPEN) " + _("No button on controller") + "  (POWER) " + _("Cancel mapping"));
    else
        gui->renderStatus("(OPEN) " + _("Save") + "  (POWER) " + _("Cancel mapping"));
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
