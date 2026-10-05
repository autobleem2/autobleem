//
// ValueHold: a held Left/Right taking its value step again and again, for the editors whose loop sleeps between
// presses (GuiEditorRA, GuiAppSettings, GuiRaCores) - the DpadHold of Up/Down, for the value. The same HoldRepeat pace
// as the PS1 game editor's values and the Options rows (HoldRepeat::rows()).
//   on Dpad events:  if (const int dir = valueHold.press(input, now)) { change the value by dir; }  (a press: wraps)
//   once a pass:     valueHold.tick(input, now, [&](int dir, bool repeat) { change the value by dir; });
// The press wraps past the last value to the first (and back), a repeat stops at the end - the one rule of every menu
// (abgui::stepIndex, <ab_gui/hold_repeat.h>): `repeat` is true for every step tick() gives.
// While a direction is held the loop runs every pass (FrameNeed::Active); released, it goes back to Idle.
//
#pragma once
#include "gui/hold_repeat.h"

class ValueHold {
public:
    // the direction (+1 right, -1 left) that went down just now, else 0 (nothing, or the one already held)
    int press(ableem::Input &input, uint32_t now) {
        const int dir = input.dpadRight() ? 1 : (input.dpadLeft() ? -1 : 0);
        if (dir == hold_.step())
            return 0;
        if (dir == 0) {
            stop(input);
            return 0;
        }
        hold_.press(dir, now);
        input.setFrameNeed(ableem::Input::FrameNeed::Active);
        return dir;
    }

    template <class Step> void tick(ableem::Input &input, uint32_t now, Step step) {
        if (!hold_.held())
            return;
        const int dir = hold_.step();
        input.setFrameNeed(ableem::Input::FrameNeed::Active); // (a released Up/Down may have set Idle)
        if (dir > 0 ? !input.dpadRight() : !input.dpadLeft()) {
            stop(input);
            return;
        }
        for (int n = hold_.due(now); n != 0; n -= dir)
            abgui::detail::callStep(step, dir, 0);
    }

private:
    void stop(ableem::Input &input) {
        hold_.release();
        if (!input.dpadUp() && !input.dpadDown()) // Up/Down may still be held (DpadHold keeps its own need)
            input.setFrameNeed(ableem::Input::FrameNeed::Idle);
    }
    HoldRepeat hold_;
};
