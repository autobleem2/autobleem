#include "core/virtual_pad.h"

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

const char *const kPscLine = "030000004c050000da0c000011010000,Playstation Classic Controller,"
                             "a:b2,b:b1,x:b3,y:b0,back:b8,start:b9,"
                             "leftshoulder:b6,lefttrigger:b4,rightshoulder:b7,righttrigger:b5,"
                             "dpup:-a1,dpdown:+a1,dpleft:-a0,dpright:+a0,platform:Linux,";

VirtualLayout makeLayout(const char *line, int buttons, int axes, int hats, bool triggersRestLow) {
    VirtualLayout layout;
    PadMapping::parseLine(line, layout.mapping);
    layout.name = layout.mapping.name;
    layout.guid = layout.mapping.guid;
    layout.buttonCount = buttons;
    layout.axisCount = axes;
    layout.hatCount = hats;
    layout.triggerAxesRestAtMinimum = triggersRestLow;
    return layout;
}

} // namespace

//*******************************
// virtualLayout
//*******************************
const VirtualLayout &virtualLayout(VirtualPadKind kind) {
    static const VirtualLayout x360 = makeLayout(kX360Line, 11, 6, 1, true);
    static const VirtualLayout psc = makeLayout(kPscLine, 10, 2, 0, false);
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
PhysicalState buildRawState(const VirtualLayout &layout, const ControllerState &controller) {
    PhysicalState raw;
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

} // namespace abpad
