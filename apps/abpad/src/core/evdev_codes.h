#ifndef ABPAD_EVDEV_CODES_H
#define ABPAD_EVDEV_CODES_H

// The handful of Linux input codes the enumeration needs, spelled out here rather than included from
// <linux/input.h>: the core is built on the dev host too (the tests), where that header does not exist.
// These are kernel ABI - they cannot change.

namespace abpad {

enum EvdevCode {
    BTN_MISC_ = 0x100,
    BTN_JOYSTICK_ = 0x120,
    BTN_TRIGGER_ = 0x120,
    BTN_THUMB_ = 0x121,
    BTN_THUMB2_ = 0x122,
    BTN_TOP_ = 0x123,
    BTN_TOP2_ = 0x124,
    BTN_PINKIE_ = 0x125,
    BTN_BASE_ = 0x126,
    BTN_BASE2_ = 0x127,
    BTN_BASE3_ = 0x128,
    BTN_BASE4_ = 0x129,
    BTN_BASE5_ = 0x12a,
    BTN_BASE6_ = 0x12b,
    BTN_DEAD_ = 0x12f,
    BTN_GAMEPAD_ = 0x130,
    BTN_SOUTH_ = 0x130,
    BTN_EAST_ = 0x131,
    BTN_C_ = 0x132,
    BTN_NORTH_ = 0x133,
    BTN_WEST_ = 0x134,
    BTN_Z_ = 0x135,
    BTN_TL_ = 0x136,
    BTN_TR_ = 0x137,
    BTN_TL2_ = 0x138,
    BTN_TR2_ = 0x139,
    BTN_SELECT_ = 0x13a,
    BTN_START_ = 0x13b,
    BTN_MODE_ = 0x13c,
    BTN_THUMBL_ = 0x13d,
    BTN_THUMBR_ = 0x13e,
    BTN_DPAD_UP_ = 0x220,
    BTN_DPAD_DOWN_ = 0x221,
    BTN_DPAD_LEFT_ = 0x222,
    BTN_DPAD_RIGHT_ = 0x223,
    KEY_MAX_ = 0x2ff,

    ABS_X_ = 0x00,
    ABS_Y_ = 0x01,
    ABS_Z_ = 0x02,
    ABS_RX_ = 0x03,
    ABS_RY_ = 0x04,
    ABS_RZ_ = 0x05,
    ABS_HAT0X_ = 0x10,
    ABS_HAT0Y_ = 0x11,
    ABS_HAT3Y_ = 0x17,
    ABS_MAX_ = 0x3f,
};

} // namespace abpad

#endif
