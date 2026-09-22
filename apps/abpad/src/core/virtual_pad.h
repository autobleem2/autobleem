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
PhysicalState buildRawState(const VirtualLayout &layout, const ControllerState &controller);

} // namespace abpad

#endif
