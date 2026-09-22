#include "core/profile.h"

#include "core/shared_state.h"

#include <cstdlib>
#include <fstream>
#include <istream>
#include <string>

using namespace std;

namespace abpad {

namespace {

string trimmed(const string &text) {
    size_t first = text.find_first_not_of(" \t\r\n");
    if (first == string::npos) {
        return "";
    }
    size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

string lowered(const string &text) {
    string result = text;
    for (char &c : result) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return result;
}

// "start", "back"/"select", "a"/"cross", ... - the element names a profile may use. The pad's own
// names are accepted next to SDL's because a reader of the file is thinking about a PlayStation pad.
Element elementFromProfileName(const string &raw) {
    string name = lowered(trimmed(raw));
    if (name == "select") {
        return Element::Back;
    }
    if (name == "cross") {
        return Element::A;
    }
    if (name == "circle") {
        return Element::B;
    }
    if (name == "square") {
        return Element::X;
    }
    if (name == "triangle") {
        return Element::Y;
    }
    if (name == "l1") {
        return Element::LeftShoulder;
    }
    if (name == "r1") {
        return Element::RightShoulder;
    }
    if (name == "l2") {
        return Element::LeftTrigger;
    }
    if (name == "r2") {
        return Element::RightTrigger;
    }
    if (name == "l3") {
        return Element::LeftStick;
    }
    if (name == "r3") {
        return Element::RightStick;
    }
    return elementFromName(name);
}

} // namespace

//*******************************
// padModeFromName / padModeName
//*******************************
PadMode padModeFromName(const string &name) {
    string text = lowered(trimmed(name));
    if (text == "off" || text == "none") {
        return PadMode::Off;
    }
    if (text == "keyboard" || text == "keys") {
        return PadMode::Keyboard;
    }
    if (text == "both") {
        return PadMode::Both;
    }
    return PadMode::Joystick;
}

const char *padModeName(PadMode mode) {
    switch (mode) {
    case PadMode::Off:
        return "off";
    case PadMode::Keyboard:
        return "keyboard";
    case PadMode::Both:
        return "both";
    case PadMode::Joystick:
        break;
    }
    return "joystick";
}

//*******************************
// parseHotkey
//*******************************
vector<Element> parseHotkey(const string &text) {
    vector<Element> elements;
    size_t start = 0;
    while (start <= text.size()) {
        size_t plus = text.find('+', start);
        string part = (plus == string::npos) ? text.substr(start) : text.substr(start, plus - start);
        Element element = elementFromProfileName(part);
        if (element != Element::Count) {
            elements.push_back(element);
        }
        if (plus == string::npos) {
            break;
        }
        start = plus + 1;
    }
    return elements;
}

//*******************************
// hotkeyHeld
//*******************************
bool hotkeyHeld(const vector<Element> &hotkey, const ControllerState &state) {
    if (hotkey.empty()) {
        return false;
    }
    for (Element element : hotkey) {
        if (!state.held(element)) {
            return false;
        }
    }
    return true;
}

//*******************************
// Profile::keyFor
//*******************************
string Profile::keyFor(Element element) const {
    map<Element, string>::const_iterator found = keys.find(element);
    return (found == keys.end()) ? string() : found->second;
}

//*******************************
// Profile::loadStream
//*******************************
void Profile::loadStream(istream &in) {
    string line;
    while (getline(in, line)) {
        string text = trimmed(line);
        if (text.empty() || text[0] == '#' || text[0] == ';') {
            continue;
        }
        size_t comment = text.find('#');
        if (comment != string::npos) {
            text = trimmed(text.substr(0, comment));
        }
        size_t equals = text.find('=');
        if (equals == string::npos) {
            continue;
        }
        string key = lowered(trimmed(text.substr(0, equals)));
        string value = trimmed(text.substr(equals + 1));

        if (key == "mode") {
            mode = padModeFromName(value);
        } else if (key == "virtual" || key == "pad") {
            virtualPad = virtualPadKindFromName(lowered(value));
        } else if (key == "players") {
            int count = atoi(value.c_str());
            players = (count < 1) ? 1 : (count > MaxPads ? MaxPads : count);
        } else if (key == "hotkey") {
            hotkey = parseHotkey(lowered(value));
        } else if (key == "movement" || key == "dpad") {
            movement = movementAidFromName(lowered(value));
        } else if (key == "cursor") {
            string want = lowered(value);
            hideCursor = !(want == "keep" || want == "show" || want == "visible" || want == "on");
        } else if (key == "log") {
            logPath = value;
        } else if (key.compare(0, 4, "key.") == 0) {
            Element element = elementFromProfileName(key.substr(4));
            if (element != Element::Count) {
                if (value.empty()) {
                    keys.erase(element);
                } else {
                    keys[element] = value;
                }
            }
        }
    }
}

//*******************************
// Profile::loadFile
//*******************************
bool Profile::loadFile(const string &path) {
    ifstream in(path.c_str());
    if (!in.is_open()) {
        return false;
    }
    loadStream(in);
    return true;
}

} // namespace abpad
