//
// ValueHold: a held Left/Right taking its value step again and again, for the editors whose loop sleeps between
// presses (GuiEditorRA, GuiAppSettings, GuiRaCores) - the DpadHold of Up/Down, for the value. The same HoldRepeat pace
// as the PS1 game editor's values and the Options rows (HoldRepeat::rows()).
//   on Dpad events:  if (const int dir = valueHold.press(input, now)) { change the value by dir; }
//   once a pass:     valueHold.tick(input, now, [&](int dir) { change the value by dir; });
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
            step(dir);
    }

private:
    void stop(ableem::Input &input) {
        hold_.release();
        if (!input.dpadUp() && !input.dpadDown()) // Up/Down may still be held (DpadHold keeps its own need)
            input.setFrameNeed(ableem::Input::FrameNeed::Idle);
    }
    HoldRepeat hold_;
};
