#ifndef ABPAD_KEYBOARD_MODE_H
#define ABPAD_KEYBOARD_MODE_H

// Keyboard mode's logic: which key events a pad's changes are worth.
//
// The shim asks it once per pad per update and turns what comes back into the app's SDL key events. It is kept out of
// the shim so it can be tested without an SDL, a daemon or a shared block.
//
// Two elements (or two players) may be bound to one key - A and Start both on Return, say. The key is then down while
// any of them is held and goes up when the last one is let go, as a real keyboard's key does; a release of one while
// another still holds it must not lift the key under the app.

#include "core/key_names.h"
#include "core/mapping.h"
#include "core/shared_state.h"

#include <vector>

namespace abpad {

struct KeyChange {
    KeyCode key;
    bool down = false;
};

class KeyboardMode {
public:
    // the key an element is sent as; an invalid key unbinds it
    void bind(Element element, const KeyCode &key);

    // The pad's state now; appends the key events its change since the last call is worth. A pad that is not there
    // is passed as a neutral state, which lifts every key it held.
    void update(int pad, const ControllerState &state, std::vector<KeyChange> &out);

private:
    bool heldElsewhere(int pad, int element) const;

    KeyCode keys_[ElementCount];
    bool held_[MaxPads][ElementCount] = {};
};

} // namespace abpad

#endif
