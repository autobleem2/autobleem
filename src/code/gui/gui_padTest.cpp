
#include "gui_padTest.h"
#include <unistd.h>
#include <string>
#include "gui.h"
#include "../core/lang.h"
#include <iostream>
#include <sstream>

using namespace std;

namespace {
string buttonName(ableem::Button b) {
    switch (b) {
        case ableem::Button::Cross: return "Cross";
        case ableem::Button::Circle: return "Circle";
        case ableem::Button::Square: return "Square";
        case ableem::Button::Triangle: return "Triangle";
        case ableem::Button::Start: return "Start";
        case ableem::Button::Select: return "Select";
        case ableem::Button::L1: return "L1";
        case ableem::Button::R1: return "R1";
        case ableem::Button::L2: return "L2";
        case ableem::Button::R2: return "R2";
        case ableem::Button::DpadUp: return "DpadUp";
        case ableem::Button::DpadDown: return "DpadDown";
        case ableem::Button::DpadLeft: return "DpadLeft";
        case ableem::Button::DpadRight: return "DpadRight";
        default: return "None";
    }
}
} // namespace

//*******************************
// GuiPadTest::init
//*******************************
void GuiPadTest::init() {
    GuiScrollWin::init();
    if (joyid != -1) {
        appendLine("-=" + _("New GamePad found") + "=-");
        auto pads = gui->input().pads();
        for (const auto &pad : pads) {
            if (pad.index == joyid) {
                appendLine(pad.name);
                break;
            }
        }
    }
    appendLine("Hold down three buttons to exit");
}

//*******************************
// GuiPadTest::loop
//*******************************
void GuiPadTest::loop() {
    // eat any events in the queue
    Event e;
    while (gui->input().poll(e))
        ;

    int buttonDownCount = 0;
    while (buttonDownCount >= 0 && buttonDownCount < 3) {
        bool status = gui->input().poll(e);
        if (status) {
            if (e.type == Event::Type::KeyDown) {
                appendLine("KeyDown");
            } else if (e.type == Event::Type::KeyUp) {
                appendLine("KeyUp");
            } else if (e.type == Event::Type::ButtonDown) {
                appendLine("ButtonDown = " + buttonName(e.button));
                buttonDownCount++;
            } else if (e.type == Event::Type::ButtonUp) {
                appendLine("ButtonUp = " + buttonName(e.button));
                if (buttonDownCount > 0)
                    buttonDownCount--;
            } else if (e.type == Event::Type::DpadUp) {
                appendLine("DpadUp = " + buttonName(e.button));
            } else if (e.type == Event::Type::DpadDown) {
                appendLine("DpadDown = " + buttonName(e.button));
            } else if (e.type == Event::Type::Quit) {
                ; // ignore
            } else {
                appendLine("something else");
            }
            render();
        }
    }
    appendLine("Release all buttons now");
    sleep(3);
    while (gui->input().poll(e))
        ;   // eat any events in the queue
}
