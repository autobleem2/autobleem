#include "core/key_names.h"

#include <cstring>

using namespace std;

namespace abpad {

namespace {

// SDL2 builds the keycode of a key that has no ASCII character from its scancode
constexpr int Sdl2ScancodeMask = 1 << 30;

struct KeyEntry {
    const char *name;
    int sdl1Sym;
    int sdl2Scancode;
    int sdl2Keycode; // 0: the key has no character, so SDL2's keycode is the masked scancode
};

// The keys a game is likely to want bound. Letters and digits are generated rather than listed.
const KeyEntry kKeys[] = {
    {"return", 13, 40, 13},     {"enter", 13, 40, 13},       {"escape", 27, 41, 27},       {"esc", 27, 41, 27},
    {"backspace", 8, 42, 8},    {"tab", 9, 43, 9},           {"space", 32, 44, 32},        {"minus", 45, 45, 45},
    {"equals", 61, 46, 61},     {"leftbracket", 91, 47, 91}, {"rightbracket", 93, 48, 93}, {"backslash", 92, 49, 92},
    {"semicolon", 59, 51, 59},  {"apostrophe", 39, 52, 39},  {"grave", 96, 53, 96},        {"comma", 44, 54, 44},
    {"period", 46, 55, 46},     {"slash", 47, 56, 47},

    {"right", 275, 79, 0},      {"left", 276, 80, 0},        {"down", 274, 81, 0},         {"up", 273, 82, 0},

    {"insert", 277, 73, 0},     {"home", 278, 74, 0},        {"pageup", 280, 75, 0},       {"delete", 127, 76, 127},
    {"end", 279, 77, 0},        {"pagedown", 281, 78, 0},

    {"left ctrl", 306, 224, 0}, {"lctrl", 306, 224, 0},      {"ctrl", 306, 224, 0},        {"left shift", 304, 225, 0},
    {"lshift", 304, 225, 0},    {"shift", 304, 225, 0},      {"left alt", 308, 226, 0},    {"lalt", 308, 226, 0},
    {"alt", 308, 226, 0},       {"right ctrl", 305, 228, 0}, {"rctrl", 305, 228, 0},       {"right shift", 303, 229, 0},
    {"rshift", 303, 229, 0},    {"right alt", 307, 230, 0},  {"ralt", 307, 230, 0},

    {"kp enter", 271, 88, 0},   {"kp0", 256, 98, 0},         {"kp1", 257, 89, 0},          {"kp2", 258, 90, 0},
    {"kp3", 259, 91, 0},        {"kp4", 260, 92, 0},         {"kp5", 261, 93, 0},          {"kp6", 262, 94, 0},
    {"kp7", 263, 95, 0},        {"kp8", 264, 96, 0},         {"kp9", 265, 97, 0},
};

// a name lowered, its spaces and underscores squeezed to a single space, so "Left Ctrl",
// "left_ctrl" and "LEFTCTRL" are one key
string normalized(const string &raw) {
    string text;
    bool lastWasSpace = false;
    for (char c : raw) {
        if (c == ' ' || c == '\t' || c == '_' || c == '-') {
            if (!text.empty() && !lastWasSpace) {
                text += ' ';
                lastWasSpace = true;
            }
            continue;
        }
        lastWasSpace = false;
        text += (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }
    while (!text.empty() && text.back() == ' ') {
        text.erase(text.size() - 1);
    }
    return text;
}

// the Linux input code (linux/input-event-codes.h) of an SDL2 scancode, 0 for one not in the table; this is the inverse
// of SDL2's own linux_scancode_table
int evdevFromSdl2Scancode(int scancode) {
    // KEY_A.. KEY_Z are in keyboard-row order, not alphabetical
    static const int letters[26] = {30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38, 50,
                                    49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44};
    static const int keypad[10] = {82, 79, 80, 81, 75, 76, 77, 71, 72, 73}; // KP0 .. KP9
    if (scancode >= 4 && scancode <= 29) {
        return letters[scancode - 4];
    }
    if (scancode >= 30 && scancode <= 38) {
        return 2 + (scancode - 30); // KEY_1 .. KEY_9
    }
    if (scancode == 39) {
        return 11; // KEY_0
    }
    if (scancode >= 58 && scancode <= 67) {
        return 59 + (scancode - 58); // KEY_F1 .. KEY_F10
    }
    if (scancode >= 89 && scancode <= 98) {
        return keypad[(scancode - 88) % 10];
    }
    switch (scancode) {
    case 40: return 28;  // Return
    case 41: return 1;   // Escape
    case 42: return 14;  // Backspace
    case 43: return 15;  // Tab
    case 44: return 57;  // Space
    case 45: return 12;  // minus
    case 46: return 13;  // equals
    case 47: return 26;  // left bracket
    case 48: return 27;  // right bracket
    case 49: return 43;  // backslash
    case 51: return 39;  // semicolon
    case 52: return 40;  // apostrophe
    case 53: return 41;  // grave
    case 54: return 51;  // comma
    case 55: return 52;  // period
    case 56: return 53;  // slash
    case 68: return 87;  // F11
    case 69: return 88;  // F12
    case 73: return 110; // Insert
    case 74: return 102; // Home
    case 75: return 104; // Page up
    case 76: return 111; // Delete
    case 77: return 107; // End
    case 78: return 109; // Page down
    case 79: return 106; // Right
    case 80: return 105; // Left
    case 81: return 108; // Down
    case 82: return 103; // Up
    case 88: return 96;  // keypad Enter
    case 224: return 29; // left Ctrl
    case 225: return 42; // left Shift
    case 226: return 56; // left Alt
    case 228: return 97; // right Ctrl
    case 229: return 54; // right Shift
    case 230: return 100; // right Alt
    }
    return 0;
}

KeyCode made(int sdl1Sym, int sdl2Scancode, int sdl2Keycode) {
    KeyCode key;
    key.evdev = evdevFromSdl2Scancode(sdl2Scancode);
    key.sdl1Sym = sdl1Sym;
    key.sdl2Scancode = sdl2Scancode;
    key.sdl2Keycode = sdl2Keycode ? sdl2Keycode : (sdl2Scancode | Sdl2ScancodeMask);
    key.known = true;
    return key;
}

} // namespace

//*******************************
// keyCodeFromName
//*******************************
KeyCode keyCodeFromName(const string &name) {
    string text = normalized(name);
    if (text.empty()) {
        return KeyCode();
    }

    // a single letter or digit: both SDLs use the character itself as the keysym/keycode, and SDL2's
    // scancodes run a..z from 4 and 1..9 from 30 with 0 after them
    if (text.size() == 1) {
        char c = text[0];
        if (c >= 'a' && c <= 'z') {
            return made(c, 4 + (c - 'a'), c);
        }
        if (c >= '1' && c <= '9') {
            return made(c, 30 + (c - '1'), c);
        }
        if (c == '0') {
            return made('0', 39, '0');
        }
    }

    // f1..f12
    if (text.size() >= 2 && text[0] == 'f') {
        int number = 0;
        bool digits = true;
        for (size_t i = 1; i < text.size(); ++i) {
            if (text[i] < '0' || text[i] > '9') {
                digits = false;
                break;
            }
            number = number * 10 + (text[i] - '0');
        }
        if (digits && number >= 1 && number <= 12) {
            return made(281 + number, 57 + number, 0);
        }
    }

    for (const KeyEntry &entry : kKeys) {
        if (text == entry.name) {
            return made(entry.sdl1Sym, entry.sdl2Scancode, entry.sdl2Keycode);
        }
    }
    return KeyCode();
}

} // namespace abpad
