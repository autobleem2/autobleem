#ifndef ABPAD_MAPPING_H
#define ABPAD_MAPPING_H

// A gamecontrollerdb.txt mapping, and what it turns a physical pad's raw state into.
//
// The vocabulary is SDL's game controller model - the same twenty-one elements a mapping line names
// - because that is the one description of a pad both ends of the shim can agree on: the physical
// pad is read *through* a mapping line, the virtual pad is written *through* another one (a virtual
// layout is itself just a mapping line, see virtual_pad.h). Nothing here touches a file descriptor.

#include <cstdint>
#include <iosfwd>
#include <map>
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
    // "guid,name,a:b0,...,platform:Linux," - what SDL would accept back
    std::string toLine() const;
    // false for a comment, a blank line or a line without a guid and a name
    static bool parseLine(const std::string &line, PadMapping &out);
};

//*******************************
// PhysicalState - a pad as the device reports it, in SDL's numbering (see evdev_order.h)
//*******************************
struct PhysicalState {
    std::vector<bool> buttons;
    std::vector<int16_t> axes;
    std::vector<uint8_t> hats; // 1 up, 2 right, 4 down, 8 left

    bool button(int index) const;
    int16_t axis(int index) const;
    uint8_t hat(int index) const;
};

//*******************************
// ControllerState - the pad as the mapping describes it: what both ends of the shim speak
//*******************************
struct ControllerState {
    bool buttons[FirstAxisElement] = {};
    int16_t axes[ElementCount - FirstAxisElement] = {};

    bool button(Element element) const;
    int16_t axis(Element element) const;
    void set(Element element, bool pressed);
    void set(Element element, int16_t value);
    bool operator==(const ControllerState &other) const;
    bool operator!=(const ControllerState &other) const { return !(*this == other); }
};

// read a physical pad through its mapping
ControllerState applyMapping(const PadMapping &mapping, const PhysicalState &state);

//*******************************
// MappingDb - gamecontrollerdb.txt, looked up by GUID
//*******************************
class MappingDb {
public:
    void loadStream(std::istream &in); // merges; a later line for the same GUID wins, as SDL does
    bool loadFile(const std::string &path);
    // the platform is not checked: the file the launcher loads is the one the console runs, and the
    // pscbios wizard writes its line with platform:Linux
    bool find(const std::string &guid, PadMapping &out) const;
    size_t size() const { return byGuid_.size(); }

private:
    std::map<std::string, PadMapping> byGuid_;
};

// what an unknown pad is read as, so that a pad nobody mapped is still playable: the buttons in the
// order the device reports them onto A, B, X, Y, the shoulders and Back/Start, the first two axes as
// the left stick, the next two as the right, and hat 0 as the d-pad (or axes 0/1 when there is no hat
// and no more than two axes - which is the shape of a d-pad-only pad like the PSC's).
PadMapping guessMapping(int buttonCount, int axisCount, int hatCount);

} // namespace abpad

#endif
