#include "ableem/ui/gui_screen.h"

namespace ableem {

//*******************************
// GuiScreen::loop
//*******************************
void GuiScreen::loop() {
    menuVisible = true;
    Input &input = gui.input();

    while (menuVisible) {
        Event e;
        while (input.poll(e)) {
            if (handleQuit(e))
                continue;

            switch (e.type) {
                case Event::Type::DpadDown:
                case Event::Type::DpadUp:
                    // priority order matches the original PadMapper behavior: report whichever direction is
                    // currently held, not just the one this particular event changed.
                    if (input.dpadUp()) doJoyUp();
                    else if (input.dpadDown()) doJoyDown();
                    else if (input.dpadRight()) doJoyRight();
                    else if (input.dpadLeft()) doJoyLeft();
                    else if (input.dpadCentered()) doJoyCenter();
                    break;

                case Event::Type::ButtonDown:
                    switch (e.button) {
                        case Button::Cross: doCross_Pressed(); break;
                        case Button::Circle: doCircle_Pressed(); break;
                        case Button::Triangle: doTriangle_Pressed(); break;
                        case Button::Square: doSquare_Pressed(); break;
                        case Button::Start: doStart_Pressed(); break;
                        case Button::Select: doSelect_Pressed(); break;
                        case Button::L1: doL1_Pressed(); break;
                        case Button::R1: doR1_Pressed(); break;
                        case Button::L2: doL2_Pressed(); break;
                        case Button::R2: doR2_Pressed(); break;
                        default: break;
                    }
                    break;

                case Event::Type::ButtonUp:
                    switch (e.button) {
                        case Button::Cross: doCross_Released(); break;
                        case Button::Circle: doCircle_Released(); break;
                        case Button::Triangle: doTriangle_Released(); break;
                        case Button::Square: doSquare_Released(); break;
                        case Button::Start: doStart_Released(); break;
                        case Button::Select: doSelect_Released(); break;
                        case Button::L1: doL1_Released(); break;
                        case Button::R1: doR1_Released(); break;
                        case Button::L2: doL2_Released(); break;
                        case Button::R2: doR2_Released(); break;
                        default: break;
                    }
                    break;

                case Event::Type::KeyDown:
                    switch (e.key) {
                        case Key::Up: doKeyUp(); break;
                        case Key::Down: doKeyDown(); break;
                        case Key::Right: doKeyRight(); break;
                        case Key::Left: doKeyLeft(); break;
                        case Key::PageDown: doPageDown(); break;
                        case Key::PageUp: doPageUp(); break;
                        case Key::Home: doHome(); break;
                        case Key::End: doEnd(); break;
                        case Key::Return: doEnter(); break;
                        case Key::Delete: doDelete(); break;
                        case Key::Backspace: doBackspace(); break;
                        case Key::Tab: doTab(); break;
                        case Key::Escape: doEscape(); break;
                        default: break;
                    }
                    break;

                case Event::Type::TextInput:
                    doTextInput(e.text);
                    break;

                default:
                    break;
            }
        }
        render();
    }
}

//*******************************
// GuiScreen::handleQuit
//*******************************
bool GuiScreen::handleQuit(const Event &e) {
    if (e.type == Event::Type::Quit) {     // this is for PC only
        menuVisible = false;
        return true;
    }
    return false;
}

//*******************************
// GuiScreen::fastForwardUntilAnotherEvent
//*******************************
// usage example:
// void doSomeJoyEvent() {
//      do {
//          whatever you want to do on the event
//          render();
//      } while (fastForwardUntilAnotherEvent(300));  // repeat every 300 milliseconds
bool GuiScreen::fastForwardUntilAnotherEvent(unsigned int ticksPerFastForwardRepeat) {
    unsigned int startTicks = gui.platform().ticks();
    while (true) {
        unsigned int time = gui.platform().ticks() - startTicks;
        if (time >= ticksPerFastForwardRepeat) {
            return true;    // fast forward - repeat key
        }
        if (gui.input().padEventPending()) {
            return false;   // exit fast forward mode
        }
    }
}

} // namespace ableem
