//
// GuiPadConfig: the pad test and mapping wizard. Shows one joystick raw (its axes, buttons and hats as
// numbers, and a DualShock picture lit by the mapped view); Reset (or Start on a keyboard) moves to the
// next pad, Open (or Return) starts mapping: each of the 25 standard inputs is highlighted on the picture
// in turn and the raw input the user moves is taken for it (Open skips one). At the end the mapping is
// added to SDL for a test, and Open once more names the pad and writes it to the gamecontrollerdb.txt in
// use. Power (or Escape) cancels a mapping, or closes the screen.
//
#pragma once

#include "gui/gui_screen.h"
#include "core/pad_mapping.h"

#include <ableem/ui/joystick.h>
#include <ableem/ui/texture.h>

#include <string>
#include <vector>

//********************
// GuiPadConfig
//********************
class GuiPadConfig : public GuiScreen {
public:
    using GuiScreen::GuiScreen;

    void init() override;
    void render() override;
    void loop() override;

private:
    enum class Stage { Test, Mapping, Save };
    Stage stage = Stage::Test;

    ableem::Joystick joystick;
    ableem::JoystickState initialState; // the pad at rest, when the mapping started
    std::vector<PadMapping::Element> elements;
    std::vector<PadMapping::Element> finals;
    size_t current = 0;          // the element being asked for
    std::string originalMapping; // SDL's mapping before the wizard, put back on cancel
    int joysticksAtStart = 0;    // a pad plugged or pulled mid-mapping cancels it
    ableem::Texture padImage;    // DS3.png, from the tool's own folder

    void openJoystick(int index);
    void nextJoystick();
    void startMapping();
    void cancelMapping();
    void takeInput(const std::string &value);
    void advance();
    void finishMapping();
    void saveMapping();
    void waitUntilReleased();

    void renderPadPicture(const ableem::ControllerState &state);
    void renderElements();
    std::string joystickTitle() const;
};
