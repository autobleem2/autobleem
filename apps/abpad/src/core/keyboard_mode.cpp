#include "core/keyboard_mode.h"

namespace abpad {

//*******************************
// KeyboardMode::bind
//*******************************
void KeyboardMode::bind(Element element, const KeyCode &key, bool negative) {
    int index = static_cast<int>(element);
    if (index < 0 || index >= ElementCount) {
        return;
    }
    if (negative) {
        if (!isAxis(element)) {
            return;
        }
        index = ElementCount + (index - FirstAxisElement);
    }
    keys_[index] = key;
}

//*******************************
// KeyboardMode::heldElsewhere - is the key of (pad, slot) also held by another pad or slot
//*******************************
bool KeyboardMode::heldElsewhere(int pad, int slot) const {
    for (int p = 0; p < MaxPads; ++p) {
        for (int i = 0; i < SlotCount; ++i) {
            if ((p == pad && i == slot) || !held_[p][i] || !keys_[i].valid()) {
                continue;
            }
            if (keys_[i].sdl2Scancode == keys_[slot].sdl2Scancode) {
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
    for (int i = 0; i < SlotCount; ++i) {
        if (!keys_[i].valid()) {
            continue;
        }
        bool now = (i < ElementCount) ? state.held(static_cast<Element>(i))
                                      : state.heldNegative(static_cast<Element>(FirstAxisElement + i - ElementCount));
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
