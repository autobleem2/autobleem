#ifndef ABPAD_HOLD_H
#define ABPAD_HOLD_H

// The one way out of every game, emulator and App (the owner's rule, 2026-10-07): hold Start+Select OR hold
// Home (the Guide button) for two seconds and the program is ended - SIGTERM, then SIGKILL three seconds later if
// it is still there. A short press does nothing extra. The daemon (abpadd), the exit-only watcher beside
// RetroArch and the shim all use this one piece, so they cannot disagree; it counts time, never frames, and it
// takes the time as a parameter so the tests need no clock.

#include "core/mapping.h"

#include <cstdint>
#include <string>
#include <vector>

namespace abpad {

constexpr int64_t ExitHoldMs = 2000;  // how long the combo is held before the program is asked to end
constexpr int64_t ExitGraceMs = 3000; // how long after SIGTERM before SIGKILL

// the combos that mean "leave", as pad.ini spells them: either of them held, each a "+"-joined list
extern const char *const DefaultExitHotkeys;

using Hotkey = std::vector<Element>;     // all held together
using HotkeySet = std::vector<Hotkey>;   // any one of these

// "start+select, guide" (a comma or a bar between alternatives) -> two hotkeys. Empty text = no hotkey at all; an
// alternative whose names are all unknown is dropped.
HotkeySet parseHotkeySet(const std::string &text);
// true when any alternative has every one of its elements held on this pad
bool hotkeySetHeld(const HotkeySet &set, const ControllerState &state);

//*******************************
// ExitHold - the hold, and what to do about it
//*******************************
// Fed once per cycle with whether the combo is held on any pad and the monotonic time in milliseconds. A hold that
// reaches ExitHoldMs gives Terminate once; releasing before that starts the count over. After Terminate the
// program has ExitGraceMs to go, then Kill is given once, whether the buttons are still down or not.
class ExitHold {
public:
    enum class Action { None, Terminate, Kill };

    Action update(bool held, int64_t nowMs);

private:
    bool holding_ = false;
    int64_t since_ = 0;
    bool terminated_ = false;
    int64_t terminatedAt_ = 0;
    bool killed_ = false;
};

} // namespace abpad

#endif
