#ifndef ABPAD_VIRTUAL_PAD_H
#define ABPAD_VIRTUAL_PAD_H

// The pad the app is shown.
//
// A virtual layout is a mapping line read backwards: where applyMapping() reads a physical pad
// *through* its gamecontrollerdb line to get a ControllerState, buildRawState() writes that
// ControllerState *through* the layout's own line to get the raw buttons/axes/hats an app asks SDL
// for. So the two ends of the shim are the same piece of logic twice, and a new layout is one more
// gamecontrollerdb line rather than any new code.

#include "core/mapping.h"

#include <string>

namespace abpad {

enum class VirtualPadKind {
    X360, // the wired Xbox 360 pad - what nearly every Linux port was written against
    Psc,  // the console's own pad, for an app that was ported for it but is handed another pad
};

//*******************************
// VirtualLayout - a well-known pad, described the way a gamecontrollerdb.txt line describes one
//*******************************
struct VirtualLayout {
    std::string name;
    std::string guid;
    int buttonCount = 0;
    int axisCount = 0;
    int hatCount = 0;
    // the triggers rest at the bottom of their travel rather than the middle (the X360 pad does;
    // an app reading its raw joystick expects -32768 on an untouched trigger)
    bool triggerAxesRestAtMinimum = false;
    PadMapping mapping;
};

const VirtualLayout &virtualLayout(VirtualPadKind kind);
// "x360" / "psc"; anything else comes back as X360
VirtualPadKind virtualPadKindFromName(const std::string &name);
const char *virtualPadKindName(VirtualPadKind kind);

// the controller state as the virtual pad's own raw buttons, axes and hats
RawPadState buildRawState(const VirtualLayout &layout, const ControllerState &controller);

// What the app's GameController API answers on a pad of this kind, from the physical pad's state. X360: the state
// as it is (the caller adds the movement aid). Psc: the console pad as a program ported for it sees it - the d-pad is
// the only direction (a stick pushed past the threshold presses it; both are also the left stick's two axes at
// full travel, which is how the original product's pad table reads it), L2/R2 are buttons (their trigger axes are
// 0 or full), and there is no right stick, no stick click and no guide: nothing an idle stick or trigger does is
// ever input.
ControllerState controllerView(VirtualPadKind kind, const ControllerState &physical);

} // namespace abpad

#endif
