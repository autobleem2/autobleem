#ifndef ABPAD_KEY_NAMES_H
#define ABPAD_KEY_NAMES_H

// The keys a profile may name, in both SDLs' numbers.
//
// Keyboard mode hands a keyboard-only game (wolf4sdl, sdlpop, opentyrian - DOS ports, and the console
// has no keyboard) the pad as key presses, so the profile has to name keys and the shim has to push
// them in whatever the app's SDL calls them. The two disagree: SDL 1.2's SDLK_UP is 273, SDL2's is a
// scancode of 82 and a keycode of 1073741906. Both are kernel-stable constants, so the table carries
// both and the shim picks the column for the ABI it detected.

#include <cstdint>
#include <string>

namespace abpad {

//*******************************
// KeyCode - one key, in both SDLs
//*******************************
struct KeyCode {
    int sdl1Sym = 0;      // SDL 1.2's SDLKey
    int sdl2Scancode = 0; // SDL2's SDL_Scancode
    int sdl2Keycode = 0;  // SDL2's SDL_Keycode
    bool known = false;

    bool valid() const { return known; }
};

// "Up", "left ctrl", "lctrl", "a", "F1", "Space", "Return" - case and spaces do not matter, and the
// short names a config file writer reaches for are accepted next to the long ones. An unknown name
// comes back with valid() false, so a typo is reported rather than silently sending nothing.
KeyCode keyCodeFromName(const std::string &name);

} // namespace abpad

#endif
