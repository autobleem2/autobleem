#include "core/mapping.h"

#include <cstdlib>
#include <sstream>

using namespace std;

namespace abpad {

namespace {

// index by Element - the names a gamecontrollerdb.txt line uses
const char *const kElementNames[ElementCount] = {"a",
                                                 "b",
                                                 "x",
                                                 "y",
                                                 "back",
                                                 "guide",
                                                 "start",
                                                 "leftstick",
                                                 "rightstick",
                                                 "leftshoulder",
                                                 "rightshoulder",
                                                 "dpup",
                                                 "dpdown",
                                                 "dpleft",
                                                 "dpright",
                                                 "leftx",
                                                 "lefty",
                                                 "rightx",
                                                 "righty",
                                                 "lefttrigger",
                                                 "righttrigger"};

string trimmed(const string &text) {
    size_t first = text.find_first_not_of(" \t\r\n");
    if (first == string::npos) {
        return "";
    }
    size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

} // namespace

//*******************************
// isAxis / elementName / elementFromName
//*******************************
bool isAxis(Element element) {
    return static_cast<int>(element) >= FirstAxisElement && element != Element::Count;
}

const char *elementName(Element element) {
    int index = static_cast<int>(element);
    return (index >= 0 && index < ElementCount) ? kElementNames[index] : "";
}

Element elementFromName(const string &name) {
    for (int i = 0; i < ElementCount; ++i) {
        if (name == kElementNames[i]) {
            return static_cast<Element>(i);
        }
    }
    return Element::Count;
}

//*******************************
// Binding::parse
//*******************************
Binding Binding::parse(const string &raw) {
    Binding binding;
    string text = trimmed(raw);
    if (text.empty()) {
        return binding;
    }

    if (text.back() == '~') {
        binding.inverted = true;
        text.erase(text.size() - 1);
    }
    if (!text.empty() && (text[0] == '+' || text[0] == '-')) {
        binding.half = (text[0] == '+') ? 1 : -1;
        text.erase(0, 1);
    }
    if (text.size() < 2) {
        return Binding();
    }

    char kind = text[0];
    string rest = text.substr(1);
    if (kind == 'b') {
        binding.kind = Kind::Button;
        binding.index = atoi(rest.c_str());
    } else if (kind == 'a') {
        binding.kind = Kind::Axis;
        binding.index = atoi(rest.c_str());
    } else if (kind == 'h') {
        size_t dot = rest.find('.');
        if (dot == string::npos) {
            return Binding();
        }
        binding.kind = Kind::Hat;
        binding.index = atoi(rest.substr(0, dot).c_str());
        binding.hatMask = atoi(rest.substr(dot + 1).c_str());
    } else {
        return Binding();
    }
    if (binding.index < 0) {
        return Binding();
    }
    return binding;
}

//*******************************
// Binding::toString
//*******************************
string Binding::toString() const {
    if (!bound()) {
        return "";
    }
    ostringstream out;
    if (kind == Kind::Axis && half != 0) {
        out << (half > 0 ? '+' : '-');
    }
    if (kind == Kind::Button) {
        out << 'b' << index;
    } else if (kind == Kind::Axis) {
        out << 'a' << index;
    } else {
        out << 'h' << index << '.' << hatMask;
    }
    if (inverted) {
        out << '~';
    }
    return out.str();
}

//*******************************
// PadMapping::empty
//*******************************
bool PadMapping::empty() const {
    for (const Binding &binding : bindings) {
        if (binding.bound()) {
            return false;
        }
    }
    return true;
}

//*******************************
// PadMapping::parseLine
//*******************************
bool PadMapping::parseLine(const string &line, PadMapping &out) {
    string text = trimmed(line);
    if (text.empty() || text[0] == '#') {
        return false;
    }

    vector<string> fields;
    size_t start = 0;
    while (start <= text.size()) {
        size_t comma = text.find(',', start);
        if (comma == string::npos) {
            fields.push_back(text.substr(start));
            break;
        }
        fields.push_back(text.substr(start, comma - start));
        start = comma + 1;
    }
    if (fields.size() < 2) {
        return false;
    }

    PadMapping mapping;
    mapping.guid = trimmed(fields[0]);
    mapping.name = trimmed(fields[1]);
    if (mapping.guid.empty() || mapping.name.empty()) {
        return false;
    }

    for (size_t i = 2; i < fields.size(); ++i) {
        string field = trimmed(fields[i]);
        size_t colon = field.find(':');
        if (colon == string::npos) {
            continue;
        }
        string key = field.substr(0, colon);
        string value = field.substr(colon + 1);
        if (key == "platform") {
            mapping.platform = trimmed(value);
            continue;
        }
        Element element = elementFromName(key);
        if (element == Element::Count) {
            continue; // an element this build does not model (paddle1, touchpad, misc1, ...)
        }
        Binding binding = Binding::parse(value);
        if (binding.bound()) {
            mapping[element] = binding;
        }
    }

    out = mapping;
    return true;
}

//*******************************
// PadMapping::toLine
//*******************************
string PadMapping::toLine() const {
    ostringstream out;
    out << guid << ',' << name << ',';
    for (int i = 0; i < ElementCount; ++i) {
        const Binding &binding = bindings[i];
        if (binding.bound()) {
            out << kElementNames[i] << ':' << binding.toString() << ',';
        }
    }
    if (!platform.empty()) {
        out << "platform:" << platform << ',';
    }
    return out.str();
}

//*******************************
// RawPadState accessors
//*******************************
bool RawPadState::button(int index) const {
    return index >= 0 && index < static_cast<int>(buttons.size()) && buttons[index];
}

int16_t RawPadState::axis(int index) const {
    return (index >= 0 && index < static_cast<int>(axes.size())) ? axes[index] : 0;
}

uint8_t RawPadState::hat(int index) const {
    return (index >= 0 && index < static_cast<int>(hats.size())) ? hats[index] : 0;
}

//*******************************
// ControllerState accessors
//*******************************
bool ControllerState::button(Element element) const {
    int index = static_cast<int>(element);
    return (index >= 0 && index < FirstAxisElement) ? buttons[index] : false;
}

int16_t ControllerState::axis(Element element) const {
    int index = static_cast<int>(element) - FirstAxisElement;
    return (index >= 0 && index < ElementCount - FirstAxisElement) ? axes[index] : 0;
}

bool ControllerState::held(Element element) const {
    return isAxis(element) ? axis(element) >= AxisButtonThreshold : button(element);
}

void ControllerState::set(Element element, bool pressed) {
    int index = static_cast<int>(element);
    if (index >= 0 && index < FirstAxisElement) {
        buttons[index] = pressed;
    }
}

void ControllerState::set(Element element, int16_t value) {
    int index = static_cast<int>(element) - FirstAxisElement;
    if (index >= 0 && index < ElementCount - FirstAxisElement) {
        axes[index] = value;
    }
}

bool ControllerState::operator==(const ControllerState &other) const {
    for (int i = 0; i < FirstAxisElement; ++i) {
        if (buttons[i] != other.buttons[i]) {
            return false;
        }
    }
    for (int i = 0; i < ElementCount - FirstAxisElement; ++i) {
        if (axes[i] != other.axes[i]) {
            return false;
        }
    }
    return true;
}

//*******************************
// guessMapping
//*******************************
PadMapping guessMapping(const string &guid, const string &name, int buttonCount, int axisCount, int hatCount) {
    PadMapping mapping;
    mapping.guid = guid;
    mapping.name = name.empty() ? "Unmapped pad" : name;
    mapping.platform = "Linux";

    // the face buttons and the rest in the order the device reports them - no better guess exists
    static const Element order[] = {Element::A,
                                    Element::B,
                                    Element::X,
                                    Element::Y,
                                    Element::LeftShoulder,
                                    Element::RightShoulder,
                                    Element::Back,
                                    Element::Start,
                                    Element::Guide,
                                    Element::LeftStick,
                                    Element::RightStick};
    int count = static_cast<int>(sizeof(order) / sizeof(order[0]));
    for (int i = 0; i < buttonCount && i < count; ++i) {
        Binding binding;
        binding.kind = Binding::Kind::Button;
        binding.index = i;
        mapping[order[i]] = binding;
    }

    if (hatCount > 0) {
        static const Element dpad[] = {Element::DpUp, Element::DpRight, Element::DpDown, Element::DpLeft};
        static const int masks[] = {1, 2, 4, 8};
        for (int i = 0; i < 4; ++i) {
            Binding binding;
            binding.kind = Binding::Kind::Hat;
            binding.index = 0;
            binding.hatMask = masks[i];
            mapping[dpad[i]] = binding;
        }
    }

    // a pad with no more than two axes and no hat is steered by them: that is a d-pad on axes, the
    // shape of the PSC's own pad. More than two, and they are sticks.
    if (axisCount > 0 && axisCount <= 2 && hatCount == 0) {
        static const Element dpad[] = {Element::DpLeft, Element::DpRight, Element::DpUp, Element::DpDown};
        static const int halves[] = {-1, 1, -1, 1};
        for (int i = 0; i < 4 && i / 2 < axisCount; ++i) {
            Binding binding;
            binding.kind = Binding::Kind::Axis;
            binding.index = i / 2;
            binding.half = halves[i];
            mapping[dpad[i]] = binding;
        }
    } else {
        static const Element sticks[] = {Element::LeftX, Element::LeftY, Element::RightX, Element::RightY};
        for (int i = 0; i < axisCount && i < 4; ++i) {
            Binding binding;
            binding.kind = Binding::Kind::Axis;
            binding.index = i;
            mapping[sticks[i]] = binding;
        }
    }

    return mapping;
}

} // namespace abpad
