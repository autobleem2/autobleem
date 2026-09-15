#pragma once

#include <string>
#include <vector>
#include <functional>
#include "types.h"

namespace ableem {

class Platform;

//******************
// Button
//******************
// PlayStation-style pad buttons. L2/R2 are reported as buttons (the library turns the analog trigger axes
// into press/release events for you, as the original PSC event filter did).
enum class Button {
    None, Cross, Circle, Square, Triangle, Start, Select, L1, R1, L2, R2,
    DpadUp, DpadDown, DpadLeft, DpadRight
};

//******************
// Key
//******************
// Keyboard keys the app cares about (menus, the on-screen keyboard, dev-host debugging). Anything else comes
// through as Key::Other.
enum class Key {
    Other, Escape, Return, Up, Down, Left, Right, PageUp, PageDown, Home, End, Tab, Backspace, Delete, Sleep
};

//******************
// Event
//******************
struct Event {
    enum class Type {
        None, Quit, ButtonDown, ButtonUp, DpadDown, DpadUp, KeyDown, KeyUp, TextInput,
        PadAdded, PadRemoved, RenderReset
    };
    Type type = Type::None;
    Button button = Button::None;  // valid for ButtonDown/Up, DpadDown/Up
    Key key = Key::Other;          // valid for KeyDown/Up
    std::string text;              // valid for TextInput (a UTF-8 chunk of typed text)
};

//******************
// PadInfo
//******************
struct PadInfo {
    std::string name;
    std::string guid;
    int index = 0;
};

//******************
// Input
//******************
// Wraps SDL's event queue, game controller API and joystick hot-plug, plus the platform-specific quirks that
// used to live in gui/abl.c (PSC analog-trigger-to-button simulation) and engine/padmapper.* (dpad state
// tracking, mapping file loading). Owned by GuiBase.
//
// The power button / Esc key is intercepted here: poll() calls the app's power-off handler (see
// Platform::setPowerOffHandler) instead of surfacing those as ordinary key events.
class ABLEEM_API Input {
public:
    explicit Input(Platform &platform);
    ~Input();
    Input(const Input &) = delete;
    Input &operator=(const Input &) = delete;

    // pulls one event off the queue, translating it and updating internal dpad/pad state as a side effect.
    // returns false when the queue is empty.
    bool poll(Event &out);

    void flushEvents(); // discard everything currently queued (SDL_PumpEvents + SDL_FlushEvents)

    // true if a controller axis/hat event is sitting in the queue right now, without consuming it. used to
    // implement "repeat while held" key-repeat style loops (see GuiScreen::fastForwardUntilAnotherEvent).
    bool padEventPending() const;

    // current dpad state, as tracked from the DpadDown/DpadUp events seen by poll()
    bool dpadUp() const;
    bool dpadDown() const;
    bool dpadLeft() const;
    bool dpadRight() const;
    bool dpadCentered() const;

    // on a dev host (see Platform::isDevHost), keyboard keys are turned into pad Button/Dpad events so the
    // app is usable without a real controller. On by default on dev hosts, off elsewhere.
    void setKeyboardAsPad(bool enabled);

    void loadMappings(const std::vector<std::string> &gameControllerDbPaths);
    void probePads();  // (re)opens the joystick/game controller subsystem and registers already-connected pads
    void flushPads();  // closes every open pad (e.g. before handing control to another program)
    int activePadCount() const;
    int joystickCount() const; // SDL_NumJoysticks(), including devices that are not recognized as game controllers
    std::vector<PadInfo> pads() const;

private:
    struct Impl;
    Impl *impl;
};

} // namespace ableem
