#include "core/keyboard_mode.h"

namespace abpad {

//*******************************
// KeyboardMode::bind
//*******************************
void KeyboardMode::bind(Element element, const KeyCode &key) {
    int index = static_cast<int>(element);
    if (index >= 0 && index < ElementCount) {
        keys_[index] = key;
    }
}

//*******************************
// KeyboardMode::heldElsewhere - is the key of (pad, element) also held by another pad or element
//*******************************
bool KeyboardMode::heldElsewhere(int pad, int element) const {
    for (int p = 0; p < MaxPads; ++p) {
        for (int i = 0; i < ElementCount; ++i) {
            if ((p == pad && i == element) || !held_[p][i] || !keys_[i].valid()) {
                continue;
            }
            if (keys_[i].sdl2Scancode == keys_[element].sdl2Scancode) {
                return true;
            }
        }
    }
    return false;
}

//*******************************
// KeyboardMode::update
//*******************************
void KeyboardMode::update(int pad, const ControllerState &state, std::vector<KeyChange> &out) {
    if (pad < 0 || pad >= MaxPads) {
        return;
    }
    for (int i = 0; i < ElementCount; ++i) {
        if (!keys_[i].valid()) {
            continue;
        }
        bool now = state.held(static_cast<Element>(i));
        if (held_[pad][i] == now) {
            continue;
        }
        held_[pad][i] = now;
        if (!heldElsewhere(pad, i)) {
            KeyChange change;
            change.key = keys_[i];
            change.down = now;
            out.push_back(change);
        }
    }
}

} // namespace abpad
