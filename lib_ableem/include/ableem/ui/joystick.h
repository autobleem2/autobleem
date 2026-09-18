#pragma once

#include <memory>
#include <string>
#include <vector>
#include "types.h"

namespace ableem {

//******************
// JoystickState
//******************
// a raw snapshot of one device: every axis, button and hat as SDL's joystick API reports them, before
// any game-controller mapping. Hat values are the SDL_HAT_* bit masks (Joystick::HatUp and friends).
struct JoystickState {
    std::vector<int> axes;      // -32768..32767
    std::vector<bool> buttons;  // pressed
    std::vector<unsigned> hats; // HatCentered / HatUp | HatDown | HatLeft | HatRight
};

//******************
// ControllerState
//******************
// the same device through its mapping, when it has one: SDL's 15 standard buttons and 6 axes
struct ControllerState {
    bool buttons[15] = {};
    int axes[6] = {};
};

//******************
// Joystick
//******************
// One joystick device by index, opened raw - what a pad-mapping wizard reads: it shows a pad's raw inputs,
// watches which one the user moves for each standard button, and checks the mapping it built through the
// controller view. Independent of Input's own pads (which are opened as game controllers); close() before
// Input::probePads() re-opens the device as a controller. The joystick subsystem must be initialised
// (Input::probePads() does).
class ABLEEM_API Joystick {
public:
    Joystick();
    ~Joystick();
    Joystick(const Joystick &) = delete;
    Joystick &operator=(const Joystick &) = delete;

    // SDL_NumJoysticks()
    static int count();
    // the device's name and whether SDL has a mapping for it, without opening it
    static std::string nameForIndex(int index);
    static std::string guidForIndex(int index);
    static bool isGameControllerAtIndex(int index);
    static std::string controllerNameForIndex(int index);

    // opens the device at `index`; false when there is none. Closes the previous one first.
    bool open(int index);
    void close();
    bool isOpen() const;

    int index() const;
    std::string name() const; // SDL_JoystickName
    std::string guid() const;
    bool isGameController() const; // opened with a mapping; controllerState() is meaningful

    // reads the device now (SDL_JoystickUpdate) into state()/controllerState()
    void update();
    const JoystickState &state() const;
    const ControllerState &controllerState() const;

    enum : unsigned { HatCentered = 0, HatUp = 1, HatRight = 2, HatDown = 4, HatLeft = 8 };

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace ableem
