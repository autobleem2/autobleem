#pragma once

#include "gui_base.h"
#include "input.h"

namespace ableem {

//******************
// GuiScreen
//******************
// Base class for one full-screen UI screen (a menu, a dialog, the launcher...). Moved here verbatim from the
// app's original gui/gui_screen.h, just rebased onto ableem::Event instead of raw SDL events. The app's own
// Gui-aware behavior (playing a cursor sound, drawing themed text, etc.) belongs in a subclass, not here.
class ABLEEM_API GuiScreen {
public:
    explicit GuiScreen(GuiBase &_gui) : gui(_gui) {}
    virtual ~GuiScreen() {}

    GuiBase &gui;
    bool menuVisible = true; // set this to false to exit the loop() here or your inherited loop()

    virtual void init() {}
    virtual void render() = 0;
    virtual void loop();

    // usage example:
    // void doSomeJoyEvent() {
    //      do {
    //          whatever you want to do on the event
    //          render();
    //      } while (fastForwardUntilAnotherEvent(300));  // repeat every 300 milliseconds
    // by the time render() finishes the user may have already pushed the button one or more times
    bool fastForwardUntilAnotherEvent(unsigned int ticksPerFastForwardRepeat = 200);

    void show() {
        init();
        render();
        loop();
    }

    // controller dpad/joystick pressed
    virtual void doJoyUp() {}
    virtual void doJoyDown() {}
    virtual void doJoyRight() {}
    virtual void doJoyLeft() {}
    virtual void doJoyCenter() {}

    // controller button pressed
    virtual void doCross_Pressed() {}
    virtual void doCircle_Pressed() {}
    virtual void doTriangle_Pressed() {}
    virtual void doSquare_Pressed() {}
    virtual void doStart_Pressed() {}
    virtual void doSelect_Pressed() {}

    virtual void doL1_Pressed() {}
    virtual void doR1_Pressed() {}
    virtual void doL2_Pressed() {}
    virtual void doR2_Pressed() {}

    // controller button released
    virtual void doCircle_Released() {}
    virtual void doCross_Released() {}
    virtual void doTriangle_Released() {}
    virtual void doSquare_Released() {}
    virtual void doStart_Released() {}
    virtual void doSelect_Released() {}

    virtual void doL1_Released() {}
    virtual void doR1_Released() {}
    virtual void doL2_Released() {}
    virtual void doR2_Released() {}

    // keyboard
    virtual void doKeyUp() {}
    virtual void doKeyDown() {}
    virtual void doKeyRight() {}
    virtual void doKeyLeft() {}
    virtual void doPageDown() {}
    virtual void doPageUp() {}
    virtual void doHome() {}
    virtual void doEnd() {}
    virtual void doEnter() {}
    virtual void doDelete() {}
    virtual void doBackspace() {}
    virtual void doTab() {}
    virtual void doEscape() {}
    virtual void doTextInput(const std::string &) {}

    // returns true if this event was a Quit (PC window close): sets menuVisible = false.
    // the power button / Esc case is handled by Input itself (Platform::setPowerOffHandler), so screens no
    // longer need to check for it themselves.
    bool handleQuit(const Event &e);
};

} // namespace ableem
