#include "core/hold.h"

#include "core/profile.h"

using namespace std;

namespace abpad {

const char *const DefaultExitHotkeys = "start+select,guide";

//*******************************
// parseHotkeySet
//*******************************
HotkeySet parseHotkeySet(const string &text) {
    HotkeySet set;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find_first_of(",|", start);
        string part = (end == string::npos) ? text.substr(start) : text.substr(start, end - start);
        Hotkey hotkey = parseHotkey(part);
        if (!hotkey.empty()) {
            set.push_back(hotkey);
        }
        if (end == string::npos) {
            break;
        }
        start = end + 1;
    }
    return set;
}

//*******************************
// hotkeySetHeld
//*******************************
bool hotkeySetHeld(const HotkeySet &set, const ControllerState &state) {
    for (const Hotkey &hotkey : set) {
        if (hotkeyHeld(hotkey, state)) {
            return true;
        }
    }
    return false;
}

//*******************************
// ExitHold::update
//*******************************
ExitHold::Action ExitHold::update(bool held, int64_t nowMs) {
    if (terminated_) {
        // the program has been asked; the hold no longer matters, only whether it went
        if (!killed_ && nowMs - terminatedAt_ >= ExitGraceMs) {
            killed_ = true;
            return Action::Kill;
        }
        return Action::None;
    }
    if (!held) {
        holding_ = false;
        return Action::None;
    }
    if (!holding_) {
        holding_ = true;
        since_ = nowMs;
    }
    if (nowMs - since_ >= ExitHoldMs) {
        terminated_ = true;
        terminatedAt_ = nowMs;
        return Action::Terminate;
    }
    return Action::None;
}

} // namespace abpad
