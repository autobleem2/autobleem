#ifndef ABPAD_KERNEL_PAD_H
#define ABPAD_KERNEL_PAD_H

// The kernel pad: a pad made at the evdev level, for an App no preload can reach (a statically linked SDL, a program
// that opens /dev/input/event* itself).
//
// Three pure pieces live here - the daemon (daemon/kernel_pad_linux.cpp) only does the ioctls and the reads:
//   * EvdevPadState + applyMapping(): a real pad read from its evdev node, numbered the way SDL's own evdev driver
//     numbers it (so the gamecontrollerdb line SDL resolved for the pad reads it), turned into a ControllerState.
//   * uinputPlan(): what the virtual device has to be - name, ids, keys, axes - so that it is the same pad the
//     shim shows (the same gamecontrollerdb line, read the other way).
//   * evdevFrame(): the values to put on that device for a raw pad state.
// No linux headers here: the codes are the kernel's own numbers, spelled out, so this builds and is tested anywhere.

#include "core/mapping.h"
#include "core/virtual_pad.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace abpad {

// the kernel's numbers (linux/input-event-codes.h)
constexpr int EvdevBtnMisc = 0x100;
constexpr int EvdevBtnJoystick = 0x120;
constexpr int EvdevKeyMax = 0x2ff;
constexpr int EvdevAbsMax = 0x3f;
constexpr int EvdevAbsHat0X = 0x10;
constexpr int EvdevAbsHat3Y = 0x17;

struct EvdevAbs {
    int code = 0;
    int min = 0;
    int max = 0;
};

//*******************************
// EvdevPadState - one device's keys and axes, numbered as SDL's evdev joystick driver does
//*******************************
// Buttons: the codes from BTN_JOYSTICK up, then BTN_MISC..BTN_JOYSTICK, in code order. Axes: every ABS code in
// order except the hats' (ABS_HAT0X..ABS_HAT3Y). Hats: a pair of hat codes each, in order.
class EvdevPadState {
public:
    EvdevPadState(const std::vector<int> &keyCodes, const std::vector<EvdevAbs> &absCodes);

    void setKey(int code, int value);
    void setAbs(int code, int value);

    int buttonCount() const { return static_cast<int>(buttonCodes_.size()); }
    int axisCount() const { return static_cast<int>(axes_.size()); }
    int hatCount() const { return static_cast<int>(hats_.size()); }

    // the pad as SDL's joystick API would give it: axes spread over -32768..32767
    RawPadState raw() const;

private:
    struct Axis {
        EvdevAbs info;
        int value = 0;
    };
    struct Hat {
        int xCode = 0;
        int x = 0;
        int y = 0;
    };

    std::vector<int> buttonCodes_;
    std::vector<bool> buttons_;
    std::vector<Axis> axes_;
    std::vector<Hat> hats_;
};

// a raw joystick state read through a gamecontrollerdb line - what SDL's GameController API does
ControllerState applyMapping(const PadMapping &mapping, const RawPadState &raw);

//*******************************
// The virtual device
//*******************************
struct UinputAxis {
    int code = 0;
    int min = 0;
    int max = 0;
    int fuzz = 0;
    int flat = 0;
};

struct UinputPlan {
    std::string name;
    uint16_t bus = 0;
    uint16_t vendor = 0;
    uint16_t product = 0;
    uint16_t version = 0;
    std::vector<int> keys;       // the key codes, in the order of the raw pad's buttons
    std::vector<UinputAxis> abs; // the axes, the hat's two last for a layout that has one
};

const UinputPlan &uinputPlan(VirtualPadKind kind);

// every value the device has for a raw pad state, whole - the daemon sends what changed
struct EvdevFrame {
    std::vector<std::pair<int, int>> keys; // code, 0/1
    std::vector<std::pair<int, int>> abs;  // code, value
};
EvdevFrame evdevFrame(VirtualPadKind kind, const RawPadState &raw);

} // namespace abpad

#endif
