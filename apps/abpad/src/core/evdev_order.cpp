#include "core/evdev_order.h"

#include "core/evdev_codes.h"

#include <algorithm>
#include <cstdio>

using namespace std;

namespace abpad {

//*******************************
// SdlJoystickLayout::buttonForCode
//*******************************
int SdlJoystickLayout::buttonForCode(int code) const {
    for (size_t i = 0; i < buttons.size(); ++i) {
        if (buttons[i] == code) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

//*******************************
// SdlJoystickLayout::axisForCode
//*******************************
int SdlJoystickLayout::axisForCode(int code) const {
    for (size_t i = 0; i < axes.size(); ++i) {
        if (axes[i] == code) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

//*******************************
// SdlJoystickLayout::hatForCode
//*******************************
pair<int, int> SdlJoystickLayout::hatForCode(int code) const {
    for (size_t i = 0; i < hats.size(); ++i) {
        if (hats[i].first == code) {
            return {static_cast<int>(i), 0};
        }
        if (hats[i].second == code) {
            return {static_cast<int>(i), 1};
        }
    }
    return {-1, -1};
}

//*******************************
// enumerateLikeSdl
//*******************************
SdlJoystickLayout enumerateLikeSdl(const EvdevCaps &caps) {
    SdlJoystickLayout layout;

    // the joystick/gamepad codes come first, then the BTN_MISC range and anything below it: that is
    // the order SDL walks, and it is why a pad reporting BTN_TRIGGER gets button 0 there
    for (int code : caps.keys) {
        if (code >= BTN_JOYSTICK_ && code <= KEY_MAX_) {
            layout.buttons.push_back(code);
        }
    }
    for (int code : caps.keys) {
        if (code >= 0 && code < BTN_JOYSTICK_) {
            layout.buttons.push_back(code);
        }
    }

    for (int code : caps.axes) {
        if (code >= ABS_HAT0X_ && code <= ABS_HAT3Y_) {
            continue; // the hats are counted separately, below
        }
        if (code >= 0 && code < ABS_MAX_) {
            layout.axes.push_back(code);
        }
    }

    for (int code = ABS_HAT0X_; code <= ABS_HAT3Y_; code += 2) {
        bool hasX = find(caps.axes.begin(), caps.axes.end(), code) != caps.axes.end();
        bool hasY = find(caps.axes.begin(), caps.axes.end(), code + 1) != caps.axes.end();
        if (hasX || hasY) {
            layout.hats.emplace_back(code, code + 1);
        }
    }

    return layout;
}

//*******************************
// guidFromIds
//*******************************
string guidFromIds(uint16_t bus, uint16_t vendor, uint16_t product, uint16_t version) {
    // SDL writes the 16 GUID bytes little-endian and prints them byte by byte, so each id lands as a
    // byte-swapped pair followed by a zero pair
    char text[33];
    snprintf(text, sizeof(text), "%02x%02x0000%02x%02x0000%02x%02x0000%02x%02x0000", bus & 0xff, (bus >> 8) & 0xff,
             vendor & 0xff, (vendor >> 8) & 0xff, product & 0xff, (product >> 8) & 0xff, version & 0xff,
             (version >> 8) & 0xff);
    return string(text);
}

} // namespace abpad
