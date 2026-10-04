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

#include <cstdint>
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
    // what SDL_JoystickGetVendor/Product/ProductVersion and ...GetType answer for this pad
    uint16_t vendor = 0;
    uint16_t product = 0;
    uint16_t version = 0;
    int controllerType = 0; // SDL_GameControllerType: 0 unknown, 1 Xbox 360
};

const VirtualLayout &virtualLayout(VirtualPadKind kind);
// "x360" / "psc"; anything else comes back as X360
VirtualPadKind virtualPadKindFromName(const std::string &name);
const char *virtualPadKindName(VirtualPadKind kind);

// the controller state as the virtual pad's own raw buttons, axes and hats
RawPadState buildRawState(const VirtualLayout &layout, const ControllerState &controller);

// a trigger counts as the console pad's L2/R2 button only near full pull: the original remap took a trigger for the
// button at 32767 exactly (pad-mapping.md 1.1); a little short of it, so a pad whose trigger tops out one step low
// still presses it
constexpr int16_t TriggerFullPull = 32000;

// What the app's GameController API answers on a pad of this kind, from the physical pad's state, with the per-App
// movement flags (`aid`: DpadToStick = Dpad2Analog, StickToDpad = Analog2Dpad).
// X360: the state as it is, the d-pad and the stick standing in for each other as `aid` says.
// Psc: the console pad as the original remap made any pad into it (pad-mapping.md 1.1): the d-pad on the left stick's
// two axes at their ends (-32768 / 32767 - the console pad's table reads its d-pad as leftx/lefty), a pushed left stick
// feeding the same axes with its own value once past half travel (only with Analog2Dpad, which is on by default: the
// remap always did it; nothing inside half travel is ever input, so drift is not), L2/R2 as buttons at full pull, no
// d-pad buttons, no right stick, no stick clicks, no guide. Dpad2Analog adds nothing: the console pad has no stick.
ControllerState controllerView(VirtualPadKind kind, const ControllerState &physical, MovementAid aid);

// A mod's own pad-remap preload that the shim stands in for (rc/pe_compat.ini remap=, passed as AB_PAD_REMAP): the
// shim then also does what that library did on top of the console pad, so the mod's config still fits.
enum class ModRemap {
    None,
    Drastic, // drastic_sdl_remap.so (pad-mapping.md 1.2): the d-pad axes reported as hat 0, the buttons renumbered
};
ModRemap modRemapFromFileName(const std::string &fileName);
// DraStic's remap, as events on the console pad: the number a console pad button b0..b9 is reported as (-1: none)
int drasticButton(int pscButton);
// ... and the hat 0 mask the d-pad axes 0/1 are reported as (1 up, 2 right, 4 down, 8 left)
uint8_t drasticHat(int16_t axis0, int16_t axis1);

} // namespace abpad

#endif
