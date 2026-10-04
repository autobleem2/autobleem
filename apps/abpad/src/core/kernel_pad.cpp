#include "core/kernel_pad.h"

#include <algorithm>

using namespace std;

namespace abpad {

namespace {

bool isHatCode(int code) {
    return code >= EvdevAbsHat0X && code <= EvdevAbsHat3Y;
}

int clampInt(int value, int low, int high) {
    return max(low, min(high, value));
}

// the console pad's two axes are 0, 1, 2 - one end, centre, the other end
int pscAxis(int16_t raw) {
    return raw < 0 ? 0 : (raw > 0 ? 2 : 1);
}

} // namespace

//*******************************
// EvdevPadState
//*******************************
EvdevPadState::EvdevPadState(const vector<int> &keyCodes, const vector<EvdevAbs> &absCodes) {
    vector<int> keys = keyCodes;
    sort(keys.begin(), keys.end());
    for (int code : keys) {
        if (code >= EvdevBtnJoystick && code < EvdevKeyMax) {
            buttonCodes_.push_back(code);
        }
    }
    for (int code : keys) {
        if (code >= EvdevBtnMisc && code < EvdevBtnJoystick) {
            buttonCodes_.push_back(code);
        }
    }
    buttons_.assign(buttonCodes_.size(), false);

    vector<EvdevAbs> axes = absCodes;
    sort(axes.begin(), axes.end(), [](const EvdevAbs &a, const EvdevAbs &b) { return a.code < b.code; });
    for (const EvdevAbs &abs : axes) {
        if (abs.code < 0 || abs.code > EvdevAbsMax) {
            continue;
        }
        if (isHatCode(abs.code)) {
            int xCode = abs.code & ~1;
            bool known = false;
            for (const Hat &hat : hats_) {
                known = known || hat.xCode == xCode;
            }
            if (!known) {
                Hat hat;
                hat.xCode = xCode;
                hats_.push_back(hat);
            }
        } else {
            Axis axis;
            axis.info = abs;
            axis.value = abs.min + (abs.max - abs.min) / 2; // resting until told otherwise
            axes_.push_back(axis);
        }
    }
}

void EvdevPadState::setKey(int code, int value) {
    for (size_t i = 0; i < buttonCodes_.size(); ++i) {
        if (buttonCodes_[i] == code) {
            buttons_[i] = value != 0;
            return;
        }
    }
}

void EvdevPadState::setAbs(int code, int value) {
    if (isHatCode(code)) {
        for (Hat &hat : hats_) {
            if (hat.xCode == (code & ~1)) {
                ((code & 1) == 0 ? hat.x : hat.y) = clampInt(value, -1, 1);
                return;
            }
        }
        return;
    }
    for (Axis &axis : axes_) {
        if (axis.info.code == code) {
            axis.value = value;
            return;
        }
    }
}

RawPadState EvdevPadState::raw() const {
    RawPadState state;
    state.buttons = buttons_;
    for (const Axis &axis : axes_) {
        int span = axis.info.max - axis.info.min;
        int value = 0;
        if (span > 0) {
            value = static_cast<int>((static_cast<long long>(axis.value - axis.info.min) * 65535) / span) - 32768;
        }
        state.axes.push_back(static_cast<int16_t>(clampInt(value, -32768, 32767)));
    }
    for (const Hat &hat : hats_) {
        int mask = (hat.y < 0 ? 1 : 0) | (hat.x > 0 ? 2 : 0) | (hat.y > 0 ? 4 : 0) | (hat.x < 0 ? 8 : 0);
        state.hats.push_back(static_cast<uint8_t>(mask));
    }
    return state;
}

//*******************************
// applyMapping
//*******************************
ControllerState applyMapping(const PadMapping &mapping, const RawPadState &raw) {
    ControllerState state;
    for (int i = 0; i < ElementCount; ++i) {
        Element element = static_cast<Element>(i);
        const Binding &binding = mapping[element];
        if (!binding.bound()) {
            continue;
        }
        bool trigger = element == Element::LeftTrigger || element == Element::RightTrigger;

        // what the binding reads, as a position in -32768..32767 (a button or a hat direction is one end)
        int position = 0;
        bool onAxis = false;
        switch (binding.kind) {
        case Binding::Kind::Button:
            position = raw.button(binding.index) ? 32767 : 0;
            break;
        case Binding::Kind::Hat:
            position = (raw.hat(binding.index) & binding.hatMask) != 0 ? 32767 : 0;
            break;
        case Binding::Kind::Axis:
            position = raw.axis(binding.index);
            onAxis = true;
            break;
        case Binding::Kind::None:
            break;
        }
        if (onAxis && binding.inverted) {
            position = clampInt(-position, -32768, 32767);
        }

        if (isAxis(element)) {
            int value;
            if (!onAxis) {
                value = position;
            } else if (binding.half > 0) {
                value = max(position, 0);
            } else if (binding.half < 0) {
                value = clampInt(-position, 0, 32767);
            } else if (trigger) {
                value = (position + 32768) / 2; // -32768..32767 of travel is 0..32767 of pull
            } else {
                value = position;
            }
            state.set(element, static_cast<int16_t>(clampInt(value, -32768, 32767)));
        } else {
            bool pressed;
            if (!onAxis) {
                pressed = position != 0;
            } else if (binding.half < 0) {
                pressed = position <= -AxisButtonThreshold;
            } else {
                pressed = position >= AxisButtonThreshold;
            }
            state.set(element, pressed);
        }
    }
    return state;
}

//*******************************
// uinputPlan
//*******************************
const UinputPlan &uinputPlan(VirtualPadKind kind) {
    static const UinputPlan psc = [] {
        UinputPlan plan;
        plan.name = "Playstation Classic Controller";
        plan.bus = 0x0003;
        plan.vendor = 0x054c;
        plan.product = 0x0cda;
        plan.version = 0x0111;
        for (int i = 0; i < 10; ++i) {
            plan.keys.push_back(0x130 + i); // BTN_A .. BTN_TR2
        }
        plan.abs.push_back({0x00, 0, 2, 0, 0}); // ABS_X
        plan.abs.push_back({0x01, 0, 2, 0, 0}); // ABS_Y
        return plan;
    }();
    static const UinputPlan x360 = [] {
        UinputPlan plan;
        plan.name = "Microsoft X-Box 360 pad";
        plan.bus = 0x0003;
        plan.vendor = 0x045e;
        plan.product = 0x028e;
        plan.version = 0x0110;
        // A B X Y LB RB Back Start Guide LS RS, as xpad reports them
        for (int code : {0x130, 0x131, 0x133, 0x134, 0x136, 0x137, 0x13a, 0x13b, 0x13c, 0x13d, 0x13e}) {
            plan.keys.push_back(code);
        }
        plan.abs.push_back({0x00, -32768, 32767, 16, 128}); // ABS_X
        plan.abs.push_back({0x01, -32768, 32767, 16, 128}); // ABS_Y
        plan.abs.push_back({0x02, 0, 255, 0, 0});           // ABS_Z, the left trigger
        plan.abs.push_back({0x03, -32768, 32767, 16, 128}); // ABS_RX
        plan.abs.push_back({0x04, -32768, 32767, 16, 128}); // ABS_RY
        plan.abs.push_back({0x05, 0, 255, 0, 0});           // ABS_RZ, the right trigger
        plan.abs.push_back({0x10, -1, 1, 0, 0});            // ABS_HAT0X
        plan.abs.push_back({0x11, -1, 1, 0, 0});            // ABS_HAT0Y
        return plan;
    }();
    return kind == VirtualPadKind::Psc ? psc : x360;
}

//*******************************
// evdevFrame
//*******************************
EvdevFrame evdevFrame(VirtualPadKind kind, const RawPadState &raw) {
    const UinputPlan &plan = uinputPlan(kind);
    EvdevFrame frame;
    for (size_t i = 0; i < plan.keys.size(); ++i) {
        frame.keys.emplace_back(plan.keys[i], raw.button(static_cast<int>(i)) ? 1 : 0);
    }
    if (kind == VirtualPadKind::Psc) {
        frame.abs.emplace_back(0x00, pscAxis(raw.axis(0)));
        frame.abs.emplace_back(0x01, pscAxis(raw.axis(1)));
        return frame;
    }
    for (int axis = 0; axis < 6; ++axis) {
        int value = raw.axis(axis);
        if (axis == 2 || axis == 5) {
            value = ((value + 32768) * 255) / 65535; // the triggers: 0..255 of pull
        }
        frame.abs.emplace_back(axis, value);
    }
    uint8_t hat = raw.hat(0);
    frame.abs.emplace_back(0x10, ((hat & 2) != 0 ? 1 : 0) - ((hat & 8) != 0 ? 1 : 0));
    frame.abs.emplace_back(0x11, ((hat & 4) != 0 ? 1 : 0) - ((hat & 1) != 0 ? 1 : 0));
    return frame;
}

} // namespace abpad
