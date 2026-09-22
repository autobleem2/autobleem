#ifndef ABPAD_PROFILE_H
#define ABPAD_PROFILE_H

// What an App wants the shim to do - Apps/<name>/pad.ini, or the default next to app_env.sh.
//
//     mode = joystick          # joystick | keyboard | both | off
//     virtual = x360           # the pad a joystick-mode app is shown
//     players = 1
//     hotkey = start+select    # held together, quits the app
//     key.a = Left Ctrl        # keyboard mode: an element -> a key the app already understands
//     key.dpup = Up
//
// Deliberately not an ableem IniFile: the shim links nothing (see the CMakeLists), and the format is
// small enough that reading it here costs less than the dependency would.

#include "core/mapping.h"
#include "core/virtual_pad.h"

#include <iosfwd>
#include <map>
#include <string>
#include <vector>

namespace abpad {

enum class PadMode {
    Off,      // the shim stands aside - the app already does the right thing with the pad
    Joystick, // the app is shown one well-known pad
    Keyboard, // the pad arrives as key presses
    Both,
};

PadMode padModeFromName(const std::string &name); // an unknown name is Joystick
const char *padModeName(PadMode mode);

//*******************************
// Profile
//*******************************
struct Profile {
    PadMode mode = PadMode::Joystick;
    VirtualPadKind virtualPad = VirtualPadKind::X360;
    int players = 1;
    std::vector<Element> hotkey; // all held together; empty = no hotkey
    std::map<Element, std::string> keys;
    std::string logPath;
    // These machines have no mouse, so a cursor on screen is never wanted - but an app is free to
    // turn one on, and several do (SDLPoP shows one with its menu). Hidden unless a profile says
    // otherwise, and independently of `mode`: an app that needs no help with its pad may still put
    // a cursor on the television.
    bool hideCursor = true;

    bool wantsJoystick() const { return mode == PadMode::Joystick || mode == PadMode::Both; }
    bool wantsKeyboard() const { return mode == PadMode::Keyboard || mode == PadMode::Both; }
    // the key an element is to be sent as, "" when the profile does not name one
    std::string keyFor(Element element) const;

    void loadStream(std::istream &in); // applied over what is already there
    bool loadFile(const std::string &path);
};

// "start+select" -> {Start, Back}; an unknown name is dropped, so a typo costs that button and not
// the whole hotkey
std::vector<Element> parseHotkey(const std::string &text);
// true when every element of the hotkey is held
bool hotkeyHeld(const std::vector<Element> &hotkey, const ControllerState &state);

} // namespace abpad

#endif
