//
// PadMapping: the wizard's logic.
//
#include "pad_mapping.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

using namespace std;
using ableem::JoystickState;

//*******************************
// PadMapping::standardElements
//*******************************
vector<PadMapping::Element> PadMapping::standardElements() {
    auto digital = [](const char *name, int button) {
        Element e;
        e.apiName = name;
        e.scan = Scan::Digital;
        e.button = button;
        return e;
    };
    auto trigger = [](const char *name, int axis) {
        Element e;
        e.apiName = name;
        e.scan = Scan::Trigger;
        e.axis = axis;
        return e;
    };
    auto analog = [](const char *name, int axis, bool negative) {
        Element e;
        e.apiName = name;
        e.scan = Scan::Analog;
        e.axis = axis;
        e.negative = negative;
        return e;
    };
    // the button numbers are SDL_GameControllerButton's, the axes SDL_GameControllerAxis's
    return {digital("a", 0),
            digital("b", 1),
            digital("x", 2),
            digital("y", 3),
            digital("back", 4),
            digital("guide", 5),
            digital("start", 6),
            digital("leftstick", 7),
            digital("rightstick", 8),
            digital("leftshoulder", 9),
            digital("rightshoulder", 10),
            trigger("lefttrigger", 4),
            trigger("righttrigger", 5),
            digital("dpup", 11),
            digital("dpdown", 12),
            digital("dpleft", 13),
            digital("dpright", 14),
            analog("-leftx", 0, true),
            analog("+leftx", 0, false),
            analog("-lefty", 1, true),
            analog("+lefty", 1, false),
            analog("-rightx", 2, true),
            analog("+rightx", 2, false),
            analog("-righty", 3, true),
            analog("+righty", 3, false)};
}

//*******************************
// PadMapping::detectChange
//*******************************
string PadMapping::detectChange(const JoystickState &initial, const JoystickState &now, const vector<Element> &taken) {
    string result;
    size_t buttons = min(initial.buttons.size(), now.buttons.size());
    for (size_t i = 0; i < buttons && result.empty(); i++)
        if (now.buttons[i] != initial.buttons[i])
            result = "b" + to_string(i);
    // a hat is reported by where it points now, one direction at a time (a diagonal is not a mapping)
    for (size_t i = 0; i < now.hats.size() && result.empty(); i++) {
        unsigned hat = now.hats[i];
        if (hat == ableem::Joystick::HatUp || hat == ableem::Joystick::HatDown || hat == ableem::Joystick::HatLeft ||
            hat == ableem::Joystick::HatRight)
            result = "h" + to_string(i) + "." + to_string(hat);
    }
    size_t axes = min(initial.axes.size(), now.axes.size());
    for (size_t i = 0; i < axes && result.empty(); i++) {
        int delta = now.axes[i] - initial.axes[i];
        if (abs(delta) <= AxisThreshold)
            continue;
        if (abs(initial.axes[i]) < RestTolerance) // rested in the middle: a stick, one half of it
            result = string(delta > 0 ? "+" : "-") + "a" + to_string(i);
        else // rested at one end: a trigger, the whole axis
            result = "a" + to_string(i);
    }
    if (result.empty())
        return "";
    for (const Element &element : taken)
        if (element.value == result)
            return "";
    return result;
}

//*******************************
// PadMapping::anythingHeld
//*******************************
bool PadMapping::anythingHeld(const JoystickState &initial, const JoystickState &now) {
    size_t buttons = min(initial.buttons.size(), now.buttons.size());
    for (size_t i = 0; i < buttons; i++)
        if (now.buttons[i] != initial.buttons[i])
            return true;
    size_t hats = min(initial.hats.size(), now.hats.size());
    for (size_t i = 0; i < hats; i++)
        if (now.hats[i] != initial.hats[i])
            return true;
    size_t axes = min(initial.axes.size(), now.axes.size());
    for (size_t i = 0; i < axes; i++)
        if (abs(now.axes[i] - initial.axes[i]) > HeldThreshold)
            return true;
    return false;
}

//*******************************
// PadMapping::mergeAxis
//*******************************
// the "-x" and "+x" halves of a stick axis: both mapped to the two halves of the same raw axis become one
// "a<n>" element (inverted, "~", when the pad's + is our -); a half without its partner drops both (a
// stick that only goes one way is no stick); anything else - halves on different axes, a half on a button -
// is kept as the two half-axis keys SDL also understands ("-leftx:-a0,+leftx:b5")
void PadMapping::mergeAxis(vector<Element> &elements, const string &apiName, size_t minus, size_t plus) {
    string m = elements[minus].value, p = elements[plus].value;
    if (m.empty() || p.empty()) {
        elements[minus].value.clear();
        elements[plus].value.clear();
        return;
    }
    bool halves = m.size() >= 3 && p.size() >= 3 && (m[0] == '+' || m[0] == '-') && (p[0] == '+' || p[0] == '-') &&
                  m[1] == 'a' && p[1] == 'a';
    if (!halves || m.substr(2) != p.substr(2))
        return;
    Element merged;
    merged.apiName = apiName;
    merged.scan = Scan::Analog;
    merged.value = "a" + m.substr(2);
    if (p[0] == '-' && m[0] == '+')
        merged.value += "~";
    elements[minus].value.clear();
    elements[plus].value.clear();
    elements.push_back(merged);
}

//*******************************
// PadMapping::finalElements
//*******************************
vector<PadMapping::Element> PadMapping::finalElements(const vector<Element> &scanned, const string &platform) {
    vector<Element> elements = scanned;
    if (elements.size() >= 25) {
        mergeAxis(elements, "leftx", 17, 18);
        mergeAxis(elements, "lefty", 19, 20);
        mergeAxis(elements, "rightx", 21, 22);
        mergeAxis(elements, "righty", 23, 24);
    }
    vector<Element> finals;
    for (const Element &element : elements)
        if (!element.value.empty())
            finals.push_back(element);
    Element platformElement;
    platformElement.apiName = "platform";
    platformElement.value = platform;
    finals.push_back(platformElement);
    return finals;
}

//*******************************
// PadMapping::mappingLine / cleanName
//*******************************
string PadMapping::mappingLine(const string &guid, const string &name, const vector<Element> &finals) {
    string line = guid + "," + name + ",";
    for (const Element &element : finals)
        line += element.apiName + ":" + element.value + ",";
    return line;
}

string PadMapping::cleanName(const string &name) {
    string clean;
    for (char c : name)
        if (isalnum(static_cast<unsigned char>(c)) || isspace(static_cast<unsigned char>(c)))
            clean += c;
    return clean;
}
