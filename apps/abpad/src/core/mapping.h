#ifndef ABPAD_MAPPING_H
#define ABPAD_MAPPING_H

// The vocabulary the whole of abpad speaks: SDL's game controller model.
//
// The daemon reads every pad through SDL2's own GameController API with our gamecontrollerdb.txt, so
// nothing here parses a device or resolves a mapping - SDL does both, which is what makes a pad
// resolve in an App exactly as it resolves in the launcher. What is left is the shape of the answer
// (ControllerState), the shape of the question an app asks its SDL (RawPadState: buttons, axes and
// hats), and enough of a gamecontrollerdb line to *describe* a pad - because a virtual layout is
// written as one (see virtual_pad.h) and because an unmapped pad is given one (guessMapping).

#include <cstdint>
#include <string>
#include <vector>

namespace abpad {

// SDL's game controller elements, buttons first then axes - the order matters, isAxis() splits on it
enum class Element {
    A,
    B,
    X,
    Y,
    Back,
    Guide,
    Start,
    LeftStick,
    RightStick,
    LeftShoulder,
    RightShoulder,
    DpUp,
    DpDown,
    DpLeft,
    DpRight,
    LeftX,
    LeftY,
    RightX,
    RightY,
    LeftTrigger,
    RightTrigger,
    Count
};

constexpr int ElementCount = static_cast<int>(Element::Count);
constexpr int FirstAxisElement = static_cast<int>(Element::LeftX);
constexpr int ButtonElementCount = FirstAxisElement;
constexpr int AxisElementCount = ElementCount - FirstAxisElement;

bool isAxis(Element element);
const char *elementName(Element element);         // the name a mapping line uses ("leftshoulder")
Element elementFromName(const std::string &name); // Element::Count when it is not one of ours

// an axis is taken for a pressed button past this much of its travel, as SDL does
constexpr int16_t AxisButtonThreshold = 16384;

//*******************************
// Binding - what one element is wired to on some pad ("b3", "-a1", "h0.4", "a2~")
//*******************************
struct Binding {
    enum class Kind { None, Button, Axis, Hat };

    Kind kind = Kind::None;
    int index = 0;         // the button, axis or hat number
    int hatMask = 0;       // Hat: 1 up, 2 right, 4 down, 8 left
    int half = 0;          // Axis: 0 the whole travel, +1 the positive half, -1 the negative half
    bool inverted = false; // Axis: the trailing '~'

    bool bound() const { return kind != Kind::None; }
    static Binding parse(const std::string &text); // an unparseable binding comes back unbound
    std::string toString() const;
};

//*******************************
// PadMapping - one gamecontrollerdb.txt line
//*******************************
struct PadMapping {
    std::string guid;
    std::string name;
    std::string platform;
    Binding bindings[ElementCount];

    const Binding &operator[](Element element) const { return bindings[static_cast<int>(element)]; }
    Binding &operator[](Element element) { return bindings[static_cast<int>(element)]; }

    bool empty() const;
    // "guid,name,a:b0,...,platform:Linux," - what SDL_GameControllerAddMapping takes
    std::string toLine() const;
    // false for a comment, a blank line or a line without a guid and a name
    static bool parseLine(const std::string &line, PadMapping &out);
};

//*******************************
// RawPadState - a pad as an app's SDL describes one: what the shim answers with
//*******************************
struct RawPadState {
    std::vector<bool> buttons;
    std::vector<int16_t> axes;
    std::vector<uint8_t> hats; // 1 up, 2 right, 4 down, 8 left

    bool button(int index) const;
    int16_t axis(int index) const;
    uint8_t hat(int index) const;
};

//*******************************
// ControllerState - a pad as SDL's GameController API describes one: what the daemon publishes
//*******************************
struct ControllerState {
    bool buttons[ButtonElementCount] = {};
    int16_t axes[AxisElementCount] = {};

    bool button(Element element) const;
    int16_t axis(Element element) const;
    // true for a button, and for an axis pulled past the threshold - what a hotkey and keyboard mode
    // both want to ask
    bool held(Element element) const;
    void set(Element element, bool pressed);
    void set(Element element, int16_t value);
    bool operator==(const ControllerState &other) const;
    bool operator!=(const ControllerState &other) const { return !(*this == other); }
};

// A mapping line for a pad no gamecontrollerdb.txt knows, so that an unknown pad is playable rather
// than invisible: SDL only offers a pad as a GameController when it has a mapping for it, so the
// daemon makes one up and hands it to SDL_GameControllerAddMapping. The buttons in the order the
// device reports them onto A, B, X, Y, the shoulders and Back/Start; hat 0 as the d-pad, or axes 0/1
// when there is no hat and no more than two axes - which is the shape of the PSC's own pad.
PadMapping guessMapping(const std::string &guid, const std::string &name, int buttonCount, int axisCount, int hatCount);

} // namespace abpad

#endif
