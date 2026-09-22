#ifndef ABPAD_EVDEV_ORDER_H
#define ABPAD_EVDEV_ORDER_H

// SDL's own numbering of an evdev device, reproduced.
//
// A gamecontrollerdb.txt line is written in SDL's numbering: "b3" is the fourth button *SDL* counted
// for that device, not an evdev key code. The shim reads evdev itself, so to read a mapping line it
// has to count the way SDL's Linux joystick driver counts (SDL_sysjoystick.c, ConfigJoystick):
//
//   buttons: the EV_KEY codes the device reports, BTN_JOYSTICK..KEY_MAX first, then 0..BTN_JOYSTICK-1
//   axes:    the EV_ABS codes in code order, with ABS_HAT0X..ABS_HAT3Y skipped
//   hats:    ABS_HAT0X..ABS_HAT3Y in pairs, one hat per pair where either half is reported
//
// Stable across SDL 2.0.x; the console has 2.0.12. Pure logic - no device, no I/O, so it is tested.

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace abpad {

// what a device reports, as read from its EVIOCGBIT bitmaps (sorted, ascending)
struct EvdevCaps {
    std::vector<int> keys; // EV_KEY codes
    std::vector<int> axes; // EV_ABS codes, hats included
};

// the same device as SDL numbers it
struct SdlJoystickLayout {
    std::vector<int> buttons;              // SDL button index -> EV_KEY code
    std::vector<int> axes;                 // SDL axis index -> EV_ABS code
    std::vector<std::pair<int, int>> hats; // SDL hat index -> (X code, Y code)

    int buttonForCode(int code) const; // SDL index, -1 when the device does not report it
    int axisForCode(int code) const;
    // SDL hat index and which half moved: 0 = X, 1 = Y; {-1, -1} when it is not a hat axis
    std::pair<int, int> hatForCode(int code) const;
};

SdlJoystickLayout enumerateLikeSdl(const EvdevCaps &caps);

// the device's SDL GUID from EVIOCGID: bus, vendor, product and version as little-endian 16-bit
// values, each followed by a zero pair - "030000004c050000da0c000011010000" is bus 0003, vendor 054c,
// product 0cda, version 0111 (the PSC pad). A device with no vendor/product is identified by name
// instead, which SDL does by hashing it into the same 16 bytes; that form is not produced here.
std::string guidFromIds(uint16_t bus, uint16_t vendor, uint16_t product, uint16_t version);

} // namespace abpad

#endif
