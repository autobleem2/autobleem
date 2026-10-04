#include "core/virtual_pad.h"

#include <cstdint>

using namespace std;

namespace abpad {

namespace {

// The layouts, as the lines that describe them in any gamecontrollerdb.txt. Keeping them in this form
// is deliberate: it is checkable against the real file, and it is what a reader of that file already
// knows how to read.
const char *const kX360Line = "030000005e0400008e02000010010000,Microsoft X-Box 360 pad,"
                              "a:b0,b:b1,x:b2,y:b3,back:b6,guide:b8,start:b7,leftstick:b9,rightstick:b10,"
                              "leftshoulder:b4,rightshoulder:b5,dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8,"
                              "leftx:a0,lefty:a1,rightx:a3,righty:a4,lefttrigger:a2,righttrigger:a5,platform:Linux,";

// The console's own pad exactly as the original product's pad table has it (pad-mapping.md 1.3): the real device's
// name, its GUID (USB 054c:0cda version 0111), and the d-pad - two axes of 0..2 - as the left stick.
const char *const kPscLine = "030000004c050000da0c000011010000,Sony Interactive Entertainment Controller,"
                             "a:b2,b:b1,x:b3,y:b0,back:b8,start:b9,"
                             "leftshoulder:b6,lefttrigger:b4,rightshoulder:b7,righttrigger:b5,"
                             "leftx:a0,lefty:a1,platform:Linux,";

struct LayoutFacts {
    int buttons;
    int axes;
    int hats;
    bool triggersRestLow;
    uint16_t vendor;
    uint16_t product;
    uint16_t version;
    int controllerType;
};

VirtualLayout makeLayout(const char *line, const LayoutFacts &facts) {
    VirtualLayout layout;
    PadMapping::parseLine(line, layout.mapping);
    layout.name = layout.mapping.name;
    layout.guid = layout.mapping.guid;
    layout.buttonCount = facts.buttons;
    layout.axisCount = facts.axes;
    layout.hatCount = facts.hats;
    layout.triggerAxesRestAtMinimum = facts.triggersRestLow;
    layout.vendor = facts.vendor;
    layout.product = facts.product;
    layout.version = facts.version;
    layout.controllerType = facts.controllerType;
    return layout;
}

// the value the console pad's d-pad axis takes: an end for the d-pad, else the left stick's own value past half
// travel when it may feed the d-pad (the remap passed it on as it was), else the centre
int16_t pscAxis(bool negative, bool positive, int16_t stick, bool stickFeeds) {
    if (negative != positive) {
        return negative ? static_cast<int16_t>(-32768) : static_cast<int16_t>(32767);
    }
    if (negative) {
        return 0; // both ends at once: nothing
    }
    if (stickFeeds && (stick > AxisButtonThreshold || stick < -AxisButtonThreshold)) {
        return stick;
    }
    return 0;
}

} // namespace

//*******************************
// virtualLayout
//*******************************
const VirtualLayout &virtualLayout(VirtualPadKind kind) {
    static const VirtualLayout x360 = makeLayout(kX360Line, {11, 6, 1, true, 0x045e, 0x028e, 0x0110, 1});
    static const VirtualLayout psc = makeLayout(kPscLine, {10, 2, 0, false, 0x054c, 0x0cda, 0x0111, 0});
    return (kind == VirtualPadKind::Psc) ? psc : x360;
}

//*******************************
// virtualPadKindFromName / virtualPadKindName
//*******************************
VirtualPadKind virtualPadKindFromName(const string &name) {
    return (name == "psc") ? VirtualPadKind::Psc : VirtualPadKind::X360;
}

const char *virtualPadKindName(VirtualPadKind kind) {
    return (kind == VirtualPadKind::Psc) ? "psc" : "x360";
}

//*******************************
// buildRawState
//*******************************
RawPadState buildRawState(const VirtualLayout &layout, const ControllerState &controller) {
    RawPadState raw;
    raw.buttons.assign(layout.buttonCount, false);
    raw.axes.assign(layout.axisCount, 0);
    raw.hats.assign(layout.hatCount, 0);

    // a trigger the layout puts on a whole axis rests at the bottom of its travel, not the middle
    if (layout.triggerAxesRestAtMinimum) {
        for (Element element : {Element::LeftTrigger, Element::RightTrigger}) {
            const Binding &binding = layout.mapping[element];
            if (binding.kind == Binding::Kind::Axis && binding.half == 0 && binding.index >= 0 &&
                binding.index < layout.axisCount) {
                raw.axes[binding.index] = -32768;
            }
        }
    }

    for (int i = 0; i < ElementCount; ++i) {
        Element element = static_cast<Element>(i);
        const Binding &binding = layout.mapping[element];
        if (!binding.bound()) {
            continue;
        }

        bool pressed = false;
        int16_t value = 0;
        if (isAxis(element)) {
            value = controller.axis(element);
            pressed = value >= AxisButtonThreshold || value <= -AxisButtonThreshold;
        } else {
            pressed = controller.button(element);
            value = pressed ? 32767 : 0;
        }

        switch (binding.kind) {
        case Binding::Kind::Button:
            if (binding.index < layout.buttonCount) {
                raw.buttons[binding.index] = pressed;
            }
            break;

        case Binding::Kind::Hat:
            if (binding.index < layout.hatCount && pressed) {
                raw.hats[binding.index] = static_cast<uint8_t>(raw.hats[binding.index] | binding.hatMask);
            }
            break;

        case Binding::Kind::Axis: {
            if (binding.index >= layout.axisCount) {
                break;
            }
            int written;
            if (isAxis(element)) {
                written = value;
                bool isTrigger = element == Element::LeftTrigger || element == Element::RightTrigger;
                if (isTrigger && layout.triggerAxesRestAtMinimum && binding.half == 0) {
                    // 0..32767 of pull over the axis's whole travel
                    written = (value > 0 ? value : 0) * 2 - 32768;
                }
            } else {
                // a button element on an axis: the d-pad of a pad that has no hat
                written = pressed ? (binding.half < 0 ? -32767 : 32767) : 0;
            }
            if (binding.inverted) {
                written = -written;
            }
            if (written > 32767) {
                written = 32767;
            }
            if (written < -32768) {
                written = -32768;
            }
            // two elements can share an axis (dpleft and dpright); the one that is pressed wins
            if (written != 0 || raw.axes[binding.index] == 0) {
                raw.axes[binding.index] = static_cast<int16_t>(written);
            }
            break;
        }

        case Binding::Kind::None:
            break;
        }
    }

    return raw;
}

//*******************************
// controllerView
//*******************************
ControllerState controllerView(VirtualPadKind kind, const ControllerState &physical, MovementAid aid) {
    if (kind != VirtualPadKind::Psc) {
        ControllerState view = physical;
        applyMovementAid(view, aid);
        return view;
    }
    ControllerState view;
    for (Element element : {Element::A, Element::B, Element::X, Element::Y, Element::Back, Element::Start,
                            Element::LeftShoulder, Element::RightShoulder}) {
        view.set(element, physical.button(element));
    }
    // L2/R2: buttons on the console's pad, pressed by a trigger at (nearly) full pull
    view.set(Element::LeftTrigger,
             static_cast<int16_t>(physical.axis(Element::LeftTrigger) >= TriggerFullPull ? 32767 : 0));
    view.set(Element::RightTrigger,
             static_cast<int16_t>(physical.axis(Element::RightTrigger) >= TriggerFullPull ? 32767 : 0));

    // the d-pad on the left stick's axes; the stick feeds them past half travel when Analog2Dpad is on
    const bool stickFeeds = aid == MovementAid::StickToDpad || aid == MovementAid::Both;
    view.set(Element::LeftX, pscAxis(physical.button(Element::DpLeft), physical.button(Element::DpRight),
                                     physical.axis(Element::LeftX), stickFeeds));
    view.set(Element::LeftY, pscAxis(physical.button(Element::DpUp), physical.button(Element::DpDown),
                                     physical.axis(Element::LeftY), stickFeeds));
    return view;
}

//*******************************
// modRemapFromFileName / drasticButton / drasticHat
//*******************************
ModRemap modRemapFromFileName(const string &fileName) {
    string name = fileName;
    size_t slash = name.find_last_of('/');
    if (slash != string::npos) {
        name = name.substr(slash + 1);
    }
    return name == "drastic_sdl_remap.so" ? ModRemap::Drastic : ModRemap::None;
}

int drasticButton(int pscButton) {
    // Triangle, Circle, Cross, Square, L2, R2, L1, R1, Select, Start -> DraStic's numbers (pad-mapping.md 1.2)
    static const int numbers[10] = {3, 1, 0, 2, 8, 9, 4, 5, 7, 6};
    return (pscButton >= 0 && pscButton < 10) ? numbers[pscButton] : -1;
}

uint8_t drasticHat(int16_t axis0, int16_t axis1) {
    int mask = (axis0 > 0 ? 2 : (axis0 < 0 ? 8 : 0)) | (axis1 > 0 ? 4 : (axis1 < 0 ? 1 : 0));
    return static_cast<uint8_t>(mask);
}

} // namespace abpad
